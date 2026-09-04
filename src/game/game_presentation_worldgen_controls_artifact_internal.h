#ifndef WORLD_SIM_GAME_PRESENTATION_WORLDGEN_CONTROLS_ARTIFACT_INTERNAL_H
#define WORLD_SIM_GAME_PRESENTATION_WORLDGEN_CONTROLS_ARTIFACT_INTERNAL_H

#include "game/game_presentation_worldgen_controls_probe_internal.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdint.h>

#define WORLDGEN_CONTROLS_ARTIFACT_HEIGHT 1080
#define WORLDGEN_CONTROLS_ARTIFACT_EXPECTED_COUNT 99

#define WORLDGEN_CONTROLS_NATIVE_REGION_CUSTOM UINT32_C(0x00000001)
#define WORLDGEN_CONTROLS_NATIVE_INITIAL UINT32_C(0x00000002)
#define WORLDGEN_CONTROLS_NATIVE_NAME UINT32_C(0x00000004)
#define WORLDGEN_CONTROLS_NATIVE_SYMBOL UINT32_C(0x00000008)
#define WORLDGEN_CONTROLS_NATIVE_METRICS UINT32_C(0x000007f0)
#define WORLDGEN_CONTROLS_NATIVE_ADD UINT32_C(0x00000800)
#define WORLDGEN_CONTROLS_NATIVE_APPLY UINT32_C(0x00001000)
#define WORLDGEN_CONTROLS_NATIVE_HYDROLOGY_INITIAL UINT32_C(0x00002000)
#define WORLDGEN_CONTROLS_NATIVE_LEGACY_BOTTOM \
    (WORLDGEN_CONTROLS_NATIVE_NAME | WORLDGEN_CONTROLS_NATIVE_SYMBOL | \
     WORLDGEN_CONTROLS_NATIVE_METRICS | WORLDGEN_CONTROLS_NATIVE_ADD | \
     WORLDGEN_CONTROLS_NATIVE_APPLY)

typedef struct {
    WorldgenControlsProbeReport *report;
    const char *directory;
    FILE *manifest;
    HWND owner;
    int artifact_count;
    int failure_count;
    uint64_t last_hash;
    unsigned int last_non_background;
    unsigned int last_native_visible;
    unsigned int last_native_composited;
    unsigned int last_native_edit_text_expected;
    unsigned int last_native_edit_text_rendered;
    unsigned int last_native_edit_text_pixels;
    uint32_t last_native_mask;
} WorldgenControlsArtifactWriter;

int worldgen_controls_artifact_render(
    WorldgenControlsArtifactWriter *writer, const char *filename,
    int width, int language);
int worldgen_controls_artifact_matrix(
    WorldgenControlsArtifactWriter *writer);
int worldgen_controls_artifact_assets(
    WorldgenControlsArtifactWriter *writer);

#endif
