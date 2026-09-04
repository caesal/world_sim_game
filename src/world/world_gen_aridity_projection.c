#include "world/world_gen_aridity_projection.h"

#include <string.h>

typedef struct {
    int enabled;
    int failed;
    WorldGenContext *context;
    int tile_count;
    int pre_count;
    int post_count;
    WorldGenAridityProjectionResult result;
} ProjectionCaptureState;

static ProjectionCaptureState capture_state;

static uint64_t hash_mix(uint64_t hash, uint64_t value) {
    int byte_index;
    for (byte_index = 0; byte_index < 8; byte_index++) {
        hash ^= (uint8_t)(value >> (byte_index * 8));
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static int context_fields_valid(const WorldGenContext *context) {
    return context && context->width > 0 && context->height > 0 &&
        context->tile_count == context->width * context->height &&
        context->land_mask && context->elevation &&
        context->relative_altitude && context->moisture &&
        context->temperature && context->climate;
}

static void clear_capture_preserving_enable(void) {
    int enabled = capture_state.enabled;
    memset(&capture_state, 0, sizeof(capture_state));
    capture_state.enabled = enabled;
}

int world_gen_aridity_projection_validation_enable(void) {
    world_gen_aridity_projection_validation_reset();
    capture_state.enabled = 1;
    return 1;
}

void world_gen_aridity_projection_validation_reset(void) {
    memset(&capture_state, 0, sizeof(capture_state));
}

int world_gen_aridity_projection_validation_active(void) {
    return capture_state.enabled;
}

int world_gen_aridity_projection_validation_matches(void) {
    return capture_state.enabled && !capture_state.failed;
}

int world_gen_aridity_projection_capture_begin_context(
    WorldGenContext *context) {
    uint64_t hash = UINT64_C(1469598103934665603);
    if (!capture_state.enabled || !context_fields_valid(context)) {
        capture_state.failed = 1;
        return 0;
    }
    clear_capture_preserving_enable();
    capture_state.context = context;
    capture_state.tile_count = context->tile_count;
    capture_state.result.width = context->width;
    capture_state.result.height = context->height;
    capture_state.result.master_seed = context->master_seed;
    hash = hash_mix(hash, (uint32_t)context->width);
    hash = hash_mix(hash, (uint32_t)context->height);
    hash = hash_mix(hash, context->master_seed);
    hash = hash_mix(hash, (uint32_t)context->config.ocean);
    hash = hash_mix(hash, (uint32_t)context->config.continent);
    hash = hash_mix(hash, (uint32_t)context->config.relief);
    hash = hash_mix(hash, (uint32_t)context->config.moisture);
    hash = hash_mix(hash, (uint32_t)context->config.drought);
    hash = hash_mix(hash, (uint32_t)context->config.vegetation);
    hash = hash_mix(hash, (uint32_t)context->config.bias_forest);
    hash = hash_mix(hash, (uint32_t)context->config.bias_mountain);
    hash = hash_mix(hash, (uint32_t)context->config.bias_wetland);
    hash = hash_mix(hash, context->phase_seed[WORLD_GEN_PHASE_CLIMATE]);
    hash = hash_mix(hash, context->phase_seed[WORLD_GEN_PHASE_HYDROLOGY]);
    capture_state.result.climate_input_hash = hash;
    return 1;
}

int world_gen_aridity_projection_capture_pre(
    const WorldGenContext *context, int index, int eligible, Climate climate) {
    uint64_t identity;
    uint64_t climate_fields;
    uint64_t hash;
    if (!capture_state.enabled || capture_state.failed ||
        capture_state.context != context || index != capture_state.pre_count ||
        index < 0 || index >= context->tile_count ||
        (eligible != 0 && eligible != 1) || climate < 0 ||
        climate >= CLIMATE_COUNT) {
        capture_state.failed = 1;
        return 0;
    }
    if (context->moisture[index] < 0 || context->moisture[index] > 100 ||
        context->temperature[index] < 0 || context->temperature[index] > 100) {
        capture_state.failed = 1;
        return 0;
    }
    if (context->land_mask[index]) {
        capture_state.result.land_count++;
        capture_state.result.pre_climate_count[climate]++;
        if (eligible) {
            capture_state.result.eligible_count++;
            capture_state.result.histogram[context->moisture[index]]++;
        }
    } else if (eligible || climate != CLIMATE_OCEANIC) {
        capture_state.failed = 1;
        return 0;
    }
    identity = (uint32_t)index |
        ((uint64_t)context->land_mask[index] << 32);
    climate_fields = (uint16_t)context->elevation[index] |
        ((uint64_t)(uint16_t)context->relative_altitude[index] << 16) |
        ((uint64_t)(uint16_t)context->moisture[index] << 32) |
        ((uint64_t)(uint16_t)context->temperature[index] << 48);
    hash = capture_state.result.climate_input_hash;
    hash = hash_mix(hash, identity);
    hash = hash_mix(hash, climate_fields);
    capture_state.result.climate_input_hash = hash;
    capture_state.pre_count++;
    return 1;
}

int world_gen_aridity_projection_capture_post(
    const WorldGenContext *context, int index, int refresh_eligible,
    Climate pre_climate, Climate post_climate) {
    int transition_kind;
    if (!capture_state.enabled || capture_state.failed ||
        capture_state.context != context || index != capture_state.post_count ||
        index < 0 || index >= context->tile_count ||
        (refresh_eligible != 0 && refresh_eligible != 1) ||
        pre_climate < 0 || pre_climate >= CLIMATE_COUNT ||
        post_climate < 0 || post_climate >= CLIMATE_COUNT ||
        context->climate[index] != (uint8_t)pre_climate) {
        capture_state.failed = 1;
        return 0;
    }
    if (context->land_mask[index]) {
        transition_kind = refresh_eligible
            ? WORLD_GEN_ARIDITY_TRANSITION_REFRESH
            : WORLD_GEN_ARIDITY_TRANSITION_NON_REFRESH;
        capture_state.result.post_climate_count[post_climate]++;
        capture_state.result.transition_count[transition_kind]
            [pre_climate][post_climate]++;
        capture_state.result.transition_accounted_count++;
        capture_state.result.refresh_eligible_count += refresh_eligible;
        if (pre_climate != post_climate) {
            if (refresh_eligible) {
                capture_state.result.refresh_changed_count++;
            } else {
                capture_state.result.non_refresh_changed_count++;
            }
        }
    } else if (refresh_eligible || pre_climate != CLIMATE_OCEANIC ||
               post_climate != CLIMATE_OCEANIC) {
        capture_state.failed = 1;
        return 0;
    }
    capture_state.post_count++;
    return 1;
}

static int transition_accounting_valid(void) {
    int incoming[CLIMATE_COUNT] = {0};
    int pre_total = 0;
    int post_total = 0;
    int refresh_total = 0;
    int non_refresh_total = 0;
    int refresh_changed = 0;
    int non_refresh_changed = 0;
    int from;
    int to;
    int kind;
    for (from = 0; from < CLIMATE_COUNT; from++) {
        int outgoing = 0;
        if (capture_state.result.pre_climate_count[from] < 0 ||
            capture_state.result.post_climate_count[from] < 0) return 0;
        pre_total += capture_state.result.pre_climate_count[from];
        post_total += capture_state.result.post_climate_count[from];
        for (to = 0; to < CLIMATE_COUNT; to++) {
            for (kind = 0; kind < WORLD_GEN_ARIDITY_TRANSITION_KIND_COUNT;
                 kind++) {
                int count = capture_state.result.transition_count[kind]
                    [from][to];
                if (count < 0) return 0;
                outgoing += count;
                incoming[to] += count;
                if (kind == WORLD_GEN_ARIDITY_TRANSITION_REFRESH) {
                    refresh_total += count;
                    if (from != to) refresh_changed += count;
                } else {
                    non_refresh_total += count;
                    if (from != to) non_refresh_changed += count;
                }
            }
        }
        if (outgoing != capture_state.result.pre_climate_count[from]) {
            return 0;
        }
    }
    if (pre_total != capture_state.result.land_count ||
        post_total != capture_state.result.land_count ||
        refresh_total != capture_state.result.refresh_eligible_count ||
        non_refresh_total != capture_state.result.land_count - refresh_total ||
        refresh_changed != capture_state.result.refresh_changed_count ||
        non_refresh_changed !=
            capture_state.result.non_refresh_changed_count ||
        capture_state.result.non_refresh_changed_count != 0 ||
        refresh_total + non_refresh_total !=
            capture_state.result.transition_accounted_count ||
        capture_state.result.transition_accounted_count !=
            capture_state.result.land_count) return 0;
    for (to = 0; to < CLIMATE_COUNT; to++) {
        int incoming_changed = 0;
        int outgoing_changed = 0;
        if (incoming[to] != capture_state.result.post_climate_count[to]) {
            return 0;
        }
        for (from = 0; from < CLIMATE_COUNT; from++) {
            if (from == to) continue;
            for (kind = 0; kind < WORLD_GEN_ARIDITY_TRANSITION_KIND_COUNT;
                 kind++) {
                incoming_changed += capture_state.result.transition_count[kind]
                    [from][to];
                outgoing_changed += capture_state.result.transition_count[kind]
                    [to][from];
            }
        }
        if (capture_state.result.post_climate_count[to] -
                capture_state.result.pre_climate_count[to] !=
            incoming_changed - outgoing_changed) return 0;
    }
    return 1;
}

int world_gen_aridity_projection_get_result(
    WorldGenAridityProjectionResult *result) {
    int index;
    int total = 0;
    if (!result || !capture_state.enabled || capture_state.failed ||
        !capture_state.context || capture_state.tile_count <= 0 ||
        capture_state.pre_count != capture_state.tile_count ||
        capture_state.post_count != capture_state.tile_count) return 0;
    if (!capture_state.result.complete) {
        for (index = 0; index < WORLD_GEN_ARIDITY_PROJECTION_BIN_COUNT;
             index++) {
            total += capture_state.result.histogram[index];
            capture_state.result.prefix[index] = total;
        }
        if (total != capture_state.result.eligible_count) {
            capture_state.failed = 1;
            return 0;
        }
        if (capture_state.result.climate_input_hash == 0) {
            capture_state.result.climate_input_hash =
                UINT64_C(0xcbf29ce484222325);
        }
        if (!transition_accounting_valid()) {
            capture_state.result.accounting_unexplained_count = 1;
            capture_state.failed = 1;
            return 0;
        }
        capture_state.result.accounting_unexplained_count = 0;
        capture_state.result.accounting_ok = 1;
        capture_state.result.complete = 1;
    }
    *result = capture_state.result;
    return 1;
}
