#include "core/render_snapshot_civs.h"

#include "core/render_snapshot_profile.h"
#include "sim/civilization_slots.h"
#include "sim/collapse.h"
#include "sim/decision_snapshot.h"
#include "sim/diplomacy.h"
#include "sim/disorder.h"
#include "sim/population.h"
#include "sim/simulation.h"
#include "sim/technology.h"
#include "sim/vassal.h"
#include "sim/war.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>

static int last_decision_cached_count;
static int last_decision_stale_count;
static int last_decision_fallback_count;

static void record_phase(RenderSnapshotCivProfilePhase phase, DWORD start) {
    render_snapshot_profile_record_civ_phase(phase, (int)(GetTickCount() - start));
}

static int same_identity(const RenderSnapshot *snapshot, const SnapshotCiv *dst,
                         const Civilization *src, int id) {
    return snapshot->revision != 0 && dst->id == id && dst->uid == src->uid;
}

static void bind_decision_strings(SnapshotCiv *dst) {
    dst->decision.main_intent = dst->main_intent;
    dst->decision.expansion_reason = dst->decision_expansion_reason;
    dst->decision.war_reason = dst->decision_war_reason;
}

static void copy_decision_strings(SnapshotCiv *dst, const DecisionSnapshot *src) {
    snprintf(dst->main_intent, sizeof(dst->main_intent), "%s",
             src->main_intent ? src->main_intent : "");
    snprintf(dst->decision_expansion_reason, sizeof(dst->decision_expansion_reason), "%s",
             src->expansion_reason ? src->expansion_reason : "");
    snprintf(dst->decision_war_reason, sizeof(dst->decision_war_reason), "%s",
             src->war_reason ? src->war_reason : "");
}

static void reset_stale_fields(SnapshotCiv *dst) {
    memset(&dst->summary, 0, sizeof(dst->summary));
    memset(&dst->population_summary, 0, sizeof(dst->population_summary));
    memset(&dst->decision, 0, sizeof(dst->decision));
    dst->decision_expansion_weight = 0;
    dst->decision_war_weight = 0;
    dst->decision_stability_weight = 0;
    dst->decision_next_expansion_months = 0;
    dst->tech_stage_progress_percent = 0;
    dst->tech_months_to_next = 0;
    dst->tech_required_months = 0;
    dst->collapse_can_trigger = 0;
    dst->collapse_block_reason = 0;
    dst->current_soldiers = 0;
    dst->war_available_reserve = 0;
    dst->vassal_callable_soldiers = 0;
    dst->vassal_resource_tribute = 0;
    dst->vassal_support_used = 0;
    dst->main_intent[0] = '\0';
    dst->decision_expansion_reason[0] = '\0';
    dst->decision_war_reason[0] = '\0';
    bind_decision_strings(dst);
}

