#include "sim/civ_colors.h"

#include "core/dirty_flags.h"
#include "core/game_types.h"
#include "sim/regions.h"

#include <string.h>

static unsigned char manual_color_locked[MAX_CIVS];
static unsigned char color_repair_dirty[MAX_CIVS];
static int color_repair_last_month[MAX_CIVS];
static int color_repair_changed_total;
static int color_repair_unresolved_total;
static int color_repair_cooldown_total;

static int color_month_index(void) {
    return year * 12 + month - 1;
}

void civilization_color_repair_note_changed(void) {
    color_repair_changed_total++;
}

void civilization_color_repair_note_unresolved(void) {
    color_repair_unresolved_total++;
}

void civilization_color_reset_manual_locks(void) {
    memset(manual_color_locked, 0, sizeof(manual_color_locked));
    memset(color_repair_dirty, 0, sizeof(color_repair_dirty));
    memset(color_repair_last_month, 0, sizeof(color_repair_last_month));
    color_repair_changed_total = 0;
    color_repair_unresolved_total = 0;
    color_repair_cooldown_total = 0;
}

void civilization_color_mark_manual(int civ_id) {
    if (civ_id >= 0 && civ_id < MAX_CIVS) manual_color_locked[civ_id] = 1;
}

int civilization_color_manual_locked(int civ_id) {
    return civ_id >= 0 && civ_id < MAX_CIVS && manual_color_locked[civ_id];
}

void civilization_color_note_region_claim(int owner, int region_id) {
    int i;
    if (owner >= 0 && owner < MAX_CIVS) color_repair_dirty[owner] = 1;
    if (region_id < 0 || region_id >= region_count) return;
    for (i = 0; i < natural_regions[region_id].neighbor_count; i++) {
        int neighbor = natural_regions[region_id].neighbors[i];
        int other;
        if (neighbor < 0 || neighbor >= region_count) continue;
        other = natural_regions[neighbor].owner_civ;
        if (other >= 0 && other < MAX_CIVS) color_repair_dirty[other] = 1;
    }
}

static int color_seed_region_for_civ(int civ_id) {
    int i;
    if (civ_id >= 0 && civ_id < civ_count &&
        civs[civ_id].capital_city >= 0 && civs[civ_id].capital_city < city_count) {
        return regions_region_for_city(civs[civ_id].capital_city);
    }
    for (i = 0; i < region_count; i++) {
        if (natural_regions[i].alive && natural_regions[i].owner_civ == civ_id) return i;
    }
    return -1;
}

static int color_recently_repaired(int civ_id) {
    int last = civ_id >= 0 && civ_id < MAX_CIVS ? color_repair_last_month[civ_id] : 0;
    return last > 0 && color_month_index() - last < 12;
}

static int color_repair_target(int a, int b) {
    int a_locked = civilization_color_manual_locked(a);
    int b_locked = civilization_color_manual_locked(b);
    int a_size;
    int b_size;
    int first;
    int second;

    if (a_locked && b_locked) {
        civilization_color_repair_note_unresolved();
        return -1;
    }
    if (a_locked) {
        if (!color_recently_repaired(b)) return b;
        color_repair_cooldown_total++;
        return -1;
    }
    if (b_locked) {
        if (!color_recently_repaired(a)) return a;
        color_repair_cooldown_total++;
        return -1;
    }
    a_size = regions_owned_count_for_civ(a);
    b_size = regions_owned_count_for_civ(b);
    first = a_size <= b_size ? a : b;
    second = first == a ? b : a;
    if (!color_recently_repaired(first)) return first;
    if (!color_recently_repaired(second)) return second;
    color_repair_cooldown_total++;
    return -1;
}

static int civilization_repair_color_pair(int a, int b) {
    int target;
    Color32 old_color;
    Color32 replacement;
    if (a < 0 || b < 0 || a >= civ_count || b >= civ_count || a == b) return 0;
    if (!civs[a].alive || !civs[b].alive) return 0;
    if (!civilization_colors_too_similar_for_display(civs[a].color, civs[b].color)) return 0;
    target = color_repair_target(a, b);
    if (target < 0) return 0;
    old_color = civs[target].color;
    replacement = civilization_pick_distinct_color(target, 0, -1, color_seed_region_for_civ(target));
    if (replacement == old_color) {
        civilization_color_repair_note_unresolved();
        return 0;
    }
    civs[target].color = replacement;
    color_repair_last_month[target] = color_month_index();
    civilization_color_repair_note_changed();
    dirty_mark_civ();
    dirty_mark_territory();
    return 1;
}

static int civilization_repair_neighbor_conflicts(int civ_id, int max_repairs) {
    unsigned char seen[MAX_CIVS];
    int repairs = 0;
    int r;
    memset(seen, 0, sizeof(seen));
    if (civ_id < 0 || civ_id >= civ_count || !civs[civ_id].alive) return 0;
    for (r = 0; r < region_count && repairs < max_repairs; r++) {
        int n;
        if (!natural_regions[r].alive || natural_regions[r].owner_civ != civ_id) continue;
        for (n = 0; n < natural_regions[r].neighbor_count && repairs < max_repairs; n++) {
            int neighbor = natural_regions[r].neighbors[n];
            int other;
            if (neighbor < 0 || neighbor >= region_count) continue;
            other = natural_regions[neighbor].owner_civ;
            if (other < 0 || other >= civ_count || other == civ_id || seen[other]) continue;
            seen[other] = 1;
            repairs += civilization_repair_color_pair(civ_id, other);
        }
    }
    return repairs;
}

int civilization_repair_queued_color_conflicts(int budget) {
    int processed = 0;
    int changed_before = color_repair_changed_total;
    int i;
    if (budget < 1) budget = 1;
    for (i = 0; i < civ_count && processed < budget; i++) {
        if (!color_repair_dirty[i]) continue;
        color_repair_dirty[i] = 0;
        civilization_repair_neighbor_conflicts(i, 1);
        processed++;
    }
    return color_repair_changed_total - changed_before;
}

int civilization_color_repair_changed_count(void) { return color_repair_changed_total; }
int civilization_color_repair_unresolved_count(void) { return color_repair_unresolved_total; }
int civilization_color_repair_cooldown_count(void) { return color_repair_cooldown_total; }
