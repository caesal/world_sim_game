#include "technology.h"

#include "core/game_state.h"
#include "data/game_tables.h"
#include "sim/disorder.h"
#include "sim/population.h"
#include "sim/simulation.h"

static int tech_progress_remainder[MAX_CIVS];

static int resource_score_for_civ(int civ_id) {
    CountrySummary summary = summarize_country(civ_id);
    return summary.food + summary.water + summary.money + summary.minerals + summary.pop_capacity;
}

static int normalized_stage(int civ_id) {
    if (civ_id < 0 || civ_id >= civ_count) return 0;
    return clamp(civs[civ_id].tech_stage, 0, 10);
}

static int required_years_for_civ(int civ_id) {
    Civilization *civ = &civs[civ_id];
    int resources = resource_score_for_civ(civ_id);
    int pressure = population_pressure_for_civ(civ_id);
    int stage = clamp(civ->tech_stage, 0, 10);
    int years = 130 - (civ->innovation - 5) * 4;

    if (resources >= 36) years -= 10;
    else if (resources < 24) years += 12;
    if (pressure < 50) years -= 5;
    else if (pressure > 115) years += 20;
    else if (pressure > 80) years += 10;
    if (stage >= 8) years = years * 90 / 100;
    return clamp(years, 95, 160);
}

static void bonus_summary_for_stage(int stage, TechnologyBonusSummary *out) {
    if (!out) return;
    stage = clamp(stage, 0, 10);
    out->expansion_percent = stage >= 2 ? 120 : (stage >= 1 ? 110 : 100);
    out->resource_percent = stage >= 5 ? 120 : (stage >= 4 ? 110 : (stage >= 3 ? 105 : 100));
    out->technology_percent = stage >= 8 ? 110 : 100;
    out->deep_sea_stability = stage >= 9 ? 99 : (stage >= 8 ? 90 : (stage >= 7 ? 80 : (stage >= 6 ? 50 : 0)));
    out->defense_army_percent = stage >= 8 ? 120 : 100;
    out->battle_chance_bonus = stage >= 9 ? 15 : 0;
    out->vassal_annexation_unlocked = stage >= 10;
}

void technology_initialize_civ(int civ_id) {
    if (civ_id < 0 || civ_id >= MAX_CIVS) return;
    civs[civ_id].tech_stage = 0;
    civs[civ_id].tech_progress = 0;
    tech_progress_remainder[civ_id] = 0;
    civs[civ_id].deep_sea_route_unlocked_event_done = 0;
}

void technology_update_month(void) {
    int i;

    for (i = 0; i < civ_count; i++) {
        Civilization *civ = &civs[i];
        int required;
        if (!civ->alive || civ->tech_stage >= 10) continue;
        required = required_years_for_civ(i) * 12;
        tech_progress_remainder[i] += disorder_technology_percent(civ->disorder);
        while (tech_progress_remainder[i] >= 100) {
            civ->tech_progress++;
            tech_progress_remainder[i] -= 100;
        }
        if (civ->tech_progress >= required) {
            civ->tech_stage = clamp(civ->tech_stage + 1, 0, 10);
            civ->tech_progress = 0;
            tech_progress_remainder[i] = 0;
            world_invalidate_population_cache();
        }
    }
}

int technology_years_to_next(int civ_id) {
    return (technology_months_to_next(civ_id) + 11) / 12;
}

int technology_months_to_next(int civ_id) {
    int remaining;
    int monthly;
    if (civ_id < 0 || civ_id >= civ_count || civs[civ_id].tech_stage >= 10) return 0;
    remaining = required_years_for_civ(civ_id) * 12 - civs[civ_id].tech_progress;
    remaining = remaining * 100 - tech_progress_remainder[civ_id];
    monthly = max(1, disorder_technology_percent(civs[civ_id].disorder));
    return max(0, (remaining + monthly - 1) / monthly);
}

int technology_expansion_percent(int civ_id) {
    int stage = normalized_stage(civ_id);
    if (stage >= 2) return 120;
    if (stage >= 1) return 110;
    return 100;
}

