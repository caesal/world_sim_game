#include "render/render_allocation_diagnostics.h"

#include "core/worldgen_attempt.h"
#include "core/worldgen_fault_injection.h"

#include <string.h>

static RenderAllocationDiagnostics diagnostics;

static uint64_t allocation_bytes(int width, int height) {
    if (width <= 0 || height <= 0) return 0;
    return (uint64_t)width * (uint64_t)height * 4u;
}

void render_allocation_note_attempt(RenderAllocationOwner owner, int width, int height) {
    uint64_t bytes;
    if (owner < 0 || owner >= RENDER_ALLOCATION_COUNT) return;
    bytes = allocation_bytes(width, height);
    diagnostics.attempts[owner]++;
    if (bytes > diagnostics.largest_request_bytes) diagnostics.largest_request_bytes = bytes;
}

void render_allocation_note_failure(RenderAllocationOwner owner, int width, int height) {
    if (owner < 0 || owner >= RENDER_ALLOCATION_COUNT) return;
    diagnostics.failures[owner]++;
    diagnostics.last_failure_owner = owner;
    diagnostics.last_failure_width = width;
    diagnostics.last_failure_height = height;
    diagnostics.last_failure_bytes = allocation_bytes(width, height);
}

int render_allocation_inject_failure(RenderAllocationOwner owner, int width, int height) {
    if (!worldgen_fault_injection_should_fail(WORLDGEN_FAULT_PREWARM_ALLOCATION))
        return 0;
    diagnostics.injected_failures++;
    render_allocation_note_failure(owner, width, height);
    worldgen_attempt_record_failure(WORLDGEN_FAILURE_PREWARM_ALLOCATION_INJECTED);
    return 1;
}

void render_allocation_diagnostics_get(RenderAllocationDiagnostics *out) {
    if (out) *out = diagnostics;
}

void render_allocation_diagnostics_reset(void) {
    memset(&diagnostics, 0, sizeof(diagnostics));
}

const char *render_allocation_owner_name(RenderAllocationOwner owner) {
    switch (owner) {
        case RENDER_ALLOCATION_LAYER_CACHE: return "layer-cache";
        case RENDER_ALLOCATION_STATIC_MAP: return "static-map";
        case RENDER_ALLOCATION_PHYSICAL: return "physical";
        case RENDER_ALLOCATION_PHYSICAL_OVERLAY: return "physical-overlay";
        default: return "unknown";
    }
}
