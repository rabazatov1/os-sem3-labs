#pragma once

typedef enum {
    SUM_OK = 0,
    SUM_EMPTY,
    SUM_INVALID,
    SUM_OUT_OF_RANGE,
    SUM_OVERFLOW
} sum_status_t;

sum_status_t sum_line(const char *line, int *result);

const char *sum_status_message(sum_status_t status);

