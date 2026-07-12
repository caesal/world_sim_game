#ifndef WORLD_SIM_TOP_WORLD_ANNOUNCEMENT_H
#define WORLD_SIM_TOP_WORLD_ANNOUNCEMENT_H

#include "core/value_types.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef struct {
    int active;
    int background_alpha;
    int country_spans;
    int alliance_spans;
    int neutral_spans;
    int dark_identity_outlines;
    int page;
    int page_count;
    int related_first;
    int related_visible;
    int header_font_px;
    int body_font_px;
    int metadata_font_px;
    int minimum_body_font_px;
    int compact_shrink_enabled;
    Color32 first_country_color;
    Color32 first_alliance_color;
} TopWorldAnnouncementProbeInfo;

int draw_top_world_announcement(HDC hdc, RECT client);
TopWorldAnnouncementProbeInfo top_world_announcement_probe_info(void);
int top_world_announcement_last_draw_ms(void);
int top_world_announcement_peak_draw_ms(void);
int top_world_announcement_peak_layout_ms(void);
int top_world_announcement_peak_content_ms(void);
int top_world_announcement_peak_composite_ms(void);
void top_world_announcement_reset_draw_metrics(void);

#endif
