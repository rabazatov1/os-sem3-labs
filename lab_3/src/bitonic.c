#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "bitonic.h"
#include "thread_id.h"

typedef struct {
    int *values;
    size_t count;
    size_t threads;
    pthread_barrier_t barrier;
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    int started;
    int cancelled;
} Shared;

typedef struct {
    Shared *shared;
    size_t id;
} Worker;

static void check_thread(int error, const char *operation)
{
    if (error != 0) {
        fprintf(stderr, "%s: %s\n", operation, strerror(error));
        exit(EXIT_FAILURE);
    }
}

static void compare_range(Shared *shared, size_t id, size_t k, size_t j)
{
    size_t block = shared->count / shared->threads;
    size_t extra = shared->count % shared->threads;
    size_t begin = block * id + (id < extra ? id : extra);
    size_t end = block * (id + 1) + (id + 1 < extra ? id + 1 : extra);

    for (size_t i = begin; i < end; ++i) {
        size_t partner = i ^ j;
        if (partner < i)
            continue;

        int ascending = (i & k) == 0;
        if ((ascending && shared->values[i] > shared->values[partner]) ||
            (!ascending && shared->values[i] < shared->values[partner])) {
            int temporary = shared->values[i];
            shared->values[i] = shared->values[partner];
            shared->values[partner] = temporary;
        }
    }
}

static void run_network(Shared *shared, size_t id)
{
    for (size_t k = 2; k <= shared->count; k *= 2) {
        for (size_t j = k / 2; j != 0; j /= 2) {
            compare_range(shared, id, k, j);
            if (shared->threads > 1) {
                int error = pthread_barrier_wait(&shared->barrier);
                if (error != 0 && error != PTHREAD_BARRIER_SERIAL_THREAD)
                    check_thread(error, "pthread_barrier_wait");
            }
        }
        if (k == shared->count)
            break;
    }
}

static void *worker_main(void *argument)
{
    Worker *worker = argument;
    Shared *shared = worker->shared;
    fprintf(stderr, "[pid %ld tid %lu] worker %zu\n",
            (long)getpid(), current_thread_id(), worker->id);

    check_thread(pthread_mutex_lock(&shared->mutex), "pthread_mutex_lock");
    while (!shared->started && !shared->cancelled)
        check_thread(pthread_cond_wait(&shared->condition, &shared->mutex),
                     "pthread_cond_wait");
    int cancelled = shared->cancelled;
    check_thread(pthread_mutex_unlock(&shared->mutex), "pthread_mutex_unlock");

    if (!cancelled)
        run_network(shared, worker->id);
    return NULL;
}

static void release_workers(Shared *shared, int cancel)
{
    check_thread(pthread_mutex_lock(&shared->mutex), "pthread_mutex_lock");
    shared->cancelled = cancel;
    shared->started = !cancel;
    check_thread(pthread_cond_broadcast(&shared->condition),
                 "pthread_cond_broadcast");
    check_thread(pthread_mutex_unlock(&shared->mutex), "pthread_mutex_unlock");
}

static void cleanup(Shared *shared, pthread_t *handles, Worker *workers,
                    size_t created)
{
    for (size_t i = 0; i < created; ++i)
        check_thread(pthread_join(handles[i], NULL), "pthread_join");
    free(workers);
    free(handles);
    check_thread(pthread_cond_destroy(&shared->condition), "pthread_cond_destroy");
    check_thread(pthread_mutex_destroy(&shared->mutex), "pthread_mutex_destroy");
    check_thread(pthread_barrier_destroy(&shared->barrier),
                 "pthread_barrier_destroy");
}

int bitonic_sort(int *values, size_t count, size_t thread_limit,
                 unsigned pause_seconds, size_t *threads_used, double *elapsed_ms)
{
    if (values == NULL || count == 0 || thread_limit == 0 ||
        threads_used == NULL || elapsed_ms == NULL)
        return EINVAL;

    size_t padded = 1;
    while (padded < count) {
        if (padded > SIZE_MAX / 2)
            return EOVERFLOW;
        padded *= 2;
    }
    if (padded > SIZE_MAX / sizeof(int))
        return EOVERFLOW;

    int *work = malloc(padded * sizeof *work);
    if (work == NULL)
        return ENOMEM;
    memcpy(work, values, count * sizeof *work);
    for (size_t i = count; i < padded; ++i)
        work[i] = INT_MAX;

    Shared shared = {0};
    shared.values = work;
    shared.count = padded;
    shared.threads = thread_limit < count ? thread_limit : count;
    *threads_used = shared.threads;
    if (shared.threads > UINT_MAX) {
        free(work);
        return EOVERFLOW;
    }

    int error = pthread_barrier_init(&shared.barrier, NULL,
                                     (unsigned)shared.threads);
    if (error != 0) {
        free(work);
        return error;
    }
    error = pthread_mutex_init(&shared.mutex, NULL);
    if (error != 0) {
        check_thread(pthread_barrier_destroy(&shared.barrier),
                     "pthread_barrier_destroy");
        free(work);
        return error;
    }
    error = pthread_cond_init(&shared.condition, NULL);
    if (error != 0) {
        check_thread(pthread_mutex_destroy(&shared.mutex),
                     "pthread_mutex_destroy");
        check_thread(pthread_barrier_destroy(&shared.barrier),
                     "pthread_barrier_destroy");
        free(work);
        return error;
    }

    size_t worker_count = shared.threads - 1;
    pthread_t *handles = worker_count ? calloc(worker_count, sizeof *handles) : NULL;
    Worker *workers = worker_count ? calloc(worker_count, sizeof *workers) : NULL;
    if (worker_count && (handles == NULL || workers == NULL)) {
        release_workers(&shared, 1);
        cleanup(&shared, handles, workers, 0);
        free(work);
        return ENOMEM;
    }

    size_t created = 0;
    for (; created < worker_count; ++created) {
        workers[created] = (Worker){&shared, created + 1};
        error = pthread_create(&handles[created], NULL,
                               worker_main, &workers[created]);
        if (error != 0) {
            release_workers(&shared, 1);
            cleanup(&shared, handles, workers, created);
            free(work);
            return error;
        }
    }

    if (pause_seconds != 0) {
        fprintf(stderr, "pid=%ld threads=%zu; пауза %u с для проверки потоков\n",
                (long)getpid(), shared.threads, pause_seconds);
        sleep(pause_seconds);
    }

    struct timespec start, finish;
    if (clock_gettime(CLOCK_MONOTONIC, &start) != 0) {
        error = errno;
        release_workers(&shared, 1);
        cleanup(&shared, handles, workers, created);
        free(work);
        return error;
    }

    release_workers(&shared, 0);
    run_network(&shared, 0);
    if (clock_gettime(CLOCK_MONOTONIC, &finish) != 0) {
        error = errno;
        cleanup(&shared, handles, workers, created);
        free(work);
        return error;
    }

    cleanup(&shared, handles, workers, created);
    memcpy(values, work, count * sizeof *values);
    *elapsed_ms = (finish.tv_sec - start.tv_sec) * 1000.0 +
                  (finish.tv_nsec - start.tv_nsec) / 1000000.0;
    free(work);
    return 0;
}
