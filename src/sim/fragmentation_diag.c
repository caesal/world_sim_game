#include "sim/fragmentation_diag.h"

#include "core/game_state.h"
#include "sim/regions.h"
#include "sim/vassal.h"

#include <string.h>

static FragmentationDiagnostics run_counters;

void fragmentation_diag_reset(void) {
    memset(&run_counters, 0, sizeof(run_counters));
}

void fragmentation_diag_record_enclave_independent(void) { run_counters.enclave_independent++; }
void fragmentation_diag_record_enclave_original_vassal(void) { run_counters.enclave_original_vassal++; }
void fragmentation_diag_record_enclave_joined_land_neighbor(void) { run_counters.enclave_joined_land_neighbor++; }
void fragmentation_diag_record_enclave_unowned_collapse(void) { run_counters.enclave_unowned_collapse++; }
void fragmentation_diag_record_enclave_fallback_slot_full(void) { run_counters.enclave_fallback_slot_full++; }
void fragmentation_diag_record_enclave_fallback_no_land_neighbor(void) { run_counters.enclave_fallback_no_land_neighbor++; }
void fragmentation_diag_record_enclave_claim_failure(void) { run_counters.enclave_claim_failures++; }
void fragmentation_diag_record_collapse_attempt(void) { run_counters.collapse_attempts++; }
void fragmentation_diag_record_collapse_failure(void) { run_counters.collapse_failures++; }
void fragmentation_diag_record_vassal_release(void) { run_counters.vassal_releases++; }
void fragmentation_diag_record_vassal_peaceful_independence(void) { run_counters.vassal_peaceful_independence++; }
void fragmentation_diag_record_vassal_independence_war(void) { run_counters.vassal_independence_wars++; }

void fragmentation_diag_record_collapse_success(int successors) {
    run_counters.collapse_successes++;
    if (successors > 0) run_counters.collapse_successors_created += successors;
}

void fragmentation_diag_snapshot(FragmentationDiagnostics *out) {
    int owned[MAX_CIVS];
    int i;

    if (!out) return;
    *out = run_counters;
    memset(owned, 0, sizeof(owned));
    for (i = 0; i < region_count; i++) {
        int owner = natural_regions[i].owner_civ;
        if (natural_regions[i].alive && owner >= 0 && owner < MAX_CIVS) owned[owner]++;
    }
    for (i = 0; i < civ_count && i < MAX_CIVS; i++) {
        if (!civs[i].alive) continue;
        out->alive_civs++;
        if (vassal_overlord(i) >= 0) out->vassal_civs++;
        else out->independent_civs++;
        if (owned[i] == 1) out->one_province_civs++;
    }
    out->used_slots = civ_count;
    out->max_civs = MAX_CIVS;
}
