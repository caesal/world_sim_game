#include "render/panel_diplomacy_cache_key.h"

static unsigned int mix_key(unsigned int key, int value) {
    return key * 1000003u ^ (unsigned int)value;
}

static int key_bucket(int value, int bucket) {
    return bucket <= 1 ? value : (value >= 0 ? value / bucket : -((-value + bucket - 1) / bucket));
}

unsigned int panel_diplomacy_rows_cache_key(unsigned int key, const RenderSnapshot *snapshot,
                                            int civ_id, int all_relations) {
    int i;
    if (!snapshot || civ_id < 0 || civ_id >= snapshot->civ_count) return mix_key(key, 0);
    for (i = 0; i < snapshot->civ_count; i++) {
        const SnapshotCiv *other = &snapshot->civs[i];
        const SnapshotDiplomacyRelation *rel = &snapshot->relations[civ_id][i];
        const SnapshotDiplomacyRelation *rev = &snapshot->relations[i][civ_id];
        const SnapshotWar *war = &snapshot->wars[civ_id][i];
        int f;
        int direct = other->alive && (other->overlord == civ_id || snapshot->civs[civ_id].overlord == i);
        if (i == civ_id || (!all_relations && !direct)) continue;
        key = mix_key(key, other->uid); key = mix_key(key, other->alive);
        key = mix_key(key, other->overlord); key = mix_key(key, key_bucket(other->current_soldiers, 100));
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
        key = mix_key(key, rel->truce_years_left); key = mix_key(key, war->active);
        key = mix_key(key, key_bucket(war->soldiers_a + war->soldiers_b, 100));
    }
    return key;
}
