#ifndef WORLD_SIM_UI_WORLDGEN_LEGACY_FORMS_H
#define WORLD_SIM_UI_WORLDGEN_LEGACY_FORMS_H

#include "ui/ui_types.h"
#include "ui/ui_worldgen_layout.h"

typedef struct {
    const WorldgenLayout *legacy_layout;
    RECT region_custom_viewport;
    RECT region_custom_input;
    RECT hydrology_initial_viewport;
    RECT hydrology_initial_input;
    int show_legacy;
    int show_region_custom;
    int show_hydrology_initial;
} UiWorldgenLegacyFormsLayout;

void ui_worldgen_legacy_forms_create(HWND parent);
void ui_worldgen_legacy_forms_localize_name(void);
void ui_worldgen_legacy_forms_layout(const UiWorldgenLegacyFormsLayout *layout);
void ui_worldgen_legacy_forms_hide(void);

const FormControls *ui_worldgen_legacy_forms_controls(void);
int ui_worldgen_legacy_forms_is_edit_id(int control_id);
int ui_worldgen_legacy_forms_handle_change(int control_id);
int ui_worldgen_legacy_forms_custom_region_has_focus(void);
int ui_worldgen_legacy_forms_focused_numeric_id(void);

void ui_worldgen_legacy_forms_get_text_utf8(HWND control, char *buffer,
                                            int buffer_size);
void ui_worldgen_legacy_forms_set_text_utf8(HWND control, const char *text);
int ui_worldgen_legacy_forms_read_int(HWND control, int fallback,
                                      int min_value, int max_value,
                                      int normalize);
void ui_worldgen_legacy_forms_write_int(HWND control, int value);

int ui_worldgen_legacy_forms_commit_custom_region(int fallback,
                                                   int *out_value);
int ui_worldgen_legacy_forms_try_read_initial_civs(int control_id,
                                                    int *out_value);
int ui_worldgen_legacy_forms_commit_initial_civs(int control_id, int fallback,
                                                  int *out_value);
void ui_worldgen_legacy_forms_mirror_initial_civs(int source_control_id,
                                                   int value);
void ui_worldgen_legacy_forms_write_initial_civs(int value);
void ui_worldgen_legacy_forms_commit_numeric_edits(int initial_fallback,
                                                    int custom_fallback,
                                                    int *out_initial,
                                                    int *out_custom);
void ui_worldgen_legacy_forms_write_region_custom(int value);

HBRUSH ui_worldgen_legacy_forms_control_color(WPARAM wparam, LPARAM lparam);
void ui_worldgen_legacy_forms_redraw_visible(void);

#endif
