#include "state_lock.h"

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
