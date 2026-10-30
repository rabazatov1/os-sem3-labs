#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>

#include "sum.h"

sum_status_t sum_line(const char *line, int *result)
{
    const char *p = line;
    int sum = 0;
    int count = 0;

    for (;;) {
        while (isspace((unsigned char)*p))
            p++;
        if (*p == '\0')
            break;

        char *end;
        errno = 0;
        long value = strtol(p, &end, 10);

        if (end == p)
            return SUM_INVALID;
        if (*end != '\0' && !isspace((unsigned char)*end))
            return SUM_INVALID;
        if (errno == ERANGE || value < INT_MIN || value > INT_MAX)
            return SUM_OUT_OF_RANGE;

        if ((value > 0 && sum > INT_MAX - value) || (value < 0 && sum < INT_MIN - value))
            return SUM_OVERFLOW;

        sum += (int)value;
        count++;
        p = end;
    }

    if (count == 0)
        return SUM_EMPTY;
    *result = sum;
    return SUM_OK;
}

const char *sum_status_message(sum_status_t status)
{
    switch (status) {
    case SUM_OK:           return "ok";
    case SUM_EMPTY:        return "в строке нет чисел";
    case SUM_INVALID:      return "строка содержит не целое число";
    case SUM_OUT_OF_RANGE: return "число не помещается в int";
    case SUM_OVERFLOW:     return "сумма не помещается в int";
    }
    return "неизвестная ошибка";
}
