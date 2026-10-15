#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bitonic.h"
#include "thread_id.h"

static void usage(const char *program)
{
    fprintf(stderr, "Использование: %s --threads N [--pause СЕКУНДЫ]\n"
                    "Ввод: количество элементов, затем целые числа через пробел.\n",
            program);
}

static int parse_size(const char *text, size_t *value)
{
    if (text[0] < '0' || text[0] > '9')
        return 0;
    errno = 0;
    char *end;
    uintmax_t number = strtoumax(text, &end, 10);
    if (errno != 0 || *end != '\0' || number > SIZE_MAX)
        return 0;
    *value = (size_t)number;
    return 1;
}

static int read_token(char token[64])
{
    return scanf("%63s", token) == 1;
}

int main(int argc, char **argv)
{
    size_t thread_limit = 0, pause_value = 0;
    for (int i = 1; i < argc; i += 2) {
        if (i + 1 >= argc) {
            usage(argv[0]);
            return EXIT_FAILURE;
        }
        if (strcmp(argv[i], "--threads") == 0) {
            if (!parse_size(argv[i + 1], &thread_limit) || thread_limit == 0) {
                usage(argv[0]);
                return EXIT_FAILURE;
            }
        } else if (strcmp(argv[i], "--pause") == 0) {
            if (!parse_size(argv[i + 1], &pause_value) || pause_value > UINT_MAX) {
                usage(argv[0]);
                return EXIT_FAILURE;
            }
        } else {
            usage(argv[0]);
            return EXIT_FAILURE;
        }
    }
    if (thread_limit == 0) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    char token[64];
    size_t count;
    if (!read_token(token) || !parse_size(token, &count) ||
        count == 0 || count > SIZE_MAX / sizeof(int)) {
        fprintf(stderr, "Некорректный размер массива\n");
        return EXIT_FAILURE;
    }

    int *values = malloc(count * sizeof *values);
    if (values == NULL) {
        perror("malloc");
        return EXIT_FAILURE;
    }

    for (size_t i = 0; i < count; ++i) {
        if (!read_token(token)) {
            fprintf(stderr, "Недостаточно чисел во входе\n");
            free(values);
            return EXIT_FAILURE;
        }
        errno = 0;
        char *end;
        long number = strtol(token, &end, 10);
        if (errno != 0 || *end != '\0' || number < INT_MIN || number > INT_MAX) {
            fprintf(stderr, "Некорректное целое число: %s\n", token);
            free(values);
            return EXIT_FAILURE;
        }
        values[i] = (int)number;
    }
    fprintf(stderr, "[pid %ld tid %lu] main\n",
            (long)getpid(), current_thread_id());

    size_t used = 0;
    double elapsed_ms = 0.0;
    int error = bitonic_sort(values, count, thread_limit, (unsigned)pause_value,
                             &used, &elapsed_ms);
    if (error != 0) {
        fprintf(stderr, "Сортировка: %s\n", strerror(error));
        free(values);
        return EXIT_FAILURE;
    }

    for (size_t i = 1; i < count; ++i) {
        if (values[i - 1] > values[i]) {
            fprintf(stderr, "Ошибка: массив не отсортирован\n");
            free(values);
            return EXIT_FAILURE;
        }
    }

    fprintf(stderr, "count=%zu threads_used=%zu sort_ms=%.6f sorted=yes\n",
            count, used, elapsed_ms);
    for (size_t i = 0; i < count; ++i) {
        if (printf("%d%c", values[i], i + 1 == count ? '\n' : ' ') < 0) {
            perror("Вывод результата");
            free(values);
            return EXIT_FAILURE;
        }
    }
    if (fflush(stdout) != 0) {
        perror("Вывод результата");
        free(values);
        return EXIT_FAILURE;
    }

    free(values);
    return EXIT_SUCCESS;
}
