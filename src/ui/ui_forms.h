#ifndef WORLD_SIM_UI_FORMS_H
#define WORLD_SIM_UI_FORMS_H

#include "ui/ui_types.h"

void ui_forms_create(HWND hwnd);
void ui_forms_layout(HWND hwnd);
void ui_forms_write_civ(int civ_id);
void ui_forms_add_civ(HWND hwnd);
void ui_forms_apply_selected(HWND hwnd);
void ui_forms_read_world_setup_controls(void);

#endif
