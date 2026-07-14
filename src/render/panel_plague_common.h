#ifndef WORLD_SIM_PANEL_PLAGUE_COMMON_H
#define WORLD_SIM_PANEL_PLAGUE_COMMON_H

#include "sim/plague_types.h"
#include "ui/ui_widgets.h"

#include <stddef.h>
#include <stdint.h>

const char *plague_panel_size_label(PlagueSize size);
void plague_panel_format_absolute_month(int absolute_month, char *out, size_t out_size);
void plague_panel_format_duration(int months, char *out, size_t out_size);
void plague_panel_format_count64(int64_t value, char *out, size_t out_size);
void plague_panel_format_annual_mortality(int severity, char *out, size_t out_size);
void plague_panel_row(HDC hdc, UiCursor *cursor, const char *label, const char *value);
void plague_panel_pair(HDC hdc, UiCursor *cursor,
                       const char *left_label, const char *left_value,
                       const char *right_label, const char *right_value);
void plague_panel_progress(HDC hdc, UiCursor *cursor, const char *label,
                           const char *value, int amount, int maximum,
                           COLORREF color);
void plague_panel_draw_immunity(HDC hdc, UiCursor *cursor,
                                const PlagueStateView *state);
void plague_panel_draw_schedule(HDC hdc, UiCursor *cursor,
                                const PlagueStateView *state);

#endif
