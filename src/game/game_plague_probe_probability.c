#include "game/game_plague_probe_internal.h"

#include "core/game_types.h"
#include "core/render_snapshot.h"
#include "core/state_lock.h"
#include "io/map_save_plague.h"
#include "sim/plague_episode.h"
#include "sim/plague_probability.h"
#include "sim/plague_rules.h"
#include "sim/plague_state.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    char tag[8];
    int version;
    int item_size;
    int count;
    int aux_a;
    int aux_b;
} PlagueProbeSaveBlockHeader;

int game_plague_probe_live_save_roundtrip(void) {
    PlagueModelState before;
    PlagueModelState after;
    FILE *file;
    int ok;

    state_write_lock();
    plague_state_copy(&before);
    file = tmpfile();
    ok = file && map_save_plague_write(file) && fseek(file, 0, SEEK_SET) == 0 &&
         map_save_plague_read(file);
    if (file) fclose(file);
    plague_state_copy(&after);
    state_write_unlock();
    if (ok) render_snapshot_publish_from_live_state();
    return ok && memcmp(&before, &after, sizeof(before)) == 0;
}

static int distribution_value(const PlagueProbabilityDistribution *probabilities,
                              PlagueProbabilityBucket bucket) {
    switch (bucket) {
        case PLAGUE_PROBABILITY_NO_PLAGUE: return probabilities->no_plague;
        case PLAGUE_PROBABILITY_SMALL: return probabilities->small;
        case PLAGUE_PROBABILITY_MEDIUM: return probabilities->medium;
        case PLAGUE_PROBABILITY_LARGE: return probabilities->large;
        default: return -1;
    }
}

static int distribution_is(const PlagueProbabilityDistribution *probabilities,
                           int no_plague, int small, int medium, int large) {
    return probabilities && probabilities->no_plague == no_plague &&
           probabilities->small == small && probabilities->medium == medium &&
           probabilities->large == large;
}

static void begin_probe_episode(int episode_id) {
    PlagueEpisodeState episode;
    memset(&episode, 0, sizeof(episode));
    episode.active = 1;
    episode.episode_id = episode_id;
    episode.size = PLAGUE_SIZE_SMALL;
    episode.severity = 2;
    episode.start_month = 240;
    episode.spores_initial = 1;
    episode.spores_remaining = 1;
    plague_state_begin_episode(&episode);
}

static void check_default_rolls(PlagueProbeContext *context) {
    PlagueProbabilityDistribution defaults;
    int roll;
    int ok = 1;
    plague_probability_defaults(&defaults);
    for (roll = 0; roll < 100; roll++) {
        PlagueSize expected = roll < 50 ? PLAGUE_SIZE_NONE :
                              roll < 75 ? PLAGUE_SIZE_SMALL :
                              roll < 90 ? PLAGUE_SIZE_MEDIUM : PLAGUE_SIZE_LARGE;
        if (plague_rules_size_from_roll(roll) != expected ||
            plague_rules_size_from_distribution(&defaults, roll) != expected) ok = 0;
    }
    plague_probe_check(context, "probability", "default_roll_boundaries_exact", ok &&
        distribution_is(&defaults, 50, 25, 15, 10),
        "mapping=0-49:none/50-74:small/75-89:medium/90-99:large");
}

static void check_new_world_defaults(PlagueProbeContext *context) {
    const PlagueModelState *model;
    plague_state_reset();
    model = plague_state_get();
    plague_probe_check(context, "probability", "new_world_defaults",
        distribution_is(&model->effective_probabilities, 50, 25, 15, 10) &&
        distribution_is(&model->pending_probabilities, 50, 25, 15, 10) &&
        !model->pending_probabilities_valid,
        "effective=50/25/15/10 pending_valid=0");
}

