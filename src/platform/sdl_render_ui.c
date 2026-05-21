#include "platform/sdl_render_ui.h"

#include "core/game_types.h"
#include "platform/sdl_gdi_compat.h"
#include "platform/sdl_worldgen_form.h"
#include "render/pause_menu_render.h"
#include "render/render_panel_internal.h"
#include "ui/color_picker.h"
#include "ui/ui_worldgen_layout.h"

void sdl_render_draw_ui(SDL_Renderer *renderer, SdlText *text,
                        const RenderSnapshot *snapshot, RECT client) {
    HDC hdc;
    hdc = sdl_gdi_begin(renderer, text, client);
    if (!hdc) return;
    draw_top_bar(hdc, client);
    draw_bottom_bar(hdc, client);
    draw_side_panel(hdc, client);
    draw_map_legend(hdc, client);
    if (pause_menu_open) draw_pause_menu_overlay(hdc, client);
    color_picker_draw(hdc, client);
    sdl_gdi_end(hdc);
    if (!pause_menu_open && !side_panel_collapsed && panel_tab == PANEL_WORLD) {
        WorldgenLayout layout;
        int scroll = worldgen_layout_clamp_scroll(client, side_panel_w, worldgen_scroll_offset);
        worldgen_layout_build(client, side_panel_w, scroll, &layout);
        sdl_worldgen_form_sync_selected(snapshot);
        sdl_worldgen_form_draw_overlay(renderer, text, &layout);
    }
}
