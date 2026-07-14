#ifndef WORLD_SIM_RENDER_COUNTRY_IDENTITY_BLOCK_H
#define WORLD_SIM_RENDER_COUNTRY_IDENTITY_BLOCK_H

#include <windows.h>

RECT country_identity_block_draw(HDC hdc, RECT bounds, char symbol,
                                 const char *localized_name, COLORREF color);

#endif
