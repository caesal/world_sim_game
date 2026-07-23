#include "render_panel_internal.h"

#include "render/panel_worldgen_climate.h"
#include "render/panel_worldgen_fingerprint.h"
#include "render/panel_worldgen_hydrology.h"
#include "render/panel_worldgen_legacy.h"
#include "render/panel_worldgen_physical.h"
#include "render/panel_worldgen_shell.h"
#include "ui/ui_worldgen_config_adapter.h"
#include "ui/ui_worldgen_control_state.h"
#include "ui/ui_worldgen_view.h"

void draw_worldgen_panel(HDC hdc, RECT client, int x, HFONT title_font, HFONT body_font) {
    const UiWorldgenControlState *state = ui_worldgen_control_state_get();
    UiWorldgenEffectiveConfig config;
    UiWorldgenPanelLayout layout;
    int map_width;
    int map_height;
    (void)x;

    ui_worldgen_config_read(&config);
    ui_worldgen_view_build(client, side_panel_w, &layout);
    map_size_dimensions(config.pending_map_size, &map_width, &map_height);
    panel_worldgen_shell_draw(hdc, &layout, state, title_font, body_font);
    panel_worldgen_fingerprint_draw(hdc, &layout, body_font);
    if (state->tab == UI_WORLDGEN_TAB_PHYSICAL) {
        panel_worldgen_physical_draw(hdc, &layout, &config, state, body_font);
    } else if (state->tab == UI_WORLDGEN_TAB_CLIMATE) {
        panel_worldgen_climate_draw(hdc, &layout, &config, state, body_font);
    } else if (state->tab == UI_WORLDGEN_TAB_HYDROLOGY_REGIONS) {
        panel_worldgen_hydrology_draw(hdc, &layout, &config, state,
                                      map_width, map_height, body_font);
    } else {
        draw_worldgen_legacy_panel(hdc, &layout.legacy, client,
                                   title_font, body_font);
    }
    panel_worldgen_shell_draw_tooltip(hdc, client, state);
}