static void check_extreme_rolls(PlagueProbeContext *context) {
    const PlagueProbabilityDistribution distributions[4] = {
        {100, 0, 0, 0}, {0, 100, 0, 0},
        {0, 0, 100, 0}, {0, 0, 0, 100}
    };
    const PlagueSize expected[4] = {
        PLAGUE_SIZE_NONE, PLAGUE_SIZE_SMALL,
        PLAGUE_SIZE_MEDIUM, PLAGUE_SIZE_LARGE
    };
    int i;
    int roll;
    int ok = 1;
    for (i = 0; i < 4; i++) {
        for (roll = 0; roll < 100; roll++) {
            if (plague_rules_size_from_distribution(&distributions[i], roll) !=
                expected[i]) ok = 0;
        }
    }
    plague_probe_check(context, "probability", "all_single_bucket_endpoints", ok,
        "distributions=100/0/0/0,0/100/0/0,0/0/100/0,0/0/0/100");
}

static void check_single_rng_call(PlagueProbeContext *context) {
    int seed;
    int ok = 1;
    memset(cities, 0, sizeof(cities));
    memset(civs, 0, sizeof(civs));
    city_count = 0;
    civ_count = 0;
    for (seed = 0; seed < 256; seed++) {
        int expected_next;
        int actual_next;
        srand((unsigned int)seed);
        (void)rnd(100);
        expected_next = rnd(1000000);
        srand((unsigned int)seed);
        plague_state_reset();
        if (plague_episode_try_scheduled_start(240) != 0) ok = 0;
        actual_next = rnd(1000000);
        if (actual_next != expected_next) ok = 0;
    }
    plague_probe_check(context, "probability", "eligible_check_consumes_one_size_roll",
        ok, "default_distribution seed_sweep=256 next_rng_matches");
}

static void check_linked_rounding(PlagueProbeContext *context) {
    PlagueProbabilityDistribution defaults;
    PlagueProbabilityDistribution all_none = {100, 0, 0, 0};
    PlagueProbabilityDistribution equal_remainders = {97, 1, 1, 1};
    PlagueProbabilityDistribution adjusted;
    int ok;
    plague_probability_defaults(&defaults);
    ok = plague_probability_adjust_linked(&defaults,
            PLAGUE_PROBABILITY_NO_PLAGUE, 49, &adjusted) &&
         distribution_is(&adjusted, 49, 26, 15, 10) &&
         plague_probability_adjust_linked(&all_none,
            PLAGUE_PROBABILITY_NO_PLAGUE, 1, &adjusted) &&
         distribution_is(&adjusted, 1, 49, 30, 20) &&
         plague_probability_adjust_linked(&all_none,
            PLAGUE_PROBABILITY_NO_PLAGUE, 0, &adjusted) &&
         distribution_is(&adjusted, 0, 50, 30, 20) &&
         plague_probability_adjust_linked(&defaults,
            PLAGUE_PROBABILITY_SMALL, 80, &adjusted) &&
         distribution_is(&adjusted, 0, 80, 12, 8) &&
         plague_probability_adjust_linked(&defaults,
            PLAGUE_PROBABILITY_MEDIUM, 68, &adjusted) &&
         distribution_is(&adjusted, 0, 23, 68, 9) &&
         plague_probability_adjust_linked(&defaults,
            PLAGUE_PROBABILITY_LARGE, 0, &adjusted) &&
         distribution_is(&adjusted, 60, 25, 15, 0) &&
         plague_probability_adjust_linked(&equal_remainders,
            PLAGUE_PROBABILITY_NO_PLAGUE, 98, &adjusted) &&
         distribution_is(&adjusted, 98, 1, 1, 0);
    plague_probe_check(context, "probability", "linked_rounding_stable_order", ok,
        "largest_remainder_ties=small_then_medium_then_large fallback=5:3:2");
}

