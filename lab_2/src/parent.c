#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "common.h"

static void start_child(const char *filename, int to_child[2], int from_child[2])
{
    if (dup2(to_child[0], STDIN_FILENO) == -1 ||
        dup2(from_child[1], STDOUT_FILENO) == -1) {
        perror("dup2");
        _exit(127);
    }

    int failed = 0;
    failed |= close_fd(&to_child[0]) == -1;
    failed |= close_fd(&to_child[1]) == -1;
    failed |= close_fd(&from_child[0]) == -1;
    failed |= close_fd(&from_child[1]) == -1;
    if (failed)
        _exit(127);

    execl("./build/child", "child", filename, (char *)NULL);
    perror("execl");
    _exit(127);
}

static int read_reply(int fd)
{
    char reply;
    ssize_t result;
    do {
        result = read(fd, &reply, 1);
    } while (result == -1 && errno == EINTR);
    if (result == 1)
        return reply;
    if (result == 0)
        fprintf(stderr, "Дочерний процесс закрыл канал ответов\n");
    else
        perror("read(pipe2)");
    return -1;
}

int main(void)
{
    char *line = NULL;
    size_t capacity = 0;
    int to_child[2] = {-1, -1};
    int from_child[2] = {-1, -1};
    pid_t pid = -1;
    int result = EXIT_FAILURE;
    int failed = 0;

    if (ignore_sigpipe() == -1) {
        perror("sigaction(SIGPIPE)");
        goto cleanup;
    }
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

    if (pipe(to_child) == -1 || pipe(from_child) == -1) {
        perror("pipe");
        goto cleanup;
    }
    fflush(stdout);
    pid = fork();
    if (pid == -1) {
        perror("fork");
        goto cleanup;
    }
    if (pid == 0)
        start_child(line, to_child, from_child);

    fprintf(stderr, "[parent %ld] child PID %ld\n", (long)getpid(), (long)pid);
    failed |= close_fd(&to_child[0]) == -1;
    failed |= close_fd(&from_child[1]) == -1;
    if (failed || read_reply(from_child[0]) != 'R')
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
        if (write_all(to_child[1], line, (size_t)length) == -1 ||
            (line[length - 1] != '\n' && write_all(to_child[1], "\n", 1) == -1)) {
            perror("write(pipe1)");
            break;
        }
        int reply = read_reply(from_child[0]);
        if (reply == 'O')
            puts("Сумма записана в файл");
        else if (reply == 'E')
            puts("Некорректная строка или ошибка записи");
        else
            break;
    }

cleanup:
    failed = 0;
    failed |= close_fd(&to_child[0]) == -1;
    failed |= close_fd(&to_child[1]) == -1;
    failed |= close_fd(&from_child[0]) == -1;
    failed |= close_fd(&from_child[1]) == -1;
    if (failed)
        result = EXIT_FAILURE;
    if (pid > 0) {
        int status;
        pid_t waited;
        do {
            waited = waitpid(pid, &status, 0);
        } while (waited == -1 && errno == EINTR);
        if (waited == -1) {
            perror("waitpid");
            result = EXIT_FAILURE;
        } else if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            fprintf(stderr, "[parent %ld] child %ld завершился с ошибкой\n",
                    (long)getpid(), (long)pid);
            result = EXIT_FAILURE;
        } else {
            fprintf(stderr, "[parent %ld] child %ld завершился успешно\n",
                    (long)getpid(), (long)pid);
        }
    }
    free(line);
    return result;
}
