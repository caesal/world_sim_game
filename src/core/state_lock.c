#include "state_lock.h"

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static SRWLOCK state_lock = SRWLOCK_INIT;

void state_read_lock(void) {
    AcquireSRWLockShared(&state_lock);
}

int state_try_read_lock(void) {
    return TryAcquireSRWLockShared(&state_lock) != 0;
}

void state_read_unlock(void) {
    ReleaseSRWLockShared(&state_lock);
}

void state_write_lock(void) {
    AcquireSRWLockExclusive(&state_lock);
}

void state_write_unlock(void) {
    ReleaseSRWLockExclusive(&state_lock);
}

#else

#include <pthread.h>

static pthread_rwlock_t state_lock = PTHREAD_RWLOCK_INITIALIZER;

void state_read_lock(void) {
    pthread_rwlock_rdlock(&state_lock);
}

int state_try_read_lock(void) {
    return pthread_rwlock_tryrdlock(&state_lock) == 0;
}

void state_read_unlock(void) {
    pthread_rwlock_unlock(&state_lock);
}

void state_write_lock(void) {
    pthread_rwlock_wrlock(&state_lock);
}

void state_write_unlock(void) {
    pthread_rwlock_unlock(&state_lock);
}

#endif
