#include "sim/alliance.h"

#include "core/dirty_flags.h"
#include "core/game_state.h"
#include "sim/simulation.h"
#include "sim/economy.h"
#include "sim/vassal.h"
#include "sim/war.h"

#include <string.h>

typedef struct {
    int valid;
    int key;
    int value;
} AlliancePowerCacheEntry;

static AlliancePowerCacheEntry own_cache[MAX_CIVS];
static AlliancePowerCacheEntry bloc_cache[MAX_CIVS];
static int own_recompute_count, bloc_recompute_count;

static int mix_key(int key, int value) {
    return key * 1000003 ^ value;
}

static int alive_civ(int civ_id) {
    return civ_id >= 0 && civ_id < civ_count && civ_id < MAX_CIVS && civs[civ_id].alive;
}

static int power_key(void) {
    int key = dirty_revision_civ();
    key = mix_key(key, dirty_revision_population());
    key = mix_key(key, dirty_revision_ownership());
    key = mix_key(key, dirty_revision_province());
    key = mix_key(key, dirty_revision_diplomacy());
    key = mix_key(key, dirty_revision_alliance());
    key = mix_key(key, year * 12 + month);
    return mix_key(key, civ_count);
}

static int compute_own_power(int civ_id) {
    CountrySummary summary;
    Civilization *civ;
    int vassal_soldiers;
    if (!alive_civ(civ_id)) return 0;
    summary = summarize_country(civ_id);
    civ = &civs[civ_id];
    vassal_soldiers = vassal_overlord(civ_id) < 0 ? vassal_total_callable_soldiers(civ_id) : 0;
    return max(1, summary.population / 800 + summary.food * 2 + summary.water * 2 +
               summary.money * 2 + summary.minerals * 2 +
               (war_current_soldiers_for_civ(civ_id) + vassal_soldiers * 70 / 100) * 7 +
               civ->production * 4 + civ->logistics * 4 + civ->cohesion * 3 -
               economy_effective_disorder_for_civ(civ_id) / 2);
}

int alliance_own_power(int civ_id) {
    AlliancePowerCacheEntry *entry;
    int key;
    if (!alive_civ(civ_id)) return 0;
    entry = &own_cache[civ_id];
    key = power_key();
    if (entry->valid && entry->key == key) return entry->value;
    entry->key = key;
    entry->value = compute_own_power(civ_id);
    entry->valid = 1;
    own_recompute_count++;
    return entry->value;
}

int alliance_defensive_bloc_power(int civ_id) {
    AlliancePowerCacheEntry *entry;
    int alliance_id = alliance_display_for_civ(civ_id);
    int i, count, total = 0;
    int key;
    if (alliance_id < 0) return alliance_own_power(civ_id);
    entry = &bloc_cache[civ_id];
    key = mix_key(power_key(), alliance_id);
    if (entry->valid && entry->key == key) return entry->value;
    count = alliance_member_count(alliance_id);
    for (i = 0; i < count; i++) {
        int member = alliance_formal_member_at(alliance_id, i);
        total += alliance_own_power(member);
    }
    entry->key = key;
    entry->value = max(1, total);
    entry->valid = 1;
    bloc_recompute_count++;
    return entry->value;
}

void alliance_power_cache_reset(void) {
    memset(own_cache, 0, sizeof(own_cache));
    memset(bloc_cache, 0, sizeof(bloc_cache));
    own_recompute_count = bloc_recompute_count = 0;
}

int alliance_power_own_recompute_count(void) { return own_recompute_count; }
int alliance_power_bloc_recompute_count(void) { return bloc_recompute_count; }
