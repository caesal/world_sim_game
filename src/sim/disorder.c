#include "disorder.h"

#include "core/game_state.h"
#include "sim/collapse.h"
#include "sim/population.h"
#include "sim/simulation.h"

#include <stdio.h>

static int last_debuff_percent[MAX_CIVS];

static int disorder_soft_effect_percent(int disorder) {
    disorder = clamp(disorder, 0, 100);
    if (disorder <= 25) return 100;
    if (disorder >= 74) return 80;
    return 95 - (disorder - 26) * 15 / 48;
}

int disorder_productivity_percent(int disorder) {
    return disorder_soft_effect_percent(disorder);
}

int disorder_population_growth_percent(int disorder) {
    return disorder_soft_effect_percent(disorder);
}

int disorder_technology_percent(int disorder) {
    return disorder_soft_effect_percent(disorder);
}

static void record_debuff_change(int civ_id, int old_disorder, int new_disorder) {
    int old_percent = disorder_soft_effect_percent(old_disorder);
    int new_percent = disorder_soft_effect_percent(new_disorder);
    int previous = last_debuff_percent[civ_id] ? last_debuff_percent[civ_id] : old_percent;
    int diff = previous > new_percent ? previous - new_percent : new_percent - previous;
    char text[EVENT_LOG_LEN];

    if (diff >= 5) {
        snprintf(text, sizeof(text), "[Disorder] %.64s soft penalties now %d%% at disorder %d.",
                 civilization_display_name_for_language(civ_id, 0), new_percent, new_disorder);
        event_log_push(text);
    }
    last_debuff_percent[civ_id] = new_percent;
}

static void finish_disorder_change(int civ_id, int old_disorder, int allow_immediate) {
    int old_percent = disorder_soft_effect_percent(old_disorder);
    int new_percent = disorder_soft_effect_percent(civs[civ_id].disorder);

    record_debuff_change(civ_id, old_disorder, civs[civ_id].disorder);
    if (old_percent != new_percent) world_invalidate_country_summary_cache();
    if (allow_immediate && old_disorder < 100 && civs[civ_id].disorder >= 100) {
        collapse_check_immediate(civ_id, COLLAPSE_CAUSE_PRESSURE);
    }
}

void disorder_update_month(int civ_id, int resource_score) {
    Civilization *civ;
    int pressure;
    int pressure_disorder;
    int scarcity_disorder;
    int recovery;
    int delta;
    int old_disorder;

    if (civ_id < 0 || civ_id >= civ_count || !civs[civ_id].alive) return;
    civ = &civs[civ_id];
    old_disorder = civ->disorder;
    pressure = population_pressure_for_civ(civ_id);
    pressure_disorder = clamp((pressure - 85) / 3, 0, 45);
    scarcity_disorder = clamp(34 - resource_score, 0, 34);
    civ->disorder_resource = clamp(pressure_disorder + scarcity_disorder, 0, 100);
    civ->disorder_plague = clamp(civ->disorder_plague - 1, 0, 100);
    civ->disorder_migration = clamp(civ->disorder_migration - 1, 0, 100);
    civ->disorder_stability = clamp(civ->disorder_stability - 1, 0, 100);
    recovery = civ->governance / 2 + civ->cohesion / 2 + (pressure < 65 && resource_score > 30 ? 2 : 0);
    delta = civ->disorder_resource / 18 + civ->disorder_plague / 24 +
            civ->disorder_migration / 26 + civ->disorder_stability / 28 - recovery;
    civ->disorder = clamp(civ->disorder + delta, 0, 100);
    finish_disorder_change(civ_id, old_disorder, 1);
}

void disorder_set(int civ_id, int value) {
    int old_disorder;
    if (civ_id < 0 || civ_id >= civ_count || !civs[civ_id].alive) return;
    old_disorder = civs[civ_id].disorder;
    civs[civ_id].disorder = clamp(value, 0, 100);
    finish_disorder_change(civ_id, old_disorder, 0);
}

void disorder_add_war_deaths(int civ_id, int deaths) {
    int old_disorder;
    if (civ_id < 0 || civ_id >= civ_count || deaths <= 0) return;
    old_disorder = civs[civ_id].disorder;
    civs[civ_id].disorder_stability = clamp(civs[civ_id].disorder_stability + deaths / 2000 + 1, 0, 100);
    civs[civ_id].disorder = clamp(civs[civ_id].disorder + deaths / 3000 + 1, 0, 100);
    finish_disorder_change(civ_id, old_disorder, 1);
}

void disorder_add_plague_deaths(int civ_id, int deaths) {
    int old_disorder;
    if (civ_id < 0 || civ_id >= civ_count || deaths <= 0) return;
    old_disorder = civs[civ_id].disorder;
    civs[civ_id].disorder_plague = clamp(civs[civ_id].disorder_plague + deaths / 2500 + 1, 0, 100);
    civs[civ_id].disorder = clamp(civs[civ_id].disorder + deaths / 4000 + 1, 0, 100);
    finish_disorder_change(civ_id, old_disorder, 1);
}