static void check_all_linked_endpoints(PlagueProbeContext *context) {
    PlagueProbabilityDistribution current;
    PlagueProbabilityDistribution adjusted;
    int no_plague;
    int small;
    int medium;
    int bucket;
    int endpoint;
    int cases = 0;
    int ok = 1;
    for (no_plague = 0; no_plague <= 100; no_plague++) {
        for (small = 0; small <= 100 - no_plague; small++) {
            for (medium = 0; medium <= 100 - no_plague - small; medium++) {
                current.no_plague = no_plague;
                current.small = small;
                current.medium = medium;
                current.large = 100 - no_plague - small - medium;
                for (bucket = 0; bucket < PLAGUE_PROBABILITY_COUNT; bucket++) {
                    for (endpoint = 0; endpoint <= 100; endpoint += 100) {
                        cases++;
                        if (!plague_probability_adjust_linked(
                                &current, (PlagueProbabilityBucket)bucket,
                                endpoint, &adjusted) ||
                            !plague_probability_validate(&adjusted) ||
                            distribution_value(&adjusted,
                                (PlagueProbabilityBucket)bucket) != endpoint) ok = 0;
                    }
                }
            }
        }
    }
    plague_probe_check(context, "probability", "all_slider_endpoint_invariants", ok,
        "cases=%d every_bucket=0/100 sum=100 nonnegative", cases);
}

static void check_apply_lifecycle(PlagueProbeContext *context) {
    const PlagueProbabilityDistribution first = {40, 30, 20, 10};
    const PlagueProbabilityDistribution second = {10, 20, 30, 40};
    PlagueEpisodeHistory ended;
    PlagueEpisodeState before_episode;
    int immediate;
    int active_pending;
    int replacement;
    int cancellation;
    int promotion;
    plague_state_reset();
    immediate = distribution_is(&plague_state_get()->effective_probabilities,
                                 50, 25, 15, 10) &&
                !plague_state_get()->pending_probabilities_valid &&
                plague_state_apply_probabilities(&first) &&
                plague_probability_equal(&plague_state_get()->effective_probabilities,
                                          &first) &&
                !plague_state_get()->pending_probabilities_valid;
    plague_state_reset();
    begin_probe_episode(201);
    before_episode = plague_state_get()->episode;
    active_pending = plague_state_apply_probabilities(&first) &&
        plague_probability_equal(&plague_state_get()->effective_probabilities,
                                 &(PlagueProbabilityDistribution){50, 25, 15, 10}) &&
        plague_probability_equal(&plague_state_get()->pending_probabilities, &first) &&
        plague_state_get()->pending_probabilities_valid &&
        memcmp(&before_episode, &plague_state_get()->episode,
               sizeof(before_episode)) == 0;
    replacement = plague_state_apply_probabilities(&second) &&
        plague_probability_equal(&plague_state_get()->pending_probabilities, &second) &&
        plague_state_get()->pending_probabilities_valid;
    cancellation = plague_state_apply_probabilities(
        &plague_state_get()->effective_probabilities) &&
        !plague_state_get()->pending_probabilities_valid &&
        plague_state_apply_probabilities(&second) &&
        plague_state_get()->pending_probabilities_valid;
    memset(&ended, 0, sizeof(ended));
    promotion = plague_state_finish_episode(300, &ended) &&
        plague_probability_equal(&plague_state_get()->effective_probabilities, &second) &&
        !plague_state_get()->pending_probabilities_valid &&
        !plague_probability_model_promote_pending(plague_state_mutable());
    plague_probe_check(context, "probability", "inactive_apply_is_immediate",
        immediate, "effective=40/30/20/10 pending_valid=0");
    plague_probe_check(context, "probability", "active_apply_is_pending",
        active_pending, "effective_unchanged=1 episode_unchanged=1");
    plague_probe_check(context, "probability", "active_pending_replacement",
        replacement, "latest_pending=10/20/30/40");
    plague_probe_check(context, "probability", "apply_effective_clears_pending",
        cancellation, "no_unnecessary_pending=1 then_reapplied_for_end_test=1");
    plague_probe_check(context, "probability", "pending_promotes_exactly_once",
        promotion, "effective=10/20/30/40 second_promotion=0");
}

