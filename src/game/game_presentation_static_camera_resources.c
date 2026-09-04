#include "game/game_presentation_static_camera_resources.h"

#include <psapi.h>

#include <limits.h>
#include <string.h>

typedef BOOL(WINAPI *MemoryInfoFn)(HANDLE, PPROCESS_MEMORY_COUNTERS, DWORD);

StaticCameraResources static_camera_resources_capture(void) {
    StaticCameraResources result = {0};
    PROCESS_MEMORY_COUNTERS_EX memory = {0};
    HMODULE module = GetModuleHandleA("kernel32.dll");
    MemoryInfoFn query = module ? (MemoryInfoFn)(void *)GetProcAddress(
                                      module, "K32GetProcessMemoryInfo") : NULL;
    memory.cb = sizeof(memory);
    result.gdi_objects = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    if (query && query(GetCurrentProcess(), (PPROCESS_MEMORY_COUNTERS)&memory,
                       sizeof(memory))) {
        result.working_set = (unsigned long long)memory.WorkingSetSize;
        result.private_bytes = (unsigned long long)memory.PrivateUsage;
        result.valid = result.gdi_objects > 0;
    }
    return result;
}

int static_camera_resources_append(StaticCameraResourceTrace *trace,
                                   StaticCameraResources sample,
                                   const char *stage) {
    if (!trace || trace->count < 0 ||
        trace->count >= STATIC_CAMERA_RESOURCE_TRACE_MAX) return 0;
    trace->samples[trace->count] = sample;
    trace->stages[trace->count] = stage ? stage : "unspecified";
    trace->count++;
    return 1;
}

StaticCameraResources static_camera_resources_settle(
    StaticCameraResourceTrace *trace, int *settled) {
    StaticCameraResources previous = static_camera_resources_capture();
    int i;
    if (trace) memset(trace, 0, sizeof(*trace));
    if (settled) *settled = 0;
    static_camera_resources_append(trace, previous, "settle_0");
    for (i = 0; i < 8; i++) {
        StaticCameraResources current = static_camera_resources_capture();
        static_camera_resources_append(trace, current, "settle");
        if (previous.valid && current.valid &&
            previous.gdi_objects == current.gdi_objects &&
            previous.private_bytes == current.private_bytes) {
            if (settled) *settled = 1;
            return current;
        }
        previous = current;
    }
    return previous;
}

int static_camera_resource_contract_ok(int settled, int internal_ok,
                                       StaticCameraResources before,
                                       StaticCameraResources after) {
    return settled && internal_ok && before.valid && after.valid &&
           before.gdi_objects == after.gdi_objects &&
           after.private_bytes <= before.private_bytes;
}

static int working_set_tail_equal(const StaticCameraResourceTrace *trace,
                                  int count) {
    int i;
    unsigned long long value;
    if (!trace || count <= 0 || trace->count < count) return 0;
    value = trace->samples[trace->count - 1].working_set;
    for (i = trace->count - count; i < trace->count; i++) {
        if (!trace->samples[i].valid ||
            trace->samples[i].working_set != value) return 0;
    }
    return 1;
}

static int resource_trace_mandatory_ok(
    const StaticCameraResourceTrace *trace) {
    DWORD gdi;
    int i;
    if (!trace || trace->count <= 0 || !trace->samples[0].valid) return 0;
    gdi = trace->samples[0].gdi_objects;
    for (i = 0; i < trace->count; i++) {
        if (!trace->samples[i].valid ||
            trace->samples[i].gdi_objects != gdi) return 0;
    }
    return 1;
}

