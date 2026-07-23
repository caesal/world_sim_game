#include "game/game_presentation_worldgen_controls_artifact_internal.h"

#include "core/game_types.h"
#include "render/worldgen_ui_assets.h"
#include "ui/ui_types.h"
#include "ui/ui_worldgen_control_state.h"
#include "ui/ui_worldgen_view.h"

static void prepare_climate_asset_view(void) {
    UiWorldgenPanelLayout layout;
    RECT client = {0, 0, 460, WORLDGEN_CONTROLS_ARTIFACT_HEIGHT};
    int tab;
    ui_worldgen_control_state_apply_balanced();
    ui_worldgen_control_state_mark_applied();
    ui_worldgen_control_state_clear_interaction();
    for (tab = 0; tab < UI_WORLDGEN_TAB_COUNT; tab++) {
        ui_worldgen_control_state_set_scroll((UiWorldgenControlTab)tab, 0);
    }
    ui_worldgen_control_state_set_tab(UI_WORLDGEN_TAB_CLIMATE);
    ui_worldgen_view_build(client, 460, &layout);
    ui_worldgen_control_state_set_scroll(
        UI_WORLDGEN_TAB_CLIMATE,
        layout.tab_max_scroll[UI_WORLDGEN_TAB_CLIMATE]);
    hover_x = -1;
    hover_y = -1;
}

static int all_assets_diagnostic_ok(void) {
    int asset;
    for (asset = 0; asset < WORLDGEN_UI_ASSET_COUNT; asset++) {
        WorldgenUiAssetDiagnostics diagnostics;
        const WorldgenUiAssetInfo *info = worldgen_ui_assets_info(
            (WorldgenUiAssetId)asset);
        if (!info || !worldgen_ui_assets_get_diagnostics(
                (WorldgenUiAssetId)asset, &diagnostics) ||
            diagnostics.attempts != 1 || diagnostics.decodes != 1 ||
            !diagnostics.loaded || diagnostics.width != info->expected_width ||
            diagnostics.height != info->expected_height) return 0;
    }
    return 1;
}

int worldgen_controls_artifact_assets(
    WorldgenControlsArtifactWriter *writer) {
    WorldgenUiAssetDiagnostics loaded = {0};
    WorldgenUiAssetDiagnostics missing = {0};
    uint64_t loaded_hash;
    int preload_ok;
    int loaded_ok;
    int loaded_render_ok;
    int missing_render_ok;
    int missing_ok;
    int ok;
    if (!writer || !writer->manifest) return 0;

    worldgen_ui_assets_reset_for_tests();
    preload_ok = worldgen_ui_assets_preload_all();
    loaded_ok = preload_ok && all_assets_diagnostic_ok();
    prepare_climate_asset_view();
    loaded_render_ok = worldgen_controls_artifact_render(
        writer, "worldgen_assets_all_loaded.bmp", 460, UI_LANG_EN);
    loaded_hash = writer->last_hash;
    loaded_ok &= worldgen_ui_assets_get_diagnostics(
                     WORLDGEN_UI_ASSET_CLIMATE_BIOME_ENVELOPE, &loaded) &&
                 loaded.draw_calls >= 1 && loaded.fallback_draws == 0 &&
                 loaded.loaded;
    fprintf(writer->manifest,
            "fixture=all_assets_loaded ok=%d assets=%d preload=%d draws=%u fallbacks=%u hash=%016llx\n",
            loaded_ok && loaded_render_ok, WORLDGEN_UI_ASSET_COUNT,
            preload_ok, loaded.draw_calls, loaded.fallback_draws,
            (unsigned long long)loaded_hash);
    if (!loaded_ok) writer->failure_count++;

    worldgen_ui_assets_reset_for_tests();
    worldgen_ui_assets_validation_force_missing(
        WORLDGEN_UI_ASSET_CLIMATE_BIOME_ENVELOPE, 1);
    prepare_climate_asset_view();
    missing_render_ok = worldgen_controls_artifact_render(
        writer, "worldgen_assets_climate_missing_fallback.bmp", 460,
        UI_LANG_EN);
    missing_ok = worldgen_ui_assets_get_diagnostics(
                     WORLDGEN_UI_ASSET_CLIMATE_BIOME_ENVELOPE, &missing) &&
                 missing.attempts == 1 && missing.decodes == 0 &&
                 missing.draw_calls >= 1 && missing.fallback_draws >= 1 &&
                 !missing.loaded && writer->last_hash != loaded_hash;
    fprintf(
        writer->manifest,
        "fixture=neutral_missing_asset_fallback ok=%d asset=climate attempts=%u decodes=%u draws=%u fallbacks=%u loaded=%d hash=%016llx differs_from_loaded=%d\n",
        missing_ok && missing_render_ok, missing.attempts, missing.decodes,
        missing.draw_calls, missing.fallback_draws, missing.loaded,
        (unsigned long long)writer->last_hash,
        writer->last_hash != loaded_hash);
    if (!missing_ok) writer->failure_count++;
    ok = loaded_ok && loaded_render_ok && missing_ok && missing_render_ok;
    worldgen_ui_assets_validation_force_missing(
        WORLDGEN_UI_ASSET_CLIMATE_BIOME_ENVELOPE, 0);
    worldgen_ui_assets_reset_for_tests();
    return ok;
}
