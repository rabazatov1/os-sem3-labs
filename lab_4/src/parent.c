#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "shared.h"

static int wait_reply(const sigset_t *signals)
{
    int number;
    int error = sigwait(signals, &number);
    if (error != 0) {
        fprintf(stderr, "sigwait: %s\n", strerror(error));
        return -1;
    }
    if (number == SIGCHLD) {
        fprintf(stderr, "Дочерний процесс завершился до ответа\n");
        return -1;
    }
    return 0;
}

static int send_line(int fd, const char *line, size_t length, pid_t child,
                     shared_data *header, const sigset_t *signals)
{
    if (length > SIZE_MAX - sizeof(shared_data) - 1 ||
        length > (size_t)LLONG_MAX - sizeof(shared_data) - 1) {
        fprintf(stderr, "Строка слишком длинная\n");
        return -1;
    }
    size_t bytes = sizeof(shared_data) + length + 1;
    if (ftruncate(fd, (off_t)bytes) == -1) {
        perror("ftruncate");
        return -1;
    }
    shared_data *data = mmap(NULL, bytes, PROT_READ | PROT_WRITE,
                             MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        perror("mmap");
        return -1;
    }
    data->length = length;
    memcpy(data->line, line, length);
    data->line[length] = '\0';
    atomic_store_explicit(&data->state, STATE_REQUEST, memory_order_release);
    if (munmap(data, bytes) == -1) {
        perror("munmap");
        return -1;
    }
    if (kill(child, SIGUSR1) == -1) {
        perror("kill(SIGUSR1)");
        return -1;
    }
    if (wait_reply(signals) == -1)
        return -1;

    int state = atomic_load_explicit(&header->state, memory_order_acquire);
    if (state == STATE_OK)
        puts("Сумма записана в файл");
    else if (state == STATE_INVALID)
        puts("Некорректная строка");
    else {
        fprintf(stderr, "Ошибка дочернего процесса\n");
        return -1;
    }
    return 0;
}

int main(void)
{
    char *line = NULL;
    size_t capacity = 0;
    int fd = -1, linked = 0;
    shared_data *header = MAP_FAILED;
    pid_t child = -1;
    int result = EXIT_FAILURE;
    char template[] = "/tmp/lab4-XXXXXX";
    sigset_t blocked, replies;

    printf("[parent %ld] Имя файла результатов: ", (long)getpid());
    fflush(stdout);
    if (getline(&line, &capacity, stdin) == -1) {
        fprintf(stderr, "Не удалось прочитать имя файла\n");
        goto cleanup;
    }
    line[strcspn(line, "\r\n")] = '\0';
    if (line[0] == '\0') {
        fprintf(stderr, "Имя файла не может быть пустым\n");
        goto cleanup;
    }

    if (sigemptyset(&blocked) == -1 ||
        sigaddset(&blocked, SIGUSR1) == -1 ||
        sigaddset(&blocked, SIGUSR2) == -1 ||
        sigaddset(&blocked, SIGCHLD) == -1 ||
        sigprocmask(SIG_BLOCK, &blocked, NULL) == -1 ||
        sigemptyset(&replies) == -1 ||
        sigaddset(&replies, SIGUSR2) == -1 ||
        sigaddset(&replies, SIGCHLD) == -1) {
        perror("sigprocmask/sigset");
        goto cleanup;
    }

    fd = mkstemp(template);
    if (fd == -1) {
        perror("mkstemp");
        goto cleanup;
    }
    linked = 1;
    if (unlink(template) == -1) {
        perror("unlink");
        goto cleanup;
    }
    linked = 0;
    int flags = fcntl(fd, F_GETFD);
    if (flags == -1 || fcntl(fd, F_SETFD, flags & ~FD_CLOEXEC) == -1) {
        perror("fcntl");
        goto cleanup;
    }
    if (ftruncate(fd, (off_t)sizeof(shared_data) + 1) == -1) {
        perror("ftruncate");
        goto cleanup;
    }
    header = mmap(NULL, sizeof(shared_data), PROT_READ | PROT_WRITE,
                  MAP_SHARED, fd, 0);
    if (header == MAP_FAILED) {
        perror("mmap");
        goto cleanup;
    }
    header->length = 0;
    atomic_init(&header->state, STATE_STOP);

    char fd_text[32], parent_text[32];
    snprintf(fd_text, sizeof fd_text, "%d", fd);
    snprintf(parent_text, sizeof parent_text, "%ld", (long)getpid());
    fflush(stdout);
    child = fork();
    if (child == -1) {
        perror("fork");
        goto cleanup;
    }
    if (child == 0) {
        execl("./build/child", "child", line, fd_text, parent_text,
              (char *)NULL);
        perror("execl");
        _exit(127);
    }
    fprintf(stderr, "[parent %ld] child PID %ld\n", (long)getpid(), (long)child);
    if (wait_reply(&replies) == -1)
        goto cleanup;

    for (;;) {
        printf("> ");
        fflush(stdout);
        ssize_t length = getline(&line, &capacity, stdin);
        if (length == -1) {
            if (ferror(stdin))
                perror("getline");
            else
                result = EXIT_SUCCESS;
            break;
        }
        if (send_line(fd, line, (size_t)length, child, header, &replies) == -1)
            break;
    }

cleanup:
    if (child > 0) {
        if (header != MAP_FAILED)
            atomic_store_explicit(&header->state, STATE_STOP,
                                  memory_order_release);
        if (kill(child, SIGUSR1) == -1 && errno != ESRCH)
            perror("kill(stop)");
        int status;
        pid_t waited;
        do {
            waited = waitpid(child, &status, 0);
        } while (waited == -1 && errno == EINTR);
        if (waited == -1) {
            perror("waitpid");
            result = EXIT_FAILURE;
        } else if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            fprintf(stderr, "[parent %ld] child %ld завершился с ошибкой\n",
                    (long)getpid(), (long)child);
            result = EXIT_FAILURE;
        } else {
            fprintf(stderr, "[parent %ld] child %ld завершился успешно\n",
                    (long)getpid(), (long)child);
        }
    }
    if (header != MAP_FAILED && munmap(header, sizeof(shared_data)) == -1) {
        perror("munmap");
        result = EXIT_FAILURE;
    }
    if (fd != -1 && close(fd) == -1) {
        perror("close");
        result = EXIT_FAILURE;
    }
    if (linked && unlink(template) == -1) {
        perror("unlink");
        result = EXIT_FAILURE;
    }
    free(line);
    return result;
}
