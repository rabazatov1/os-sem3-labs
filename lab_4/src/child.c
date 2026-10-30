#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "shared.h"
#include "sum.h"

static int write_all(int fd, const char *text, size_t length)
{
    while (length > 0) {
        ssize_t written = write(fd, text, length);
        if (written == -1) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        text += written;
        length -= (size_t)written;
    }
    return 0;
}

static int parse_number(const char *text, long *number)
{
    char *end;
    errno = 0;
    long value = strtol(text, &end, 10);
    if (errno != 0 || *text == '\0' || *end != '\0' || value < 0)
        return -1;
    *number = value;
    return 0;
}

int main(int argc, char **argv)
{
    long shared_fd, parent_number;
    if (argc != 4 || parse_number(argv[2], &shared_fd) == -1 ||
        parse_number(argv[3], &parent_number) == -1 ||
        shared_fd > INT_MAX || parent_number == 0 || parent_number > INT_MAX) {
        fprintf(stderr, "Использование: child <файл> <общий fd> <parent PID>\n");
        return EXIT_FAILURE;
    }
    int fd = (int)shared_fd;
    pid_t parent = (pid_t)parent_number;
    sigset_t requests;
    if (sigemptyset(&requests) == -1 ||
        sigaddset(&requests, SIGUSR1) == -1 ||
        sigprocmask(SIG_BLOCK, &requests, NULL) == -1) {
        perror("sigprocmask");
        return EXIT_FAILURE;
    }

    int output = open(argv[1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (output == -1) {
        perror("open");
        return EXIT_FAILURE;
    }
    fprintf(stderr, "[child %ld] parent PID %ld, файл %s\n",
            (long)getpid(), (long)parent, argv[1]);
    int result = EXIT_SUCCESS;
    if (kill(parent, SIGUSR2) == -1) {
        perror("kill(ready)");
        result = EXIT_FAILURE;
        goto cleanup;
    }

    for (;;) {
        int signal_number;
        int error = sigwait(&requests, &signal_number);
        if (error != 0) {
            fprintf(stderr, "sigwait: %s\n", strerror(error));
            result = EXIT_FAILURE;
            break;
        }
        struct stat info;
        if (fstat(fd, &info) == -1) {
            perror("fstat");
            result = EXIT_FAILURE;
            break;
        }
        if (info.st_size < (off_t)sizeof(shared_data) + 1 ||
            (uintmax_t)info.st_size > SIZE_MAX) {
            fprintf(stderr, "Некорректный размер общего файла\n");
            result = EXIT_FAILURE;
            break;
        }
        size_t bytes = (size_t)info.st_size;
        shared_data *data = mmap(NULL, bytes, PROT_READ | PROT_WRITE,
                                 MAP_SHARED, fd, 0);
        if (data == MAP_FAILED) {
            perror("mmap");
            result = EXIT_FAILURE;
            break;
        }
        int state = atomic_load_explicit(&data->state, memory_order_acquire);
        if (state == STATE_STOP) {
            if (munmap(data, bytes) == -1) {
                perror("munmap");
                result = EXIT_FAILURE;
            }
            break;
        }
        if (state != STATE_REQUEST ||
            data->length > bytes - sizeof(shared_data) - 1 ||
            data->line[data->length] != '\0') {
            fprintf(stderr, "Некорректные данные в отображении\n");
            atomic_store_explicit(&data->state, STATE_IO_ERROR,
                                  memory_order_release);
            result = EXIT_FAILURE;
        } else {
            int sum;
            sum_status_t status = sum_line(data->line, &sum);
            if (status != SUM_OK) {
                fprintf(stderr, "[child %ld] %s\n", (long)getpid(),
                        sum_status_message(status));
                atomic_store_explicit(&data->state, STATE_INVALID,
                                      memory_order_release);
            } else {
                char text[32];
                int length = snprintf(text, sizeof text, "%d\n", sum);
                if (length < 0 || (size_t)length >= sizeof text ||
                    write_all(output, text, (size_t)length) == -1) {
                    perror("write(file)");
                    atomic_store_explicit(&data->state, STATE_IO_ERROR,
                                          memory_order_release);
                    result = EXIT_FAILURE;
                } else {
                    atomic_store_explicit(&data->state, STATE_OK,
                                          memory_order_release);
                }
            }
        }
        if (munmap(data, bytes) == -1) {
            perror("munmap");
            result = EXIT_FAILURE;
            break;
        }
        if (kill(parent, SIGUSR2) == -1) {
            perror("kill(reply)");
            result = EXIT_FAILURE;
            break;
        }
        if (result != EXIT_SUCCESS)
            break;
    }

cleanup:
    if (close(output) == -1) {
        perror("close(output)");
        result = EXIT_FAILURE;
    }
    if (close(fd) == -1) {
        perror("close(shared)");
        result = EXIT_FAILURE;
    }
    return result;
}