static void copy_raw_fields(SnapshotCiv *dst, Civilization *src, int i) {
    dst->alive = src->alive; dst->id = i; dst->uid = src->uid;
    dst->color = src->color; dst->symbol = src->symbol;
    dst->population = src->population; dst->army = src->military;
    dst->aggression = src->aggression; dst->expansion = src->expansion;
    dst->defense = src->defense; dst->culture = src->culture;
    dst->governance = src->governance; dst->cohesion = src->cohesion;
    dst->production = src->production; dst->military = src->military;
    dst->commerce = src->commerce; dst->logistics = src->logistics;
    dst->innovation = src->innovation; dst->adaptation = src->adaptation;
    dst->tech_stage = src->tech_stage; dst->tech_progress = src->tech_progress;
    dst->tech_expansion_percent = technology_expansion_percent(i);
    dst->tech_resource_percent = technology_resource_percent(i);
    dst->tech_progress_percent = technology_progress_percent(i);
    dst->disorder = src->disorder; dst->disorder_resource = src->disorder_resource;
    dst->disorder_plague = src->disorder_plague; dst->disorder_migration = src->disorder_migration;
    dst->disorder_stability = src->disorder_stability; dst->disorder_wartime = disorder_wartime_pressure(i);
    dst->disorder_last_pressure = src->disorder_last_pressure;
    dst->disorder_last_recovery = src->disorder_last_recovery; dst->disorder_last_net = src->disorder_last_net;
    dst->disorder_last_pressure_x10 = src->disorder_last_pressure_x10;
    dst->disorder_last_recovery_x10 = src->disorder_last_recovery_x10;
    dst->disorder_last_net_x10 = src->disorder_last_net_x10;
    dst->disorder_last_base_recovery_x10 = src->disorder_last_base_recovery_x10;
    dst->disorder_last_governance_recovery_x10 = src->disorder_last_governance_recovery_x10;
    dst->disorder_last_cohesion_recovery_x10 = src->disorder_last_cohesion_recovery_x10;
    dst->disorder_last_peace_recovery_x10 = src->disorder_last_peace_recovery_x10;
    dst->disorder_last_condition_recovery_x10 = src->disorder_last_condition_recovery_x10;
    dst->disorder_last_plague_decay = src->disorder_last_plague_decay;
    dst->disorder_last_war_decay = src->disorder_last_war_decay;
    dst->disorder_last_migration_decay = src->disorder_last_migration_decay;
    dst->disorder_last_wartime_pressure_x10 = disorder_last_wartime_pressure_x10(i);
    dst->disorder_last_wartime_decay_x10 = disorder_last_wartime_decay_x10(i);
    dst->collapse_grace_months = src->collapse_grace_months;
    dst->plague_random_immunity_months = src->plague_random_immunity_months;
    dst->war_active = war_active_for_civ(i); dst->war_deployed_soldiers = war_deployed_soldiers_for_civ(i);
    dst->war_front_count = war_front_count_for_civ(i);
    dst->vassal_governance_disorder = vassal_governance_disorder(i);
    dst->collapse_last_reason[0] = '\0';
    snprintf(dst->collapse_last_reason, sizeof(dst->collapse_last_reason), "%s", collapse_last_reason(i));
    dst->capital_city = src->capital_city; dst->overlord = vassal_overlord(i);
    dst->vassal_annex_threshold_years = dst->overlord >= 0 ? vassal_annex_threshold_years(dst->overlord) : 0;
    dst->vassal_annex_remaining_years = dst->overlord >= 0 ?
        vassal_annex_remaining_years(dst->overlord, diplomacy_relation(dst->overlord, i).vassal_years) : 0;
    dst->vassal_support_casualties = vassal_support_casualties(i);
    dst->vassal_count = vassal_direct_count(i);
    dst->name_id = src->name_id; dst->heritage = src->heritage;
}

static int copy_cached_country_summary(SnapshotCiv *dst, int i) {
    CountrySummary country;
    if (summarize_country_cached(i, &country)) {
        dst->summary = country;
        dst->vassal_resource_tribute = (country.food + country.livestock + country.wood +
                                        country.stone + country.minerals + country.water) * 40 / 100;
        return 1;
    }
    return 0;
}

static int copy_cached_population_summary(SnapshotCiv *dst, int i) {
    PopulationSummary population;
    if (population_country_summary_cached(i, &population)) {
        dst->population_summary = population;
        dst->current_soldiers = war_current_soldiers_for_civ(i);
        dst->war_available_reserve = war_available_reserve_for_civ(i);
        dst->vassal_callable_soldiers = vassal_callable_soldiers(i);
        dst->vassal_support_used = dst->overlord >= 0 ? vassal_support_used_by_overlord(dst->overlord, i) : 0;
        return 1;
    }
    return 0;
}

static void refresh_decision_scalars(SnapshotCiv *dst) {
    bind_decision_strings(dst);
    dst->decision_expansion_weight = dst->decision.expansion_weight;
    dst->decision_war_weight = dst->decision.war_weight;
    dst->decision_stability_weight = dst->decision.stability_weight;
    dst->decision_next_expansion_months = dst->decision.next_expansion_months;
}

