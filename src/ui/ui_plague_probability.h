#ifndef WORLD_SIM_UI_PLAGUE_PROBABILITY_H
#define WORLD_SIM_UI_PLAGUE_PROBABILITY_H

#include "sim/plague_types.h"

#include <windows.h>

typedef struct {
    RECT label;
    RECT value;
    RECT track;
    RECT hit;
} UiPlagueProbabilityControlLayout;

typedef struct {
    RECT bounds;
    RECT title;
    UiPlagueProbabilityControlLayout controls[PLAGUE_PROBABILITY_COUNT];
    RECT total;
    RECT apply_button;
    RECT reset_button;
    RECT status;
    int bottom;
} UiPlagueProbabilityLayout;

void ui_plague_probability_layout_build(RECT client, int panel_width, int top,
                                        UiPlagueProbabilityLayout *layout);
void ui_plague_probability_sync(const PlagueStateView *view);
void ui_plague_probability_get_draft(PlagueProbabilityDistribution *out);
int ui_plague_probability_adjust_draft(PlagueProbabilityBucket bucket,
                                       int requested_value);
int ui_plague_probability_reset_draft(void);
int ui_plague_probability_apply_enabled(void);
int ui_plague_probability_reset_enabled(void);
int ui_plague_probability_pressed(int hit);
unsigned int ui_plague_probability_cache_revision(void);
int ui_plague_probability_hit_test(const UiPlagueProbabilityLayout *layout,
                                   int x, int y);
int ui_plague_probability_mouse_down(HWND hwnd,
    const UiPlagueProbabilityLayout *layout, int x, int y);
int ui_plague_probability_mouse_move(HWND hwnd,
    const UiPlagueProbabilityLayout *layout, int x, int y);
int ui_plague_probability_mouse_up(HWND hwnd,
    const UiPlagueProbabilityLayout *layout, int x, int y);
void ui_plague_probability_panel_closed(void);
void ui_plague_probability_reset_presentation_state(void);

#endif