static void check_loaded_pending_lifecycle(PlagueProbeContext *context) {
    const PlagueProbabilityDistribution pending = {5, 15, 30, 50};
    PlagueModelState saved;
    PlagueEpisodeHistory ended;
    int ok;
    plague_state_reset();
    begin_probe_episode(202);
    plague_state_apply_probabilities(&pending);
    plague_state_copy(&saved);
    plague_state_reset();
    ok = plague_state_restore(&saved) && plague_state_get()->episode.active &&
         plague_state_get()->pending_probabilities_valid &&
         plague_state_finish_episode(301, &ended) &&
         plague_probability_equal(&plague_state_get()->effective_probabilities,
                                  &pending) &&
         !plague_state_get()->pending_probabilities_valid;
    plague_probe_check(context, "probability", "loaded_active_pending_promotes_on_end",
        ok, "pending=5/15/30/50 promotion_after_restore=once");
}

static int write_raw_plague_block(FILE *file, int version,
                                  const PlagueModelState *state) {
    PlagueProbeSaveBlockHeader header;
    if (!file || !state) return 0;
    memset(&header, 0, sizeof(header));
    memcpy(header.tag, "PLG19", 6);
    header.version = version;
    header.item_size = (int)sizeof(*state);
    header.count = 1;
    return fwrite(&header, sizeof(header), 1, file) == 1 &&
           fwrite(state, sizeof(*state), 1, file) == 1;
}

static void check_probability_save_contract(PlagueProbeContext *context) {
    const PlagueProbabilityDistribution pending = {12, 34, 23, 31};
    PlagueModelState before;
    PlagueModelState after;
    PlagueModelState invalid;
    FILE *file;
    int write_ok = 0;
    int read_ok = 0;
    int invalid_effective_rejected = 0;
    int invalid_pending_rejected = 0;
    int v1_rejected = 0;
    plague_state_reset();
    begin_probe_episode(203);
    plague_state_apply_probabilities(&pending);
    plague_state_copy(&before);
    file = tmpfile();
    if (file) {
        write_ok = map_save_plague_write(file);
        plague_state_reset();
        rewind(file);
        read_ok = map_save_plague_read(file);
        fclose(file);
    }
    plague_state_copy(&after);
    invalid = before;
    invalid.effective_probabilities.no_plague--;
    file = tmpfile();
    if (file) {
        if (write_raw_plague_block(file, MAP_SAVE_PLAGUE_BLOCK_VERSION, &invalid)) {
            rewind(file);
            invalid_effective_rejected = !map_save_plague_read(file);
        }
        fclose(file);
    }
    invalid = before;
    invalid.pending_probabilities.large++;
    file = tmpfile();
    if (file) {
        if (write_raw_plague_block(file, MAP_SAVE_PLAGUE_BLOCK_VERSION, &invalid)) {
            rewind(file);
            invalid_pending_rejected = !map_save_plague_read(file);
        }
        fclose(file);
    }
    file = tmpfile();
    if (file) {
        if (write_raw_plague_block(file, 1, &before)) {
            rewind(file);
            v1_rejected = !map_save_plague_read(file);
        }
        fclose(file);
    }
    plague_probe_check(context, "save", "plg19_v2_effective_pending_roundtrip",
        write_ok && read_ok && memcmp(&before, &after, sizeof(before)) == 0,
        "write=%d read=%d pending_valid=%d", write_ok, read_ok,
        after.pending_probabilities_valid);
    plague_probe_check(context, "save", "invalid_probability_payload_rejected",
        invalid_effective_rejected && invalid_pending_rejected,
        "effective_total=%d pending_total=%d",
        invalid_effective_rejected, invalid_pending_rejected);
    plague_probe_check(context, "save", "plg19_block_version_1_rejected",
        v1_rejected, "expected_block_version=2 legacy_block_version=1");
}

void plague_probe_run_probability(PlagueProbeContext *context) {
    check_default_rolls(context);
    check_new_world_defaults(context);
    check_extreme_rolls(context);
    check_single_rng_call(context);
    check_linked_rounding(context);
    check_all_linked_endpoints(context);
    check_apply_lifecycle(context);
    check_loaded_pending_lifecycle(context);
    check_probability_save_contract(context);
}
