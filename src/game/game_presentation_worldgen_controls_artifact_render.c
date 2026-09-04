#include "game/game_presentation_worldgen_controls_artifact_internal.h"

#include "core/game_types.h"
#include "game/game_presentation_static_physical_artifacts.h"
#include "render/render_panel_internal.h"
#include "ui/ui_forms.h"
#include "ui/ui_types.h"
#include "ui/ui_worldgen_config_adapter.h"
#include "ui/ui_worldgen_control_state.h"
#include "ui/ui_worldgen_legacy_forms.h"
#include "ui/ui_worldgen_view.h"

typedef struct {
    HWND window;
    uint32_t mask;
} WorldgenControlsNativeChild;

static const char *tab_name(UiWorldgenControlTab tab) {
    static const char *names[UI_WORLDGEN_TAB_COUNT] = {
        "physical", "climate", "hydrology_regions", "legacy"
    };
    return tab >= 0 && tab < UI_WORLDGEN_TAB_COUNT ? names[tab] : "invalid";
}

static const char *language_name(int language) {
    return language == UI_LANG_ZH ? "zh" : "en";
}

static unsigned int non_background_pixels(
    const StaticPhysicalProbeCanvas *canvas) {
    unsigned int count = 0;
    int pixel_count;
    int i;
    if (!canvas || !canvas->pixels) return 0;
    pixel_count = canvas->width * canvas->height;
    for (i = 0; i < pixel_count; i++) {
        if ((canvas->pixels[i] & UINT32_C(0x00ffffff)) != 0) count++;
    }
    return count;
}

static void clear_native_visibility_styles(void) {
    const FormControls *form = ui_worldgen_legacy_forms_controls();
    HWND windows[] = {
        form->name_edit, form->symbol_edit,
        form->military_edit, form->logistics_edit,
        form->governance_edit, form->cohesion_edit,
        form->production_edit, form->commerce_edit,
        form->innovation_edit, form->initial_civs_edit,
        form->hydrology_initial_civs_edit,
        form->region_custom_edit, form->add_button, form->apply_button
    };
    int count = (int)(sizeof(windows) / sizeof(windows[0]));
    int index;
    for (index = 0; index < count; index++) {
        if (windows[index] && IsWindow(windows[index])) {
            ShowWindow(windows[index], SW_HIDE);
        }
    }
}

static void layout_native_children(RECT client, int width) {
    const UiWorldgenControlState *state = ui_worldgen_control_state_get();
    UiWorldgenPanelLayout panel;
    WorldgenLayout legacy;
    UiWorldgenLegacyFormsLayout native;
    ui_forms_write_world_setup_controls();
    clear_native_visibility_styles();
    ui_worldgen_view_build(client, width, &panel);
    ui_worldgen_view_build_legacy_layout(&panel.legacy, &legacy);
    native.legacy_layout = &legacy;
    native.region_custom_viewport = panel.content_viewport;
    native.region_custom_input = panel.hydrology.region_custom_input;
    native.hydrology_initial_viewport = panel.content_viewport;
    native.hydrology_initial_input = panel.hydrology.initial_civs_input;
    native.show_legacy = state->tab == UI_WORLDGEN_TAB_LEGACY;
    native.show_region_custom =
        state->tab == UI_WORLDGEN_TAB_HYDROLOGY_REGIONS &&
        state->region_category == UI_WORLDGEN_REGION_CUSTOM;
    native.show_hydrology_initial =
        state->tab == UI_WORLDGEN_TAB_HYDROLOGY_REGIONS;
    ui_worldgen_legacy_forms_layout(&native);
}

static int native_child_logically_visible(HWND window) {
    return window && IsWindow(window) &&
           (GetWindowLongPtrW(window, GWL_STYLE) & WS_VISIBLE) != 0;
}

static int native_edit_text_signal(
    const StaticPhysicalProbeCanvas *canvas, RECT rect,
    unsigned int *out_pixels) {
    unsigned int pixels = 0;
    int left = rect.left + 2;
    int top = rect.top + 2;
    int right = rect.right - 2;
    int bottom = rect.bottom - 2;
    int area = (right - left) * (bottom - top);
    int x;
    int y;
    if (!canvas || !canvas->pixels || area <= 0) return 0;
    for (y = top; y < bottom; y++) {
        for (x = left; x < right; x++) {
            uint32_t color = canvas->pixels[y * canvas->width + x];
            unsigned int blue = color & UINT32_C(0xff);
            unsigned int green = (color >> 8) & UINT32_C(0xff);
            unsigned int red = (color >> 16) & UINT32_C(0xff);
            pixels += red >= 220 && green >= 220 && blue >= 220;
        }
    }
    if (out_pixels) *out_pixels = pixels;
    return pixels > 0 && pixels * 3 < (unsigned int)area;
}