int technology_resource_percent(int civ_id) {
    int stage = normalized_stage(civ_id);
    if (stage >= 5) return 120;
    if (stage >= 4) return 110;
    if (stage >= 3) return 105;
    return 100;
}

int technology_progress_percent(int civ_id) {
    return normalized_stage(civ_id) >= 8 ? 110 : 100;
}

int technology_stage_progress_percent(int civ_id) {
    int required;
    if (civ_id < 0 || civ_id >= civ_count || civs[civ_id].tech_stage >= 10) return 100;
    required = max(1, required_years_for_civ(civ_id) * 12);
    return clamp((civs[civ_id].tech_progress * 100 + tech_progress_remainder[civ_id]) / required, 0, 99);
}

int technology_deep_sea_unlocked(int civ_id) {
    return normalized_stage(civ_id) >= 6;
}

int technology_deep_sea_stability(int civ_id) {
    TechnologyBonusSummary summary;
    technology_current_bonus_summary(civ_id, &summary);
    return summary.deep_sea_stability;
}

int technology_deep_sea_death_rate(int civ_id) {
    if (!technology_deep_sea_unlocked(civ_id)) return 100;
    return 100 - technology_deep_sea_stability(civ_id);
}

int technology_deep_sea_plague_percent(int civ_id) {
    int stage = normalized_stage(civ_id);
    if (stage >= 10) return 0;
    if (stage >= 9) return 5;
    if (stage >= 8) return 15;
    if (stage >= 7) return 35;
    if (stage >= 6) return 50;
    return 0;
}

int technology_defense_army_percent(int civ_id) {
    TechnologyBonusSummary summary;
    technology_current_bonus_summary(civ_id, &summary);
    return summary.defense_army_percent;
}

int technology_battle_chance_bonus(int civ_id) {
    TechnologyBonusSummary summary;
    technology_current_bonus_summary(civ_id, &summary);
    return summary.battle_chance_bonus;
}

void technology_current_bonus_summary(int civ_id, TechnologyBonusSummary *out) {
    bonus_summary_for_stage(normalized_stage(civ_id), out);
}

void technology_next_bonus_summary(int civ_id, TechnologyBonusSummary *out) {
    int stage = normalized_stage(civ_id);
    bonus_summary_for_stage(stage >= 10 ? 10 : stage + 1, out);
}

void technology_bonus_for_stage(int stage, TechnologyBonusSummary *out) {
    bonus_summary_for_stage(stage, out);
}

void technology_bonus_delta(int from_stage, int to_stage, TechnologyBonusSummary *out) {
    TechnologyBonusSummary from;
    TechnologyBonusSummary to;
    if (!out) return;
    bonus_summary_for_stage(from_stage, &from);
    bonus_summary_for_stage(to_stage, &to);
    out->expansion_percent = to.expansion_percent - from.expansion_percent;
    out->resource_percent = to.resource_percent - from.resource_percent;
    out->technology_percent = to.technology_percent - from.technology_percent;
    out->deep_sea_stability = to.deep_sea_stability - from.deep_sea_stability;
    out->defense_army_percent = to.defense_army_percent - from.defense_army_percent;
    out->battle_chance_bonus = to.battle_chance_bonus - from.battle_chance_bonus;
    out->vassal_annexation_unlocked = to.vassal_annexation_unlocked && !from.vassal_annexation_unlocked;
}

const char *technology_stage_name(int stage, int language) {
    static const LocalizedText stage_zero = {"Stage 0 - Tribal Age", "阶段 0 - 部族时代"};
    if (stage <= 0) return localized_text(stage_zero, language);
    stage = clamp(stage, 1, 10);
    return localized_text(TECHNOLOGY_STAGE_RULES[stage - 1].name, language);
}

const char *technology_stage_effect(int stage, int language) {
    static const LocalizedText stage_zero = {"No technology bonus yet.", "暂无科技加成。"};
    if (stage <= 0) return localized_text(stage_zero, language);
    stage = clamp(stage, 1, 10);
    return localized_text(TECHNOLOGY_STAGE_RULES[stage - 1].effect, language);
}
