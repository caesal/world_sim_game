#include "render/panel_diplomacy_cache_key.h"

#include <stdint.h>

static unsigned int mix_key(unsigned int key, int value) {
    return key * 1000003u ^ (unsigned int)value;
}

static unsigned int mix_u64_key(unsigned int key, uint64_t value) {
    key = mix_key(key, (int)(uint32_t)value);
    return mix_key(key, (int)(uint32_t)(value >> 32));
}

static int key_bucket(int value, int bucket) {
    return bucket <= 1 ? value : (value >= 0 ? value / bucket : -((-value + bucket - 1) / bucket));
}

static unsigned int mix_war_key(unsigned int key, const RenderSnapshot *snapshot, int a, int b) {
    const SnapshotWar *war = &snapshot->wars[a][b];
    key = mix_key(key, war->active);
    key = mix_key(key, war->attacker);
    key = mix_key(key, war->defender);
    key = mix_key(key, war->years);
    key = mix_key(key, war->soldiers_a);
    key = mix_key(key, war->soldiers_b);
    key = mix_key(key, war->temporary_soldiers_a);
    key = mix_key(key, war->temporary_soldiers_b);
    key = mix_key(key, war->alliance_reinforcements_a);
    key = mix_key(key, war->alliance_reinforcements_b);
    key = mix_key(key, war->casualties_a);
    key = mix_key(key, war->casualties_b);
    key = mix_key(key, war->support_casualties_a);
    key = mix_key(key, war->support_casualties_b);
    key = mix_key(key, war->wins_a);
    key = mix_key(key, war->wins_b);
    key = mix_key(key, snapshot->war_front_flags[a][b]);
    key = mix_key(key, snapshot->war_front_flags[b][a]);
    key = mix_key(key, snapshot->war_peace_pressure[a][b]);
    key = mix_key(key, snapshot->war_peace_pressure[b][a]);
    return key;
}

unsigned int panel_diplomacy_rows_cache_key_for_view(unsigned int key, const RenderSnapshot *snapshot,
                                                     int civ_id, int all_relations, int include_war_live) {
    int i;
    if (!snapshot || civ_id < 0 || civ_id >= snapshot->civ_count) return mix_key(key, 0);
    if (include_war_live) {
        key = mix_key(key, snapshot->year);
        key = mix_key(key, snapshot->month);
        key = mix_key(key, snapshot->civs[civ_id].disorder);
        key = mix_key(key, snapshot->civs[civ_id].effective_disorder);
        key = mix_u64_key(key, snapshot->civs[civ_id].war_history.revision);
    }
    for (i = 0; i < snapshot->civ_count; i++) {
        const SnapshotCiv *other = &snapshot->civs[i];
        const SnapshotDiplomacyRelation *rel = &snapshot->relations[civ_id][i];
        const SnapshotDiplomacyRelation *rev = &snapshot->relations[i][civ_id];
        int f;
        int direct = other->alive && (other->overlord == civ_id || snapshot->civs[civ_id].overlord == i);
        if (i == civ_id || (!all_relations && !direct)) continue;
        key = mix_key(key, other->uid); key = mix_key(key, other->alive);
        key = mix_key(key, other->overlord); key = mix_key(key, key_bucket(other->current_soldiers, 100));
        key = mix_key(key, other->disorder); key = mix_key(key, other->effective_disorder);
        key = mix_key(key, other->vassal_callable_soldiers); key = mix_key(key, other->vassal_resource_tribute);
        key = mix_key(key, rel->state); key = mix_key(key, rel->relation_score);
        key = mix_key(key, rev->relation_score); key = mix_key(key, rel->yearly_delta_x100);
        key = mix_key(key, rel->state_years); key = mix_key(key, rel->candidate_state);
        key = mix_key(key, rel->candidate_years);
        for (f = 0; f < DIP_REL_FACTOR_SLOTS; f++) {
            key = mix_key(key, rel->relation_factor_ids[f]);
            key = mix_key(key, rel->relation_factor_delta_x100[f]);
            key = mix_key(key, rel->relation_factor_values[f]);
        }
        key = mix_key(key, rel->truce_years_left);
        key = mix_key(key, rel->truce_initial_years);
        key = mix_key(key, rev->truce_years_left);
        key = mix_key(key, rev->truce_initial_years);
        key = mix_key(key, rel->last_war_winner);
        key = mix_key(key, rel->last_war_loser);
        key = mix_key(key, rel->last_war_result);
        if (include_war_live) key = mix_war_key(key, snapshot, civ_id, i);
    }
    return key;
}

unsigned int panel_diplomacy_rows_cache_key(unsigned int key, const RenderSnapshot *snapshot,
                                            int civ_id, int all_relations) {
    return panel_diplomacy_rows_cache_key_for_view(key, snapshot, civ_id, all_relations, 1);
}
