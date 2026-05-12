#ifndef WORLD_SIM_STATE_LOCK_H
#define WORLD_SIM_STATE_LOCK_H

void state_read_lock(void);
int state_try_read_lock(void);
void state_read_unlock(void);
void state_write_lock(void);
void state_write_unlock(void);

#endif
