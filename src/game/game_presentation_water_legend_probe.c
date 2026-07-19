#include "game/game_presentation_water_legend_probe.h"

#include "render/panel_map_water_legend.h"
#include "render/render_water_surface_cache.h"
#include "ui/ui_types.h"

#include <stdint.h>
#include <string.h>

typedef struct {
    int unique_colors;
    uint32_t pixel_hash;
} LegendSwatchPixels;

static uint32_t solid_hash(void) {
    uint32_t color = render_water_surface_lake_tint_pixel(
        UINT32_C(0xff3787c7)) & UINT32_C(0x00ffffff);
    uint32_t hash = 2166136261u;
    int i;
    for (i = 0; i < 16 * 12; i++) {
        hash ^= color;
        hash *= 16777619u;
    }
    return hash;
}

static int rect_width(RECT rect) { return rect.right - rect.left; }
static int rect_height(RECT rect) { return rect.bottom - rect.top; }

static int geometry_contract_ok(
    const PanelMapWaterLegendPatternStats *stats) {
    const RECT *swatches[3] = {
        &stats->shallow_swatch, &stats->deep_swatch, &stats->lake_swatch
    };
    const POINT *texts[3] = {
        &stats->shallow_text, &stats->deep_text, &stats->lake_text
    };
    int i;
    for (i = 0; i < 3; i++) {
        if (rect_width(*swatches[i]) != 16 ||
            rect_height(*swatches[i]) != 12 ||
            swatches[i]->left != 36 || texts[i]->x != 58 ||
            texts[i]->x - swatches[i]->left != 22 ||
            swatches[i]->top - texts[i]->y != 3) return 0;
        if (i > 0 &&
            (swatches[i]->top - swatches[i - 1]->top != 24 ||
             texts[i]->y - texts[i - 1]->y != 24)) return 0;
    }
    return stats->shallow_swatch.top == 75 &&
           stats->deep_swatch.top == 99 && stats->lake_swatch.top == 123 &&
           stats->shallow_text.y == 72 && stats->deep_text.y == 96 &&
           stats->lake_text.y == 120 && stats->lake_border_drawn == 0;
}

static int geometry_equal(
    const PanelMapWaterLegendPatternStats *left,
    const PanelMapWaterLegendPatternStats *right) {
    return memcmp(&left->shallow_swatch, &right->shallow_swatch,
                  sizeof(left->shallow_swatch)) == 0 &&
           memcmp(&left->deep_swatch, &right->deep_swatch,
                  sizeof(left->deep_swatch)) == 0 &&
           memcmp(&left->lake_swatch, &right->lake_swatch,
                  sizeof(left->lake_swatch)) == 0 &&
           memcmp(&left->shallow_text, &right->shallow_text,
                  sizeof(left->shallow_text)) == 0 &&
           memcmp(&left->deep_text, &right->deep_text,
                  sizeof(left->deep_text)) == 0 &&
           memcmp(&left->lake_text, &right->lake_text,
                  sizeof(left->lake_text)) == 0;
}

static int sample_lake_swatch(const StaticPhysicalProbeCanvas *canvas,
                              RECT swatch,
                              LegendSwatchPixels *out) {
    uint32_t seen[16 * 12];
    uint32_t hash = 2166136261u;
    int seen_count = 0;
    int x;
    int y;
    if (!canvas || !canvas->pixels || !out ||
        rect_width(swatch) != 16 || rect_height(swatch) != 12 ||
        swatch.left < 0 || swatch.top < 0 ||
        swatch.right > canvas->width || swatch.bottom > canvas->height)
        return 0;
    for (y = swatch.top; y < swatch.bottom; y++) {
        for (x = swatch.left; x < swatch.right; x++) {
            uint32_t pixel = canvas->pixels[y * canvas->width + x] &
                             UINT32_C(0x00ffffff);
            int known = 0;
            int i;
            hash ^= pixel;
            hash *= 16777619u;
            for (i = 0; i < seen_count; i++) {
                if (seen[i] == pixel) {
                    known = 1;
                    break;
                }
            }
            if (!known) seen[seen_count++] = pixel;
        }
    }
    out->unique_colors = seen_count;
    out->pixel_hash = hash;
    return 1;
}

static int draw_artifact(StaticPhysicalProbeCanvas *canvas, int language,
                         const char *name, int prewarm,
                         PanelMapWaterLegendPatternStats *out,
                         LegendSwatchPixels *pixels) {
    int ready = 1;
    ui_language = language;
    static_physical_probe_canvas_clear(canvas);
    if (prewarm) ready = panel_map_water_legend_prewarm(canvas->dc);
    panel_map_water_legend_draw(canvas->dc, 36, 48, 24);
    if (!GdiFlush()) return 0;
    *out = *panel_map_water_legend_pattern_stats();
    return ready && sample_lake_swatch(
               canvas, out->lake_swatch, pixels) &&
           static_physical_probe_canvas_write(
        canvas, static_physical_probe_artifact_dir(), name);
}

