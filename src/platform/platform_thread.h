#ifndef WORLD_SIM_PLATFORM_THREAD_H
#define WORLD_SIM_PLATFORM_THREAD_H

#include "platform/platform_types.h"

#include <stddef.h>
#include <string.h>

#ifdef _WIN32

static inline void platform_sleep_ms(int ms) {
    Sleep((DWORD)(ms < 0 ? 0 : ms));
}

#else

#include <errno.h>
#include <sched.h>
#include <time.h>

static inline void platform_sleep_ms(int ms) {
    struct timespec req;
    if (ms <= 0) {
        sched_yield();
        return;
    }
    req.tv_sec = ms / 1000;
    req.tv_nsec = (long)(ms % 1000) * 1000000L;
    while (nanosleep(&req, &req) == -1 && errno == EINTR) {
    }
}

#endif

static inline void platform_copy_string(char *dst, const char *src, size_t dst_size) {
    if (!dst || dst_size == 0) return;
    if (!src) src = "";
    strncpy(dst, src, dst_size - 1);
    dst[dst_size - 1] = '\0';
}

#endif
