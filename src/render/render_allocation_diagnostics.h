#ifndef WORLD_SIM_RENDER_ALLOCATION_DIAGNOSTICS_H
#define WORLD_SIM_RENDER_ALLOCATION_DIAGNOSTICS_H

#include <stdint.h>

typedef enum {
    RENDER_ALLOCATION_LAYER_CACHE = 0,
    RENDER_ALLOCATION_STATIC_MAP,
    RENDER_ALLOCATION_PHYSICAL,
    RENDER_ALLOCATION_PHYSICAL_OVERLAY,
    RENDER_ALLOCATION_COUNT
} RenderAllocationOwner;

typedef struct {
    uint64_t attempts[RENDER_ALLOCATION_COUNT];
    uint64_t failures[RENDER_ALLOCATION_COUNT];
    uint64_t injected_failures;
    RenderAllocationOwner last_failure_owner;
    uint64_t last_failure_bytes;
    uint64_t largest_request_bytes;
    int last_failure_width;
    int last_failure_height;
} RenderAllocationDiagnostics;

void render_allocation_note_attempt(RenderAllocationOwner owner, int width, int height);
void render_allocation_note_failure(RenderAllocationOwner owner, int width, int height);
int render_allocation_inject_failure(RenderAllocationOwner owner, int width, int height);
void render_allocation_diagnostics_get(RenderAllocationDiagnostics *out);
void render_allocation_diagnostics_reset(void);
const char *render_allocation_owner_name(RenderAllocationOwner owner);

#endif
