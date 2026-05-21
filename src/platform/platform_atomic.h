#ifndef WORLD_SIM_PLATFORM_ATOMIC_H
#define WORLD_SIM_PLATFORM_ATOMIC_H

#include "platform/platform_types.h"

#ifdef _WIN32

static inline LONG platform_atomic_increment(volatile LONG *value) {
    return InterlockedIncrement(value);
}

static inline LONG platform_atomic_decrement(volatile LONG *value) {
    return InterlockedDecrement(value);
}

static inline LONG platform_atomic_add(volatile LONG *value, LONG delta) {
    return InterlockedAdd(value, delta);
}

static inline LONG platform_atomic_exchange(volatile LONG *value, LONG next) {
    return InterlockedExchange(value, next);
}

static inline LONG platform_atomic_read(volatile LONG *value) {
    return InterlockedAdd(value, 0);
}

#else

static inline LONG platform_atomic_increment(volatile LONG *value) {
    return __sync_add_and_fetch(value, 1);
}

static inline LONG platform_atomic_decrement(volatile LONG *value) {
    return __sync_sub_and_fetch(value, 1);
}

static inline LONG platform_atomic_add(volatile LONG *value, LONG delta) {
    return __sync_add_and_fetch(value, delta);
}

static inline LONG platform_atomic_exchange(volatile LONG *value, LONG next) {
    return __sync_lock_test_and_set(value, next);
}

static inline LONG platform_atomic_read(volatile LONG *value) {
    return __sync_fetch_and_add(value, 0);
}

#endif

#endif
