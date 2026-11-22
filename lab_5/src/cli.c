#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "cli.h"

static void skip_space(char **cursor)
{
    while (isspace((unsigned char)**cursor))
        ++*cursor;
}

static int read_float(char **cursor, float *value)
{
    skip_space(cursor);
    errno = 0;
    char *end;
    float number = strtof(*cursor, &end);
    if (end == *cursor || errno == ERANGE || !isfinite(number) ||
        (*end != '\0' && !isspace((unsigned char)*end)))
        return -1;
    *value = number;
    *cursor = end;
    return 0;
}

static int parse_command(char *line, int *command, float args[3])
{
    char *cursor = line;
    skip_space(&cursor);
    if (*cursor == '\0')
        return 0;

    errno = 0;
    char *end;
    long number = strtol(cursor, &end, 10);
    if (end == cursor || errno != 0 ||
        (*end != '\0' && !isspace((unsigned char)*end)) ||
        (number != 0 && number != 1 && number != 2))
        return -1;
    cursor = end;

    int count = number == 1 ? 3 : number == 2 ? 2 : 0;
    for (int i = 0; i < count; ++i) {
        if (read_float(&cursor, &args[i]) != 0)
            return -1;
    }
    skip_space(&cursor);
    if (*cursor != '\0' || (number == 1 && args[2] <= 0.0f) ||
        (number == 2 && args[1] == 0.0f))
        return -1;

    *command = (int)number;
    return 1;
}

int run_cli(math_api *api, switch_fn switch_implementation, void *context)
{
    char *line = NULL;
    size_t capacity = 0;
    int status = 0;

    while (getline(&line, &capacity, stdin) != -1) {
        int command;
        float args[3];
        int parsed = parse_command(line, &command, args);
        if (parsed == 0)
            continue;
        if (parsed < 0) {
            fprintf(stderr, "Неверная команда или аргументы\n");
            continue;
        }
        if (command == 0) {
            if (switch_implementation == NULL)
                fprintf(stderr, "Переключение доступно только в программе 2\n");
            else if (switch_implementation(api, context) == 0)
                printf("Реализация %d\n", api->implementation);
            continue;
        }

        float result = command == 1
            ? api->integral(args[0], args[1], args[2])
            : api->derivative(args[0], args[1]);
        if (!isfinite(result)) {
            fprintf(stderr, "Ошибка вычисления\n");
            continue;
        }
        printf("%.9g\n", result);
    }
    if (ferror(stdin)) {
        perror("stdin");
        status = 1;
    }
    free(line);
    return status;
}
