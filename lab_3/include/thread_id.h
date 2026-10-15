#pragma once

#include <sys/syscall.h>
#include <unistd.h>

static inline unsigned long current_thread_id(void)
{
    return (unsigned long)syscall(SYS_gettid);
}
