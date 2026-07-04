#ifndef WORLD_SIM_RENDER_OCEAN_TEXTURE_H
#define WORLD_SIM_RENDER_OCEAN_TEXTURE_H

#include "ui/ui_types.h"
#include <windows.h>

unsigned int render_ocean_texture_key(RECT client, RECT viewport);
int render_ocean_texture_ensure(HDC hdc, RECT client, MapLayout layout, RECT viewport);
int render_ocean_texture_copy(HDC hdc, RECT viewport);
int render_ocean_texture_score(void);
void render_ocean_texture_reset_debug(void);

#endif
