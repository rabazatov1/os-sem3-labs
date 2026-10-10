#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "common.h"
#include "sum.h"

static int reply(char code)
{
    return write_all(STDOUT_FILENO, &code, 1);
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "Использование: child <файл результатов>\n");
        return EXIT_FAILURE;
    }
    if (ignore_sigpipe() == -1) {
        perror("sigaction(SIGPIPE)");
        return EXIT_FAILURE;
    }

    int file = open(argv[1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file == -1) {
        perror("open");
        return EXIT_FAILURE;
    }
    fprintf(stderr, "[child %ld] parent PID %ld, файл %s\n",
            (long)getpid(), (long)getppid(), argv[1]);
    if (reply('R') == -1) {
        perror("write(pipe2)");
        close_fd(&file);
        return EXIT_FAILURE;
    }

    char *line = NULL;
    size_t capacity = 0;
    int result = EXIT_SUCCESS;
    while (getline(&line, &capacity, stdin) != -1) {
        int sum;
        sum_status_t status = sum_line(line, &sum);
        if (status != SUM_OK) {
            fprintf(stderr, "[child %ld] %s\n", (long)getpid(),
                    sum_status_message(status));
            if (reply('E') == -1) {
                perror("write(pipe2)");
                result = EXIT_FAILURE;
                break;
            }
            continue;
        }

        char text[32];
        int length = snprintf(text, sizeof text, "%d\n", sum);
        if (length < 0 || (size_t)length >= sizeof text ||
            write_all(file, text, (size_t)length) == -1) {
            perror("write(file)");
            if (reply('E') == -1)
                perror("write(pipe2)");
            result = EXIT_FAILURE;
            break;
        }
        if (reply('O') == -1) {
            perror("write(pipe2)");
            result = EXIT_FAILURE;
            break;
        }
    }
    if (ferror(stdin)) {
        perror("getline(pipe1)");
        result = EXIT_FAILURE;
    }
    free(line);
    if (close_fd(&file) == -1)
        result = EXIT_FAILURE;
    return result;
}
