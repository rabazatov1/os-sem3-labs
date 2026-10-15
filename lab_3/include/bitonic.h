#pragma once

#include <stddef.h>

int bitonic_sort(int *values, size_t count, size_t thread_limit,
                 unsigned pause_seconds, size_t *threads_used, double *elapsed_ms);