static int print_native_child(
    WorldgenControlsArtifactWriter *writer,
    StaticPhysicalProbeCanvas *canvas, HWND window, int expect_text,
    unsigned int *out_text_pixels, int *out_text_rendered) {
    DWORD_PTR message_result = 0;
    RECT rect;
    int saved;
    int delivered;
    int text_rendered = 0;
    unsigned int text_pixels = 0;
    if (out_text_pixels) *out_text_pixels = 0;
    if (out_text_rendered) *out_text_rendered = 0;
    if (!writer || !canvas || !window ||
        !GetWindowRect(window, &rect)) return 0;
    MapWindowPoints(HWND_DESKTOP, writer->owner, (POINT *)&rect, 2);
    if (rect.right <= rect.left || rect.bottom <= rect.top ||
        rect.left < 0 || rect.top < 0 || rect.right > canvas->width ||
        rect.bottom > canvas->height) return 0;
    saved = SaveDC(canvas->dc);
    if (!saved) return 0;
    SetViewportOrgEx(canvas->dc, rect.left, rect.top, NULL);
    IntersectClipRect(canvas->dc, 0, 0,
                      rect.right - rect.left, rect.bottom - rect.top);
    delivered = SendMessageTimeoutW(
        window, WM_PRINT, (WPARAM)canvas->dc,
        PRF_CLIENT | PRF_NONCLIENT | PRF_ERASEBKGND | PRF_CHILDREN,
        SMTO_ABORTIFHUNG | SMTO_BLOCK, 250, &message_result) != 0;
    if (expect_text) {
        GdiFlush();
        text_rendered = native_edit_text_signal(
            canvas, rect, &text_pixels);
        if (!text_rendered) {
            delivered = SendMessageTimeoutW(
                window, WM_PRINTCLIENT, (WPARAM)canvas->dc,
                PRF_CLIENT | PRF_ERASEBKGND,
                SMTO_ABORTIFHUNG | SMTO_BLOCK, 250,
                &message_result) != 0;
            GdiFlush();
            text_rendered = native_edit_text_signal(
                canvas, rect, &text_pixels);
        }
    }
    RestoreDC(canvas->dc, saved);
    if (out_text_pixels) *out_text_pixels = text_pixels;
    if (out_text_rendered) *out_text_rendered = text_rendered;
    return delivered;
}

static int composite_native_children(
    WorldgenControlsArtifactWriter *writer,
    StaticPhysicalProbeCanvas *canvas) {
    const FormControls *form = ui_worldgen_legacy_forms_controls();
    WorldgenControlsNativeChild children[] = {
        {form->region_custom_edit, WORLDGEN_CONTROLS_NATIVE_REGION_CUSTOM},
        {form->initial_civs_edit, WORLDGEN_CONTROLS_NATIVE_INITIAL},
        {form->hydrology_initial_civs_edit,
         WORLDGEN_CONTROLS_NATIVE_HYDROLOGY_INITIAL},
        {form->name_edit, WORLDGEN_CONTROLS_NATIVE_NAME},
        {form->symbol_edit, WORLDGEN_CONTROLS_NATIVE_SYMBOL},
        {form->military_edit, UINT32_C(0x00000010)},
        {form->logistics_edit, UINT32_C(0x00000020)},
        {form->governance_edit, UINT32_C(0x00000040)},
        {form->cohesion_edit, UINT32_C(0x00000080)},
        {form->production_edit, UINT32_C(0x00000100)},
        {form->commerce_edit, UINT32_C(0x00000200)},
        {form->innovation_edit, UINT32_C(0x00000400)},
        {form->add_button, WORLDGEN_CONTROLS_NATIVE_ADD},
        {form->apply_button, WORLDGEN_CONTROLS_NATIVE_APPLY}
    };
    int child_count = (int)(sizeof(children) / sizeof(children[0]));
    int ok = 1;
    int index;
    writer->last_native_visible = 0;
    writer->last_native_composited = 0;
    writer->last_native_edit_text_expected = 0;
    writer->last_native_edit_text_rendered = 0;
    writer->last_native_edit_text_pixels = 0;
    writer->last_native_mask = 0;
    for (index = 0; index < child_count; index++) {
        unsigned int text_pixels = 0;
        int text_rendered = 0;
        int expect_text;
        if (!native_child_logically_visible(children[index].window)) continue;
        expect_text = ui_worldgen_legacy_forms_is_edit_id(
                          GetDlgCtrlID(children[index].window)) &&
                      GetWindowTextLengthW(children[index].window) > 0;
        writer->last_native_visible++;
        writer->last_native_mask |= children[index].mask;
        writer->last_native_edit_text_expected += expect_text;
        if (print_native_child(writer, canvas, children[index].window,
                               expect_text, &text_pixels,
                               &text_rendered)) {
            writer->last_native_composited++;
        } else {
            ok = 0;
        }
        writer->last_native_edit_text_rendered += text_rendered;
        writer->last_native_edit_text_pixels += text_pixels;
        if (expect_text && !text_rendered) ok = 0;
    }
    return ok;
}

