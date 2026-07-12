#ifndef WORLD_SIM_WORLD_ANNOUNCEMENT_QUEUE_H
#define WORLD_SIM_WORLD_ANNOUNCEMENT_QUEUE_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "core/render_snapshot.h"

void world_announcement_queue_reset(void);
void world_announcement_queue_consume(const RenderSnapshot *snapshot);
int world_announcement_queue_tick(DWORD now);
int world_announcement_queue_active(void);
const WorldAnnouncementEvent *world_announcement_queue_current(void);
int world_announcement_queue_pending_count(void);
int world_announcement_queue_current_page(void);
int world_announcement_queue_page_count(void);
void world_announcement_queue_set_page_count(int count);
void world_announcement_queue_mark_page_visible(void);
void world_announcement_queue_previous_page(void);
void world_announcement_queue_next_page(void);
void world_announcement_queue_dismiss(void);
void world_announcement_queue_set_hovered(int hovered);
int world_announcement_queue_progress_permille(void);
int world_announcement_queue_duration_ms(int priority);

#endif
