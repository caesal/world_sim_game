#ifndef WORLD_SIM_GAME_PRESENTATION_STATIC_CAMERA_RESOURCES_H
#define WORLD_SIM_GAME_PRESENTATION_STATIC_CAMERA_RESOURCES_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>

enum { STATIC_CAMERA_RESOURCE_TRACE_MAX = 12 };

typedef struct {
    DWORD gdi_objects;
    unsigned long long working_set;
    unsigned long long private_bytes;
    int valid;
} StaticCameraResources;

typedef struct {
    StaticCameraResources samples[STATIC_CAMERA_RESOURCE_TRACE_MAX];
    const char *stages[STATIC_CAMERA_RESOURCE_TRACE_MAX];
    int count;
} StaticCameraResourceTrace;

StaticCameraResources static_camera_resources_capture(void);
StaticCameraResources static_camera_resources_settle(
    StaticCameraResourceTrace *trace, int *settled);
int static_camera_resources_append(StaticCameraResourceTrace *trace,
                                   StaticCameraResources sample,
                                   const char *stage);
int static_camera_resource_contract_ok(int settled, int internal_ok,
                                       StaticCameraResources before,
                                       StaticCameraResources after);
int static_camera_resource_trace_report(
    FILE *summary, const StaticCameraResourceTrace *trace);
int static_camera_resource_contract_probe(FILE *summary);

#endif