int worldgen_controls_artifact_render(
    WorldgenControlsArtifactWriter *writer, const char *filename,
    int width, int language) {
    StaticPhysicalProbeCanvas canvas = {0};
    const UiWorldgenControlState *state;
    UiWorldgenEffectiveConfig config;
    RECT client = {0, 0, width, WORLDGEN_CONTROLS_ARTIFACT_HEIGHT};
    RECT owner_client;
    POINT cursor_before = {0}, cursor_after = {0};
    HWND foreground_before = GetForegroundWindow();
    HWND foreground_after;
    DWORD_PTR message_result = 0;
    uint64_t hash = 0;
    unsigned int non_background = 0;
    int opened;
    int wrote = 0;
    int native_ok = 0;
    int cursor_before_ok = GetCursorPos(&cursor_before);
    int resized;
    int geometry_ok;
    int delivered = 0;
    int character_extra_before = 0;
    int character_extra_after = 0;
    int character_extra_restored = 0;
    int capture_ok;
    int ok;
    if (!writer || !writer->manifest || !writer->directory ||
        !writer->owner || !IsWindow(writer->owner) ||
        !IsWindowVisible(writer->owner) || !filename ||
        (width != 340 && width != 460 && width != 500 && width != 720) ||
        (language != UI_LANG_EN && language != UI_LANG_ZH)) return 0;

    writer->artifact_count++;
    side_panel_w = width;
    side_panel_expanded_w = width;
    side_panel_collapsed = 0;
    panel_tab = PANEL_WORLD;
    ui_language = language;
    resized = SetWindowPos(writer->owner, NULL, 0, 0, width,
                           WORLDGEN_CONTROLS_ARTIFACT_HEIGHT,
                           SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE) != 0;
    geometry_ok = GetClientRect(writer->owner, &owner_client) &&
                  owner_client.right - owner_client.left == width &&
                  owner_client.bottom - owner_client.top ==
                      WORLDGEN_CONTROLS_ARTIFACT_HEIGHT;
    layout_native_children(client, width);
    opened = static_physical_probe_canvas_open(
        &canvas, width, WORLDGEN_CONTROLS_ARTIFACT_HEIGHT);
    if (opened) {
        static_physical_probe_canvas_clear(&canvas);
        character_extra_before = GetTextCharacterExtra(canvas.dc);
        delivered = SendMessageTimeoutA(
            writer->owner, WM_PRINTCLIENT, (WPARAM)canvas.dc,
            PRF_CLIENT | PRF_ERASEBKGND,
            SMTO_ABORTIFHUNG | SMTO_BLOCK, 1000, &message_result) != 0;
        character_extra_after = GetTextCharacterExtra(canvas.dc);
        character_extra_restored =
            character_extra_before == character_extra_after;
        native_ok = composite_native_children(writer, &canvas);
        GdiFlush();
        non_background = non_background_pixels(&canvas);
        hash = static_physical_probe_canvas_hash(&canvas);
        wrote = static_physical_probe_canvas_write(
            &canvas, writer->directory, filename);
    }
    state = ui_worldgen_control_state_get();
    ui_worldgen_config_read(&config);
    foreground_after = GetForegroundWindow();
    (void)cursor_before_ok;
    GetCursorPos(&cursor_after);
    capture_ok = resized && geometry_ok && delivered && message_result == 1 &&
                 character_extra_restored &&
                 foreground_before != writer->owner &&
                 foreground_after != writer->owner;
    ok = opened && wrote && native_ok && capture_ok &&
         non_background > (unsigned int)(width *
             WORLDGEN_CONTROLS_ARTIFACT_HEIGHT / 2);
    writer->last_hash = hash;
    writer->last_non_background = non_background;
    if (!ok) writer->failure_count++;
    fprintf(
        writer->manifest,
        "artifact=%s ok=%d renderer=hwnd_wm_printclient width=%d height=%d language=%s tab=%s preset=%s scroll=%d signature=%llu hash=%016llx non_background=%u native_visible=%u native_composited=%u native_mask=%08x native_edit_text=%u/%u native_edit_text_pixels=%u native_ok=%d hwnd_capture=%d hdc_character_extra_restored=%d character_extra_before=%d character_extra_after=%d resized=%d geometry=%d delivered=%d result=%llu foreground_unchanged=%d cursor_unchanged=%d opened=%d wrote=%d\n",
        filename, ok, width, WORLDGEN_CONTROLS_ARTIFACT_HEIGHT,
        language_name(language), tab_name(state->tab),
        state->preset == UI_WORLDGEN_PRESET_BALANCED ? "balanced" : "custom",
        state->scroll_offsets[state->tab],
        (unsigned long long)ui_worldgen_config_signature(&config),
        (unsigned long long)hash, non_background,
        writer->last_native_visible, writer->last_native_composited,
        (unsigned int)writer->last_native_mask,
        writer->last_native_edit_text_rendered,
        writer->last_native_edit_text_expected,
        writer->last_native_edit_text_pixels, native_ok, capture_ok,
        character_extra_restored, character_extra_before,
        character_extra_after, resized, geometry_ok, delivered,
        (unsigned long long)message_result,
        foreground_before == foreground_after,
        cursor_before.x == cursor_after.x && cursor_before.y == cursor_after.y,
        opened, wrote);
    static_physical_probe_canvas_close(&canvas);
    return ok;
}