int game_presentation_water_legend_probe(
    FILE *summary, StaticPhysicalProbeCanvas *canvas) {
    PanelMapWaterLegendPatternStats en = {0}, zh = {0};
    PanelMapWaterLegendPatternStats fallback_first = {0};
    PanelMapWaterLegendPatternStats fallback_second = {0};
    LegendSwatchPixels en_pixels = {0}, zh_pixels = {0};
    LegendSwatchPixels fallback_pixels = {0}, fallback_repeat_pixels = {0};
    int old_language = ui_language;
    int en_artifact, zh_artifact, fallback_artifact, fallback_repeat_artifact;
    int labels_ok, pattern_ok, geometry_ok, stable_ok, fallback_ok;
    panel_map_water_legend_release();
    fallback_artifact = draw_artifact(
        canvas, UI_LANG_EN, "static_water_legend_fallback.bmp", 0,
        &fallback_first, &fallback_pixels);
    panel_map_water_legend_release();
    fallback_repeat_artifact = draw_artifact(
        canvas, UI_LANG_EN, "static_water_legend_fallback_repeat.bmp", 0,
        &fallback_second, &fallback_repeat_pixels);
    panel_map_water_legend_release();
    en_artifact = draw_artifact(canvas, UI_LANG_EN,
                                "static_water_legend_en.bmp", 1, &en,
                                &en_pixels);
    panel_map_water_legend_release();
    zh_artifact = draw_artifact(canvas, UI_LANG_ZH,
                                "static_water_legend_zh.bmp", 1, &zh,
                                &zh_pixels);
    labels_ok = strcmp(panel_map_water_legend_label_en(), "Inland Lake") == 0 &&
        strcmp(panel_map_water_legend_label_zh(), "内陆湖泊") == 0;
    pattern_ok = en.ready && en.width == 16 && en.height == 12 &&
        en.unique_colors >= 4 && en.pixel_hash != solid_hash();
    geometry_ok = geometry_contract_ok(&en) && geometry_contract_ok(&zh) &&
        geometry_equal(&en, &zh);
    stable_ok = zh.ready && zh.width == en.width && zh.height == en.height &&
        zh.unique_colors == en.unique_colors && zh.pixel_hash == en.pixel_hash;
    fallback_ok = !fallback_first.ready && !fallback_second.ready &&
        fallback_first.fallback_drawn && fallback_second.fallback_drawn &&
        geometry_contract_ok(&fallback_first) &&
        geometry_contract_ok(&fallback_second) &&
        fallback_pixels.unique_colors == 5 &&
        fallback_pixels.pixel_hash != solid_hash() &&
        fallback_repeat_pixels.unique_colors == 5 &&
        fallback_repeat_pixels.pixel_hash == fallback_pixels.pixel_hash &&
        fallback_artifact && fallback_repeat_artifact;
    if (summary) {
        fprintf(summary,
                "case=static_water_legend ok=%d labels=%d pattern=%d "
                "geometry=%d stable=%d fallback=%d border=%d/%d "
                "size=%dx%d unique=%d hash=%u/%u solid=%u "
                "fallback_ready=%d/%d fallback_drawn=%d/%d "
                "fallback_unique=%d/%d fallback_hash=%u/%u "
                "swatches=%ld,%ld,%ld,%ld/%ld,%ld,%ld,%ld/%ld,%ld,%ld,%ld "
                "text=%ld,%ld/%ld,%ld/%ld,%ld "
                "artifacts=static_water_legend_fallback.bmp/"
                "static_water_legend_fallback_repeat.bmp/"
                "static_water_legend_en.bmp/static_water_legend_zh.bmp\n",
                labels_ok && pattern_ok && geometry_ok && stable_ok && fallback_ok &&
                    en_artifact && zh_artifact,
                labels_ok, pattern_ok, geometry_ok, stable_ok, fallback_ok,
                en.lake_border_drawn, zh.lake_border_drawn,
                en.width, en.height, en.unique_colors,
                en.pixel_hash, zh.pixel_hash, solid_hash(),
                fallback_first.ready, fallback_second.ready,
                fallback_first.fallback_drawn, fallback_second.fallback_drawn,
                fallback_pixels.unique_colors,
                fallback_repeat_pixels.unique_colors,
                fallback_pixels.pixel_hash, fallback_repeat_pixels.pixel_hash,
                en.shallow_swatch.left, en.shallow_swatch.top,
                en.shallow_swatch.right, en.shallow_swatch.bottom,
                en.deep_swatch.left, en.deep_swatch.top,
                en.deep_swatch.right, en.deep_swatch.bottom,
                en.lake_swatch.left, en.lake_swatch.top,
                en.lake_swatch.right, en.lake_swatch.bottom,
                en.shallow_text.x, en.shallow_text.y,
                en.deep_text.x, en.deep_text.y,
                en.lake_text.x, en.lake_text.y);
    }
    ui_language = old_language;
    return labels_ok && pattern_ok && geometry_ok && stable_ok && fallback_ok &&
           en_artifact && zh_artifact;
}
