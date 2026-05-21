#ifndef WORLD_SIM_PLATFORM_SDL_WORLDGEN_FORM_H
#define WORLD_SIM_PLATFORM_SDL_WORLDGEN_FORM_H

#include "core/render_snapshot.h"
#include "platform/sdl_text.h"
#include "ui/ui_worldgen_layout.h"

#include <SDL3/SDL.h>

void sdl_worldgen_form_init(void);
void sdl_worldgen_form_sync_selected(const RenderSnapshot *snapshot);
void sdl_worldgen_form_draw(SDL_Renderer *renderer, SdlText *text,
                            const WorldgenLayout *layout);
void sdl_worldgen_form_draw_overlay(SDL_Renderer *renderer, SdlText *text,
                                    const WorldgenLayout *layout);
int sdl_worldgen_form_click(const WorldgenLayout *layout, int x, int y);
int sdl_worldgen_form_text_input(const char *value);
int sdl_worldgen_form_key(SDL_Keycode key);
int sdl_worldgen_form_active(void);
int sdl_worldgen_form_active_rect(const WorldgenLayout *layout, RECT *out);
void sdl_worldgen_form_deactivate(void);
void sdl_worldgen_form_randomize(void);
void sdl_worldgen_form_apply_initial_count(void);

#endif
