#include "disorder.h"

#include "core/game_state.h"
#include "sim/collapse.h"
#include "sim/plague.h"
#include "sim/population.h"
#include "sim/simulation.h"
#include "sim/vassal.h"
#include "sim/war.h"

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

    if (diff >= 5) {
        event_log_push_structured(EVENT_TYPE_DISORDER_CHANGED, EVENT_SEVERITY_INFO,
                                  civ_id, -1, -1, -1, new_percent, new_disorder, "");
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

static int plague_decay_for_civ(Civilization *civ, int civ_id) {
    if (civ->disorder_plague <= 0) return 0;
    if (plague_active_for_civ(civ_id)) {
        civ->plague_recovery_months = 0;
        return 1;
    }
    if (civ->plague_recovery_months < 0) civ->plague_recovery_months = 0;
    civ->plague_recovery_months++;
    if (civ->plague_recovery_months <= 12) return 4;
    if (civ->plague_recovery_months <= 24) return 3;
    return 2;
}

static int war_decay_for_civ(Civilization *civ, int civ_id) {
    if (civ->disorder_stability <= 0) return 0;
    if (war_active_for_civ(civ_id)) {
        civ->war_recovery_months = 0;
        return 1;
    }
    if (civ->war_recovery_months < 0) civ->war_recovery_months = 0;
    civ->war_recovery_months++;
    return civ->war_recovery_months <= 24 ? 3 : 2;
}

static int pressure_contribution_x10(Civilization *civ) {
    return civ->disorder_resource * 10 / 18 + civ->disorder_plague * 10 / 24 +
           civ->disorder_migration * 10 / 26 + civ->disorder_stability * 10 / 28;
}

static void record_recovery_components(Civilization *civ, int civ_id, int pressure, int resource_score, int has_war) {
    civ->disorder_last_base_recovery_x10 = 10;
    civ->disorder_last_governance_recovery_x10 = civ->governance * 5;
    civ->disorder_last_peace_recovery_x10 = has_war ? 0 : 10;
    civ->disorder_last_cohesion_recovery_x10 = has_war ? civ->cohesion : 0;
    civ->disorder_last_condition_recovery_x10 = (!has_war && pressure < 65 && resource_score > 30) ? 10 : 0;
    (void)civ_id;
}

void disorder_update_month(int civ_id, int resource_score) {
    Civilization *civ;
    int pressure;
    int pressure_disorder;
    int scarcity_disorder;
    int recovery_x10;
    int delta_x10;
    int total_x10;
    int next_disorder;
    int floor;
    int old_disorder;
    int plague_decay;
    int war_decay;
    int migration_decay;
    int has_war;

    if (civ_id < 0 || civ_id >= civ_count || !civs[civ_id].alive) return;
    civ = &civs[civ_id];
    old_disorder = civ->disorder;
    pressure = population_pressure_for_civ(civ_id);
    pressure_disorder = clamp((pressure - 85) / 3, 0, 45);
    scarcity_disorder = clamp(34 - resource_score, 0, 34);
    civ->disorder_resource = clamp(pressure_disorder + scarcity_disorder, 0, 100);
    plague_decay = plague_decay_for_civ(civ, civ_id);
    war_decay = war_decay_for_civ(civ, civ_id);
    migration_decay = civ->disorder_migration > 0 ? 4 : 0;
    civ->disorder_plague = clamp(civ->disorder_plague - plague_decay, 0, 100);
    civ->disorder_migration = clamp(civ->disorder_migration - migration_decay, 0, 100);
    civ->disorder_stability = clamp(civ->disorder_stability - war_decay, 0, 100);
    has_war = war_active_for_civ(civ_id);
    record_recovery_components(civ, civ_id, pressure, resource_score, has_war);
    recovery_x10 = civ->disorder_last_base_recovery_x10 + civ->disorder_last_governance_recovery_x10 +
                   civ->disorder_last_peace_recovery_x10 + civ->disorder_last_cohesion_recovery_x10 +
                   civ->disorder_last_condition_recovery_x10;
    delta_x10 = pressure_contribution_x10(civ) - recovery_x10;
    civ->disorder_last_pressure_x10 = pressure_contribution_x10(civ);
    civ->disorder_last_recovery_x10 = recovery_x10;
    civ->disorder_last_net_x10 = delta_x10;
    civ->disorder_last_pressure = civ->disorder_last_pressure_x10 / 10;
    civ->disorder_last_recovery = civ->disorder_last_recovery_x10 / 10;
    civ->disorder_last_net = civ->disorder_last_net_x10 / 10;
    civ->disorder_last_plague_decay = plague_decay;
    civ->disorder_last_war_decay = war_decay;
    civ->disorder_last_migration_decay = migration_decay;
    if (civ->collapse_grace_months > 0) civ->collapse_grace_months--;
    total_x10 = civ->disorder * 10 + civ->disorder_carry_x10 + delta_x10;
    next_disorder = clamp(total_x10 / 10, 0, 100);
    civ->disorder_carry_x10 = total_x10 - next_disorder * 10;
    if (next_disorder == 0 || next_disorder == 100) civ->disorder_carry_x10 = 0;
    civ->disorder = next_disorder;
    floor = vassal_governance_disorder(civ_id);
    if (civ->disorder < floor) {
        civ->disorder = floor;
        civ->disorder_carry_x10 = 0;
    }
    finish_disorder_change(civ_id, old_disorder, 1);
}

void disorder_set(int civ_id, int value) {
    int old_disorder;
    if (civ_id < 0 || civ_id >= civ_count || !civs[civ_id].alive) return;
    old_disorder = civs[civ_id].disorder;
    civs[civ_id].disorder = clamp(value, 0, 100);
    finish_disorder_change(civ_id, old_disorder, 0);
}

void disorder_set_civil_unrest(int civ_id) {
    if (civ_id < 0 || civ_id >= civ_count || !civs[civ_id].alive) return;
    civs[civ_id].disorder_stability = 100;
    disorder_set(civ_id, 100);
}

void disorder_relieve(int civ_id, int amount) {
    int old_disorder;
    if (civ_id < 0 || civ_id >= civ_count || amount <= 0 || !civs[civ_id].alive) return;
    old_disorder = civs[civ_id].disorder;
    civs[civ_id].disorder_plague = clamp(civs[civ_id].disorder_plague - amount, 0, 100);
    civs[civ_id].disorder_migration = clamp(civs[civ_id].disorder_migration - amount, 0, 100);
    civs[civ_id].disorder_stability = clamp(civs[civ_id].disorder_stability - amount, 0, 100);
    civs[civ_id].disorder = max(vassal_governance_disorder(civ_id), clamp(civs[civ_id].disorder - amount, 0, 100));
    finish_disorder_change(civ_id, old_disorder, 0);
}

void disorder_add_war_pressure(int civ_id, int amount) {
    if (civ_id < 0 || civ_id >= civ_count || amount <= 0 || !civs[civ_id].alive) return;
    civs[civ_id].disorder_stability = clamp(civs[civ_id].disorder_stability + amount, 0, 100);
}

void disorder_add_plague_pressure(int civ_id, int amount) {
    if (civ_id < 0 || civ_id >= civ_count || amount <= 0 || !civs[civ_id].alive) return;
    civs[civ_id].disorder_plague = clamp(civs[civ_id].disorder_plague + amount, 0, 100);
}

void disorder_add_migration_pressure(int civ_id, int amount) {
    if (civ_id < 0 || civ_id >= civ_count || amount <= 0 || !civs[civ_id].alive) return;
    civs[civ_id].disorder_migration = clamp(civs[civ_id].disorder_migration + amount, 0, 100);
}

void disorder_add_war_deaths(int civ_id, int deaths) {
    disorder_add_war_pressure(civ_id, deaths / 2000 + 1);
}

void disorder_add_plague_deaths(int civ_id, int deaths) {
    disorder_add_plague_pressure(civ_id, deaths / 2500 + 1);
}

int disorder_last_pressure(int civ_id) {
    return civ_id >= 0 && civ_id < civ_count ? civs[civ_id].disorder_last_pressure : 0;
}

int disorder_last_recovery(int civ_id) {
    return civ_id >= 0 && civ_id < civ_count ? civs[civ_id].disorder_last_recovery : 0;
}

int disorder_last_net(int civ_id) {
    return civ_id >= 0 && civ_id < civ_count ? civs[civ_id].disorder_last_net : 0;
}

int disorder_last_pressure_x10(int civ_id) {
    return civ_id >= 0 && civ_id < civ_count ? civs[civ_id].disorder_last_pressure_x10 : 0;
}

int disorder_last_recovery_x10(int civ_id) {
    return civ_id >= 0 && civ_id < civ_count ? civs[civ_id].disorder_last_recovery_x10 : 0;
}

int disorder_last_net_x10(int civ_id) {
    return civ_id >= 0 && civ_id < civ_count ? civs[civ_id].disorder_last_net_x10 : 0;
}

int disorder_last_base_recovery_x10(int civ_id) {
    return civ_id >= 0 && civ_id < civ_count ? civs[civ_id].disorder_last_base_recovery_x10 : 0;
}

int disorder_last_governance_recovery_x10(int civ_id) {
    return civ_id >= 0 && civ_id < civ_count ? civs[civ_id].disorder_last_governance_recovery_x10 : 0;
}

int disorder_last_cohesion_recovery_x10(int civ_id) {
    return civ_id >= 0 && civ_id < civ_count ? civs[civ_id].disorder_last_cohesion_recovery_x10 : 0;
}

int disorder_last_peace_recovery_x10(int civ_id) {
    return civ_id >= 0 && civ_id < civ_count ? civs[civ_id].disorder_last_peace_recovery_x10 : 0;
}

int disorder_last_condition_recovery_x10(int civ_id) {
    return civ_id >= 0 && civ_id < civ_count ? civs[civ_id].disorder_last_condition_recovery_x10 : 0;
}

int disorder_last_plague_decay(int civ_id) {
    return civ_id >= 0 && civ_id < civ_count ? civs[civ_id].disorder_last_plague_decay : 0;
}

int disorder_last_war_decay(int civ_id) {
    return civ_id >= 0 && civ_id < civ_count ? civs[civ_id].disorder_last_war_decay : 0;
}

int disorder_last_migration_decay(int civ_id) {
    return civ_id >= 0 && civ_id < civ_count ? civs[civ_id].disorder_last_migration_decay : 0;
}

int disorder_pressure_eta_months(int value, int monthly_decay) {
    if (value <= 0) return 0;
    if (monthly_decay <= 0) return -1;
    return (value + monthly_decay - 1) / monthly_decay;
}
