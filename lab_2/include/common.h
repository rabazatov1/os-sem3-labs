#pragma once

#include <stddef.h>

int write_all(int fd, const void *buffer, size_t length);
int close_fd(int *fd);
int ignore_sigpipe(void);
