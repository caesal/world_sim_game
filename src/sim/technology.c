#include "technology.h"

#include "core/game_state.h"
#include "data/game_tables.h"
#include "sim/population.h"
#include "sim/simulation.h"

static int resource_score_for_civ(int civ_id) {
    CountrySummary summary = summarize_country(civ_id);
    return summary.food + summary.water + summary.money + summary.minerals + summary.pop_capacity;
}

static int required_years_for_civ(int civ_id) {
    Civilization *civ = &civs[civ_id];
    int resources = resource_score_for_civ(civ_id);
    int pressure = population_pressure_for_civ(civ_id);
    int years = 130 - (civ->innovation - 5) * 4;

    if (resources >= 36) years -= 10;
    else if (resources < 24) years += 12;
    if (pressure < 50) years -= 5;
    else if (pressure > 115) years += 20;
    else if (pressure > 80) years += 10;
    if (civ->tech_stage >= 8) years = years * 90 / 100;
    return clamp(years, 95, 160);
}

void technology_initialize_civ(int civ_id) {
    if (civ_id < 0 || civ_id >= civ_count) return;
    civs[civ_id].tech_stage = 1;
    civs[civ_id].tech_progress = 0;
    civs[civ_id].deep_sea_route_unlocked_event_done = 0;
}

void technology_update_month(void) {
    int i;

    for (i = 0; i < civ_count; i++) {
        Civilization *civ = &civs[i];
        int required;
        if (!civ->alive || civ->tech_stage >= 10) continue;
        required = required_years_for_civ(i) * 12;
        civ->tech_progress++;
        if (civ->tech_progress >= required) {
            civ->tech_stage = clamp(civ->tech_stage + 1, 1, 10);
            civ->tech_progress = 0;
            world_invalidate_population_cache();
        }
    }
}

int technology_years_to_next(int civ_id) {
    int remaining;
    if (civ_id < 0 || civ_id >= civ_count || civs[civ_id].tech_stage >= 10) return 0;
    remaining = required_years_for_civ(civ_id) * 12 - civs[civ_id].tech_progress;
    return max(0, (remaining + 11) / 12);
}

int technology_expansion_percent(int civ_id) {
    int stage = civ_id >= 0 && civ_id < civ_count ? civs[civ_id].tech_stage : 1;
    if (stage >= 2) return 120;
    if (stage >= 1) return 110;
    return 100;
}

int technology_resource_percent(int civ_id) {
    int stage = civ_id >= 0 && civ_id < civ_count ? civs[civ_id].tech_stage : 1;
    if (stage >= 5) return 120;
    if (stage >= 4) return 110;
    if (stage >= 3) return 105;
    return 100;
}

int technology_deep_sea_stability(int civ_id) {
    int stage = civ_id >= 0 && civ_id < civ_count ? civs[civ_id].tech_stage : 1;
    if (stage >= 9) return 99;
    if (stage >= 8) return 90;
    if (stage >= 7) return 80;
    if (stage >= 6) return 50;
    return 0;
}

int technology_defense_army_percent(int civ_id) {
    int stage = civ_id >= 0 && civ_id < civ_count ? civs[civ_id].tech_stage : 1;
    return stage >= 8 ? 120 : 100;
}

int technology_battle_chance_bonus(int civ_id) {
    int stage = civ_id >= 0 && civ_id < civ_count ? civs[civ_id].tech_stage : 1;
    return stage >= 9 ? 15 : 0;
}

const char *technology_stage_name(int stage, int language) {
    stage = clamp(stage, 1, 10);
    return localized_text(TECHNOLOGY_STAGE_RULES[stage - 1].name, language);
}

const char *technology_stage_effect(int stage, int language) {
    stage = clamp(stage, 1, 10);
    return localized_text(TECHNOLOGY_STAGE_RULES[stage - 1].effect, language);
}
