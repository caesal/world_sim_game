#ifndef WORLD_SIM_PANEL_WORLDGEN_CONTROLS_H
#define WORLD_SIM_PANEL_WORLDGEN_CONTROLS_H

#include "ui/ui_clay_theme.h"

#include <windows.h>

typedef struct {
    int hovered;
    int pressed;
    int selected;
    int disabled;
    int focused;
} PanelWorldgenControlFlags;

typedef enum {
    PANEL_WORLDGEN_GLYPH_DICE,
    PANEL_WORLDGEN_GLYPH_RESET
} PanelWorldgenGlyph;

typedef enum {
    PANEL_WORLDGEN_HANDLE_PRIMARY,
    PANEL_WORLDGEN_HANDLE_SECONDARY
} PanelWorldgenHandleIdentity;

typedef struct {
    RECT clip;
    RECT label;
    RECT value;
    RECT track;
} PanelWorldgenSliderGeometry;

UiClayState panel_worldgen_controls_clay_state(
    PanelWorldgenControlFlags flags);
void panel_worldgen_controls_draw_focus_ring(HDC hdc, RECT rect,
                                             int radius, int focused);
void panel_worldgen_controls_draw_text_clipped(
    HDC hdc, RECT clip, RECT rect, const char *text, COLORREF color,
    unsigned int format);
void panel_worldgen_controls_draw_glyph_button(
    HDC hdc, RECT rect, PanelWorldgenGlyph glyph,
    PanelWorldgenControlFlags flags);

RECT panel_worldgen_controls_handle_rect(
    RECT logical_rect, PanelWorldgenHandleIdentity identity, int coincident);
void panel_worldgen_controls_draw_handle(
    HDC hdc, RECT logical_rect, PanelWorldgenHandleIdentity identity,
    int coincident, PanelWorldgenControlFlags flags);

void panel_worldgen_controls_draw_grid(HDC hdc, RECT plot,
                                       int columns, int rows,
                                       COLORREF color);
void panel_worldgen_controls_draw_axes(HDC hdc, RECT plot,
                                       int vertical_percent,
                                       int horizontal_percent,
                                       COLORREF color);
void panel_worldgen_controls_draw_ticks(HDC hdc, RECT plot,
                                        int vertical_percent,
                                        int horizontal_percent,
                                        int x_steps, int y_steps,
                                        int tick_radius, COLORREF color);

void panel_worldgen_controls_draw_continuous_slider(
    HDC hdc, const PanelWorldgenSliderGeometry *geometry,
    const char *label, int value, PanelWorldgenControlFlags flags);

#endif
