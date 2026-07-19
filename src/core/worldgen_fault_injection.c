#include "core/worldgen_fault_injection.h"

#include <string.h>

typedef struct {
    int first_call;
    int failure_count;
    int calls;
    int triggered;
} WorldGenFaultSlot;

static WorldGenFaultSlot slots[WORLDGEN_FAULT_COUNT];

static int point_valid(WorldGenFaultPoint point) {
    return point > WORLDGEN_FAULT_NONE && point < WORLDGEN_FAULT_COUNT;
}

void worldgen_fault_injection_arm(WorldGenFaultPoint point,
                                  int first_call, int failure_count) {
    WorldGenFaultSlot *slot;
    if (!point_valid(point)) return;
    slot = &slots[point];
    memset(slot, 0, sizeof(*slot));
    slot->first_call = first_call > 0 ? first_call : 1;
    slot->failure_count = failure_count > 0 ? failure_count : 1;
}

void worldgen_fault_injection_clear(void) {
    memset(slots, 0, sizeof(slots));
}

int worldgen_fault_injection_should_fail(WorldGenFaultPoint point) {
    WorldGenFaultSlot *slot;
    int offset;
    if (!point_valid(point)) return 0;
    slot = &slots[point];
    slot->calls++;
    if (slot->failure_count <= 0) return 0;
    offset = slot->calls - slot->first_call;
    if (offset < 0 || offset >= slot->failure_count) return 0;
    slot->triggered = 1;
    return 1;
}

int worldgen_fault_injection_was_triggered(WorldGenFaultPoint point) {
    return point_valid(point) ? slots[point].triggered : 0;
}

int worldgen_fault_injection_call_count(WorldGenFaultPoint point) {
    return point_valid(point) ? slots[point].calls : 0;
}
