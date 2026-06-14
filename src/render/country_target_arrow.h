#ifndef WORLD_SIM_COUNTRY_TARGET_ARROW_H
#define WORLD_SIM_COUNTRY_TARGET_ARROW_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "core/render_snapshot.h"
#include "ui/ui_types.h"

void draw_country_target_arrow(HDC hdc, RECT client, MapLayout layout,
                               const RenderSnapshot *snapshot);

#endif
