#include "sim/diplomacy_year.h"

#include "core/game_state.h"
#include "core/profiler.h"
#include "sim/diplomacy.h"
#include "sim/diplomacy_relation_score.h"
#include "sim/war_desire.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static int diplomacy_year_last_ms = 0;
static int diplomacy_year_peak_ms = 0;

static int diplomacy_year_valid_civ(int civ_id) {
    return civ_id >= 0 && civ_id < civ_count && civ_id < MAX_CIVS && civs[civ_id].alive;
}

static void record_diplomacy_year_step_ms(DWORD start) {
    diplomacy_year_last_ms = (int)(GetTickCount() - start);
    if (diplomacy_year_last_ms > diplomacy_year_peak_ms) {
        diplomacy_year_peak_ms = diplomacy_year_last_ms;
    }
    profiler_record_spike_phase(PROFILER_SPIKE_ANNUAL_DIPLOMACY,
                                "Diplomacy Year step", diplomacy_year_last_ms);
}

void diplomacy_year_work_begin(DiplomacyYearWork *work) {
    if (!work) return;
    work->started = 0;
    work->done = 0;
    work->civ_a = 0;
    work->civ_b = 1;
}

int diplomacy_update_year_step(DiplomacyYearWork *work, int pair_budget) {
    DWORD start = GetTickCount();
    int processed = 0;
    if (!work) return 1;
    if (pair_budget < 1) pair_budget = 1;
    if (work->done) return 1;
    if (!work->started) {
        diplomacy_update_contacts();
        war_desire_reset_all();
        diplomacy_relation_score_begin_year();
        work->started = 1;
    }
    while (work->civ_a < civ_count && processed < pair_budget) {
        int civ_b;
        if (!diplomacy_year_valid_civ(work->civ_a)) {
            work->civ_a++;
            work->civ_b = work->civ_a + 1;
            continue;
        }
        if (work->civ_b <= work->civ_a) work->civ_b = work->civ_a + 1;
        if (work->civ_b >= civ_count) {
            work->civ_a++;
            work->civ_b = work->civ_a + 1;
            continue;
        }
        civ_b = work->civ_b++;
        processed++;
        if (diplomacy_year_valid_civ(civ_b)) {
            diplomacy_refresh_known_relation_for_year(work->civ_a, civ_b);
        }
    }
    if (work->civ_a >= civ_count) {
        diplomacy_relation_score_end_year();
        work->done = 1;
    }
    record_diplomacy_year_step_ms(start);
    return work->done;
}

void diplomacy_update_year(void) {
    DiplomacyYearWork work;
    diplomacy_year_work_begin(&work);
    while (!diplomacy_update_year_step(&work, 64)) {}
}

int diplomacy_year_last_step_ms(void) { return diplomacy_year_last_ms; }
int diplomacy_year_peak_step_ms(void) { return diplomacy_year_peak_ms; }
