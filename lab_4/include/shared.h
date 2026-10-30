#pragma once

#include <stdatomic.h>
#include <stddef.h>

enum shared_state {
    STATE_STOP,
    STATE_REQUEST,
    STATE_OK,
    STATE_INVALID,
    STATE_IO_ERROR
};

typedef struct {
    size_t length;
    _Atomic int state;
    char line[];
} shared_data;
