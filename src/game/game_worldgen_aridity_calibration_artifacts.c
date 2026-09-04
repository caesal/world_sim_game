#include "game/game_worldgen_aridity_calibration_artifacts.h"

#include "game/game_presentation_static_physical_artifacts.h"
#include "render/render_common.h"
#include "world/world_gen_context.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

enum { ARTIFACT_NAME_CAPACITY = 160 };

static uint32_t dib_pixel(COLORREF color) {
    return (uint32_t)GetBValue(color) |
           ((uint32_t)GetGValue(color) << 8) |
           ((uint32_t)GetRValue(color) << 16) | UINT32_C(0xff000000);
}

static int make_artifact_name(char *out, size_t capacity,
                              const char *stem, const char *mode) {
    int written;
    if (!out || capacity == 0 || !stem || !stem[0] || !mode || !mode[0] ||
        strchr(stem, '/') || strchr(stem, '\\') || strchr(stem, ':')) {
        return 0;
    }
    written = snprintf(out, capacity, "%s_%s.bmp", stem, mode);
    return written > 0 && (size_t)written < capacity;
}

static int path_is_available(const char *directory, const char *name) {
    char path[MAX_PATH];
    DWORD error;
    if (!static_physical_probe_join_path(
            path, sizeof(path), directory, name)) return 0;
    if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES) return 0;
    error = GetLastError();
    return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
}

static int context_is_valid(const WorldGenContext *context) {
    size_t expected;
    if (!context || context->width <= 0 || context->height <= 0 ||
        context->width > MAX_MAP_W || context->height > MAX_MAP_H ||
        !context->geography || !context->climate) return 0;
    expected = (size_t)context->width * (size_t)context->height;
    return expected <= INT_MAX && context->tile_count == (int)expected;
}

static int fill_canvases(
    const WorldGenContext *context, StaticPhysicalProbeCanvas *geography,
    StaticPhysicalProbeCanvas *climate) {
    int geography_nonzero = 0;
    int climate_nonzero = 0;
    int i;
    for (i = 0; i < context->tile_count; i++) {
        Geography geography_value = (Geography)context->geography[i];
        Climate climate_value = (Climate)context->climate[i];
        uint32_t geography_pixel;
        uint32_t climate_pixel;
        if (geography_value < 0 || geography_value >= GEO_COUNT ||
            climate_value < 0 || climate_value >= CLIMATE_COUNT) return 0;
        geography_pixel = dib_pixel(geography_color(geography_value));
        climate_pixel = dib_pixel(climate_color(climate_value));
        geography->pixels[i] = geography_pixel;
        climate->pixels[i] = climate_pixel;
        geography_nonzero |= (geography_pixel & UINT32_C(0x00ffffff)) != 0;
        climate_nonzero |= (climate_pixel & UINT32_C(0x00ffffff)) != 0;
    }
    return geography_nonzero && climate_nonzero;
}

int game_worldgen_aridity_calibration_artifacts_write(
    const WorldGenContext *context, const char *output_directory,
    const char *filename_stem,
    WorldGenAridityCalibrationArtifactResult *result) {
    StaticPhysicalProbeCanvas geography = {0};
    StaticPhysicalProbeCanvas climate = {0};
    WorldGenAridityCalibrationArtifactResult completed = {0};
    char geography_name[ARTIFACT_NAME_CAPACITY];
    char climate_name[ARTIFACT_NAME_CAPACITY];
    int ok = 0;
    if (result) memset(result, 0, sizeof(*result));
    if (!result || !output_directory || !output_directory[0] ||
        !context_is_valid(context) ||
        !make_artifact_name(geography_name, sizeof(geography_name),
                            filename_stem, "geography") ||
        !make_artifact_name(climate_name, sizeof(climate_name),
                            filename_stem, "climate") ||
        !path_is_available(output_directory, geography_name) ||
        !path_is_available(output_directory, climate_name)) goto cleanup;
    if (!static_physical_probe_canvas_open(
            &geography, context->width, context->height) ||
        !static_physical_probe_canvas_open(
            &climate, context->width, context->height) ||
        !fill_canvases(context, &geography, &climate)) goto cleanup;
    completed.geography_hash =
        static_physical_probe_canvas_hash(&geography);
    completed.climate_hash = static_physical_probe_canvas_hash(&climate);
    completed.width = context->width;
    completed.height = context->height;
    if (!completed.geography_hash || !completed.climate_hash ||
        !static_physical_probe_canvas_write(
            &geography, output_directory, geography_name) ||
        !static_physical_probe_canvas_write(
            &climate, output_directory, climate_name)) goto cleanup;
    *result = completed;
    ok = 1;

cleanup:
    static_physical_probe_canvas_close(&climate);
    static_physical_probe_canvas_close(&geography);
    return ok;
}
