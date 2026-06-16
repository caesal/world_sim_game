#include "sim/diplomacy_stability.h"

#include <string.h>

static int state_years[MAX_CIVS][MAX_CIVS];
static int candidate_state[MAX_CIVS][MAX_CIVS];
static int candidate_years[MAX_CIVS][MAX_CIVS];

static int valid_pair(int civ_a, int civ_b) {
    return civ_a >= 0 && civ_a < MAX_CIVS && civ_b >= 0 && civ_b < MAX_CIVS && civ_a != civ_b;
}

static void mirror_pair(int civ_a, int civ_b) {
    state_years[civ_b][civ_a] = state_years[civ_a][civ_b];
    candidate_state[civ_b][civ_a] = candidate_state[civ_a][civ_b];
    candidate_years[civ_b][civ_a] = candidate_years[civ_a][civ_b];
}

static void clear_candidate(int civ_a, int civ_b) {
    candidate_state[civ_a][civ_b] = DIPLOMACY_NONE;
    candidate_years[civ_a][civ_b] = 0;
}

void diplomacy_stability_reset(void) {
    memset(state_years, 0, sizeof(state_years));
    memset(candidate_state, 0, sizeof(candidate_state));
    memset(candidate_years, 0, sizeof(candidate_years));
}

void diplomacy_stability_clear_civ(int civ_id) {
    int i;
    if (civ_id < 0 || civ_id >= MAX_CIVS) return;
    for (i = 0; i < MAX_CIVS; i++) {
        state_years[civ_id][i] = state_years[i][civ_id] = 0;
        candidate_state[civ_id][i] = candidate_state[i][civ_id] = DIPLOMACY_NONE;
        candidate_years[civ_id][i] = candidate_years[i][civ_id] = 0;
    }
}

void diplomacy_stability_reset_pair(int civ_a, int civ_b) {
    if (!valid_pair(civ_a, civ_b)) return;
    state_years[civ_a][civ_b] = 0;
    clear_candidate(civ_a, civ_b);
    mirror_pair(civ_a, civ_b);
}

void diplomacy_stability_force_pair(int civ_a, int civ_b, DiplomacyStatus state) {
    if (!valid_pair(civ_a, civ_b)) return;
    (void)state;
    state_years[civ_a][civ_b] = 0;
    clear_candidate(civ_a, civ_b);
    mirror_pair(civ_a, civ_b);
}

DiplomacyStatus diplomacy_stability_step_pair(int civ_a, int civ_b,
                                              DiplomacyStatus current,
                                              DiplomacyStatus desired,
                                              int allow_soft_change) {
    if (!valid_pair(civ_a, civ_b)) return current;
    if (desired == current) {
        if (state_years[civ_a][civ_b] < 1000000) state_years[civ_a][civ_b]++;
        clear_candidate(civ_a, civ_b);
        mirror_pair(civ_a, civ_b);
        return current;
    }
    if (!allow_soft_change) {
        if (state_years[civ_a][civ_b] < 1000000) state_years[civ_a][civ_b]++;
        clear_candidate(civ_a, civ_b);
        mirror_pair(civ_a, civ_b);
        return current;
    }
    if (candidate_state[civ_a][civ_b] != (int)desired) {
        candidate_state[civ_a][civ_b] = desired;
        candidate_years[civ_a][civ_b] = 1;
    } else if (candidate_years[civ_a][civ_b] < DIPLOMACY_SOFT_TRANSITION_YEARS) {
        candidate_years[civ_a][civ_b]++;
    }
    if (candidate_years[civ_a][civ_b] >= DIPLOMACY_SOFT_TRANSITION_YEARS) {
        diplomacy_stability_force_pair(civ_a, civ_b, desired);
        return desired;
    }
    mirror_pair(civ_a, civ_b);
    return current;
}

int diplomacy_stability_state_years(int civ_a, int civ_b) {
    return valid_pair(civ_a, civ_b) ? state_years[civ_a][civ_b] : 0;
}

int diplomacy_stability_candidate_state(int civ_a, int civ_b) {
    return valid_pair(civ_a, civ_b) ? candidate_state[civ_a][civ_b] : DIPLOMACY_NONE;
}

int diplomacy_stability_candidate_years(int civ_a, int civ_b) {
    return valid_pair(civ_a, civ_b) ? candidate_years[civ_a][civ_b] : 0;
}

int diplomacy_stability_grace_years_left(int civ_a, int civ_b) {
    int years = diplomacy_stability_state_years(civ_a, civ_b);
    return years < DIPLOMACY_SOFT_GRACE_YEARS ? DIPLOMACY_SOFT_GRACE_YEARS - years : 0;
}
