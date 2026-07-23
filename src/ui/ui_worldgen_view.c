#include "ui/ui_worldgen_view.h"

#include <string.h>

#include "ui/ui_worldgen_config_adapter.h"
#include "ui/ui_worldgen_control_state.h"
#include "ui/ui_worldgen_layout.h"

void ui_worldgen_view_build(RECT client, int panel_width,
                            UiWorldgenPanelLayout *layout) {
    const UiWorldgenControlState *state = ui_worldgen_control_state_get();
    UiWorldgenPanelLayoutInput input;
    UiWorldgenEffectiveConfig config;
    UiWorldgenFingerprint fingerprint;

    if (!layout) return;
    ui_worldgen_config_read(&config);
    ui_worldgen_config_fingerprint(&config, &fingerprint);
    memset(&input, 0, sizeof(input));
    input.client = client;
    input.panel_width = panel_width;
    input.tab = state->initialized ? state->tab : UI_WORLDGEN_TAB_PHYSICAL;
    if (state->initialized) {
        memcpy(input.scroll_offsets, state->scroll_offsets,
               sizeof(input.scroll_offsets));
        input.climate_corners = &state->climate_corners;
    }
    input.config = &config;
    input.fingerprint = &fingerprint;
    ui_worldgen_panel_layout_build(&input, layout);
    input.legacy_content_height =
        worldgen_layout_content_height(layout->legacy.viewport);
    ui_worldgen_panel_layout_build(&input, layout);
}

void ui_worldgen_view_build_legacy_layout(
    const UiWorldgenLegacyLayout *legacy, WorldgenLayout *layout) {
    if (!legacy || !layout) return;
    worldgen_layout_build_in_viewport(
        legacy->viewport, legacy->viewport.top - legacy->content_origin.y,
        layout);
}
