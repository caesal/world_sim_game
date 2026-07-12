#ifndef WORLD_SIM_TOP_WORLD_ANNOUNCEMENT_SURFACE_H
#define WORLD_SIM_TOP_WORLD_ANNOUNCEMENT_SURFACE_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "render/icons.h"

typedef struct {
    int event_id;
    int event_type;
    int year;
    int month;
    int actor_uid;
    int target_uid;
    int alliance_a_id;
    int alliance_b_id;
    int related_count;
    int related_first_uid;
    int related_last_uid;
    int detail_key;
    int page;
    int page_count;
    int pending_count;
    int language;
    int hover_control;
} TopWorldAnnouncementSurfaceKey;

typedef struct {
    int client_width;
    int client_height;
    int side_panel_width;
    int side_panel_collapsed;
    int display_mode;
    int language;
    int map_x;
    int map_y;
    int draw_width;
    int draw_height;
    int selected_civ;
    int selected_x;
    int selected_y;
    int legend_collapsed;
    int interaction_preview;
    unsigned int snapshot_revision;
} TopWorldAnnouncementUnderlayKey;

int top_world_announcement_surface_begin(HDC target, RECT band,
                                         const TopWorldAnnouncementSurfaceKey *key,
                                         HDC *surface);
void top_world_announcement_surface_finish(void);
void top_world_announcement_surface_blit(HDC target, RECT band);
void top_world_announcement_surface_invalidate(void);
int top_world_announcement_surface_capture_underlay(
    HDC target, RECT band, const TopWorldAnnouncementUnderlayKey *key);
int top_world_announcement_surface_underlay_matches(
    RECT band, const TopWorldAnnouncementUnderlayKey *key);
int top_world_announcement_surface_restore_underlay(
    HDC target, RECT band, const TopWorldAnnouncementUnderlayKey *key);
void top_world_announcement_surface_invalidate_underlay(void);
void top_world_announcement_surface_prewarm(HDC target, RECT band);
void top_world_announcement_surface_draw_icon(HDC target, RECT band,
                                              IconId icon, COLORREF fallback);
void top_world_announcement_surface_fill_alpha(HDC target, RECT rect,
                                               COLORREF color, BYTE alpha);

#endif