static int has_snapshot_decision(const SnapshotCiv *dst) {
    return dst && dst->main_intent[0] != '\0';
}

static void copy_cached_decision(SnapshotCiv *dst, const DecisionSnapshot *decision) {
    dst->decision = *decision;
    copy_decision_strings(dst, decision);
    refresh_decision_scalars(dst);
}

static void write_fallback_decision(SnapshotCiv *dst) {
    memset(&dst->decision, 0, sizeof(dst->decision));
    snprintf(dst->main_intent, sizeof(dst->main_intent), "%s", "Waiting");
    snprintf(dst->decision_expansion_reason, sizeof(dst->decision_expansion_reason), "%s", "Waiting");
    dst->decision_war_reason[0] = '\0';
    bind_decision_strings(dst);
    refresh_decision_scalars(dst);
}

static void copy_decision_state(SnapshotCiv *dst, int civ_id, int stable) {
    DecisionSnapshot decision;

    if (decision_snapshot_cached(civ_id, &decision)) {
        copy_cached_decision(dst, &decision);
        last_decision_cached_count++;
    } else if (stable && has_snapshot_decision(dst)) {
        bind_decision_strings(dst);
        refresh_decision_scalars(dst);
        last_decision_stale_count++;
    } else {
        write_fallback_decision(dst);
        last_decision_fallback_count++;
    }
}

static void copy_names(SnapshotCiv *dst, int i) {
    snprintf(dst->name_en, sizeof(dst->name_en), "%s", civilization_display_name_for_language(i, 0));
    snprintf(dst->name_zh, sizeof(dst->name_zh), "%s", civilization_display_name_for_language(i, 1));
}

void render_snapshot_copy_civs_locked(RenderSnapshot *snapshot) {
    int i;
    snapshot->civ_count = clamp(civ_count, 0, MAX_CIVS);
    snapshot->civ_independent_alive_count = 0;
    last_decision_cached_count = 0;
    last_decision_stale_count = 0;
    last_decision_fallback_count = 0;
    for (i = 0; i < snapshot->civ_count; i++) {
        SnapshotCiv *dst = &snapshot->civs[i];
        Civilization *src = &civs[i];
        int stable = same_identity(snapshot, dst, src, i);
        int country_ready;
        int population_ready;
        DWORD start = GetTickCount();
        if (!stable) reset_stale_fields(dst);
        copy_raw_fields(dst, src, i);
        if (src->alive && dst->overlord < 0) snapshot->civ_independent_alive_count++;
        record_phase(SNAPSHOT_CIV_PROFILE_RAW, start);
        start = GetTickCount();
        country_ready = copy_cached_country_summary(dst, i);
        record_phase(SNAPSHOT_CIV_PROFILE_COUNTRY, start);
        start = GetTickCount();
        population_ready = copy_cached_population_summary(dst, i);
        if (country_ready && population_ready) {
            dst->tech_required_months = technology_required_months_for_civ(i);
            dst->tech_months_to_next = technology_months_to_next(i);
            dst->tech_stage_progress_percent = technology_stage_progress_percent(i);
        }
        record_phase(SNAPSHOT_CIV_PROFILE_POPULATION, start);
        start = GetTickCount();
        copy_decision_state(dst, i, stable);
        record_phase(SNAPSHOT_CIV_PROFILE_DECISION, start);
        render_snapshot_profile_record_civ_phase(SNAPSHOT_CIV_PROFILE_EXPANSION, 0);
        render_snapshot_profile_record_civ_phase(SNAPSHOT_CIV_PROFILE_MARITIME, 0);
        start = GetTickCount();
        copy_names(dst, i);
        record_phase(SNAPSHOT_CIV_PROFILE_NAMES, start);
    }
}

int render_snapshot_civ_decision_cached_count(void) { return last_decision_cached_count; }
int render_snapshot_civ_decision_stale_count(void) { return last_decision_stale_count; }
int render_snapshot_civ_decision_fallback_count(void) { return last_decision_fallback_count; }
