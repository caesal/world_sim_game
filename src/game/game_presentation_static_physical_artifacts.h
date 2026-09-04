#ifndef WORLD_SIM_GAME_PRESENTATION_STATIC_PHYSICAL_ARTIFACTS_H
#define WORLD_SIM_GAME_PRESENTATION_STATIC_PHYSICAL_ARTIFACTS_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define PRESENTATION_PROBE_DEFAULT_DIR \
    "build/validation/presentation_probe_20260618"
#define PRESENTATION_PROBE_DIR_ENV "WORLD_SIM_PRESENTATION_PROBE_DIR"
#define PRESENTATION_PROBE_SUMMARY_ENV "WORLD_SIM_PRESENTATION_PROBE_SUMMARY"

typedef struct {
    HDC dc;
    HBITMAP bitmap;
    HBITMAP previous;
    BITMAPINFO info;
    uint32_t *pixels;
    int width;
    int height;
} StaticPhysicalProbeCanvas;

const char *static_physical_probe_artifact_dir(void);
int static_physical_probe_prepare_artifact_dir(void);
int static_physical_probe_summary_path(char *out, size_t out_size);
int static_physical_probe_join_path(char *out, size_t out_size,
                                    const char *directory, const char *name);
const char *static_physical_probe_artifact_path(const char *name);
int static_physical_probe_artifact_registry_begin(int expected_count);
int static_physical_probe_artifact_registry_finish(FILE *summary);

int static_physical_probe_canvas_open(StaticPhysicalProbeCanvas *canvas,
                                      int width, int height);
void static_physical_probe_canvas_close(StaticPhysicalProbeCanvas *canvas);
void static_physical_probe_canvas_clear(StaticPhysicalProbeCanvas *canvas);
int static_physical_probe_canvas_write(const StaticPhysicalProbeCanvas *canvas,
                                       const char *directory, const char *name);
uint64_t static_physical_probe_canvas_hash(const StaticPhysicalProbeCanvas *canvas);

#endif