int static_camera_resource_trace_report(
    FILE *summary, const StaticCameraResourceTrace *trace) {
    unsigned long long minimum = ULLONG_MAX;
    unsigned long long maximum = 0;
    unsigned long long first = 0;
    unsigned long long last = 0;
    long long delta = 0;
    int positive_transitions = 0;
    int valid_samples = 0;
    int gdi_unchanged = trace && trace->count > 0;
    int i;
    int mandatory_ok = resource_trace_mandatory_ok(trace);
    if (!summary || !trace) return 0;
    for (i = 0; i < trace->count; i++) {
        const StaticCameraResources *sample = &trace->samples[i];
        if (i > 0 && sample->gdi_objects !=
            trace->samples[0].gdi_objects) gdi_unchanged = 0;
        fprintf(summary,
                "case=static_camera_resource_sample index=%d stage=%s valid=%d gdi=%lu private=%llu working_set=%llu\n",
                i, trace->stages[i], sample->valid,
                (unsigned long)sample->gdi_objects, sample->private_bytes,
                sample->working_set);
        if (!sample->valid) continue;
        if (!valid_samples) first = sample->working_set;
        last = sample->working_set;
        if (sample->working_set < minimum) minimum = sample->working_set;
        if (sample->working_set > maximum) maximum = sample->working_set;
        valid_samples++;
        if (i > 0 && trace->samples[i - 1].valid &&
            sample->working_set > trace->samples[i - 1].working_set) {
            positive_transitions++;
        }
    }
    if (!valid_samples) minimum = 0;
    if (valid_samples) delta = (long long)last - (long long)first;
    fprintf(summary,
            "case=static_camera_working_set_diagnostics ok=%d working_set_diagnostic_only=1 samples=%d valid_samples=%d all_samples_valid=%d gdi_unchanged=%d first=%llu last=%llu delta=%lld min=%llu max=%llu positive_transitions=%d tail_last2_equal=%d tail_last3_equal=%d\n",
            mandatory_ok, trace->count,
            valid_samples, valid_samples == trace->count, gdi_unchanged,
            first, last, delta, minimum, maximum,
            positive_transitions, working_set_tail_equal(trace, 2),
            working_set_tail_equal(trace, 3));
    return mandatory_ok;
}

int static_camera_resource_contract_probe(FILE *summary) {
    StaticCameraResources base = {7, 100, 200, 1};
    StaticCameraResources rise = {7, 104, 200, 1};
    StaticCameraResources fall = {7, 96, 200, 1};
    StaticCameraResources equal = {7, 100, 200, 1};
    StaticCameraResources gdi_change = {8, 100, 200, 1};
    StaticCameraResources private_growth = {7, 100, 201, 1};
    StaticCameraResources invalid = {7, 100, 200, 0};
    StaticCameraResourceTrace valid_trace = {
        {{7, 100, 200, 1}, {7, 104, 200, 1}, {7, 96, 200, 1}},
        {"first", "rise", "fall"}, 3
    };
    StaticCameraResourceTrace invalid_trace = {
        {{7, 100, 200, 1}, {7, 104, 200, 0}, {7, 96, 200, 1}},
        {"first", "invalid", "last"}, 3
    };
    StaticCameraResourceTrace gdi_trace = {
        {{7, 100, 200, 1}, {8, 104, 200, 1}, {7, 96, 200, 1}},
        {"first", "gdi_change", "last"}, 3
    };
    int ws_rise = static_camera_resource_contract_ok(1, 1, base, rise);
    int ws_fall = static_camera_resource_contract_ok(1, 1, base, fall);
    int ws_equal = static_camera_resource_contract_ok(1, 1, base, equal);
    int gdi_rejected = !static_camera_resource_contract_ok(
        1, 1, base, gdi_change);
    int private_rejected = !static_camera_resource_contract_ok(
        1, 1, base, private_growth);
    int invalid_rejected =
        !static_camera_resource_contract_ok(1, 1, base, invalid) &&
        !static_camera_resource_contract_ok(1, 1, invalid, base);
    int unsettled_rejected = !static_camera_resource_contract_ok(
        0, 1, base, equal);
    int internal_rejected = !static_camera_resource_contract_ok(
        1, 0, base, equal);
    int trace_valid = resource_trace_mandatory_ok(&valid_trace);
    int trace_invalid_rejected =
        !resource_trace_mandatory_ok(&invalid_trace);
    int trace_gdi_rejected = !resource_trace_mandatory_ok(&gdi_trace);
    int ok = ws_rise && ws_fall && ws_equal && gdi_rejected &&
             private_rejected && invalid_rejected && unsettled_rejected &&
             internal_rejected && trace_valid && trace_invalid_rejected &&
             trace_gdi_rejected;
    if (summary) {
        fprintf(summary,
                "case=static_camera_resource_classifier_contract ok=%d working_set_rise_pass=%d working_set_fall_pass=%d working_set_equal_pass=%d gdi_change_rejected=%d private_growth_rejected=%d invalid_rejected=%d unsettled_rejected=%d internal_change_rejected=%d trace_valid_pass=%d trace_invalid_rejected=%d trace_gdi_change_rejected=%d working_set_diagnostic_only=1\n",
                ok, ws_rise, ws_fall, ws_equal, gdi_rejected,
                private_rejected, invalid_rejected, unsettled_rejected,
                internal_rejected, trace_valid, trace_invalid_rejected,
                trace_gdi_rejected);
    }
    return ok;
}
