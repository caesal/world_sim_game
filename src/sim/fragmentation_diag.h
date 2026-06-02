#ifndef WORLD_SIM_FRAGMENTATION_DIAG_H
#define WORLD_SIM_FRAGMENTATION_DIAG_H

typedef struct {
    int enclave_independent;
    int enclave_original_vassal;
    int enclave_joined_land_neighbor;
    int enclave_unowned_collapse;
    int enclave_fallback_slot_full;
    int enclave_fallback_no_land_neighbor;
    int enclave_claim_failures;
    int collapse_successors_created;
    int collapse_attempts;
    int collapse_successes;
    int collapse_failures;
    int vassal_releases;
    int vassal_peaceful_independence;
    int vassal_independence_wars;
    int alive_civs;
    int independent_civs;
    int vassal_civs;
    int one_province_civs;
    int used_slots;
    int max_civs;
} FragmentationDiagnostics;

void fragmentation_diag_reset(void);
void fragmentation_diag_record_enclave_independent(void);
void fragmentation_diag_record_enclave_original_vassal(void);
void fragmentation_diag_record_enclave_joined_land_neighbor(void);
void fragmentation_diag_record_enclave_unowned_collapse(void);
void fragmentation_diag_record_enclave_fallback_slot_full(void);
void fragmentation_diag_record_enclave_fallback_no_land_neighbor(void);
void fragmentation_diag_record_enclave_claim_failure(void);
void fragmentation_diag_record_collapse_attempt(void);
void fragmentation_diag_record_collapse_success(int successors);
void fragmentation_diag_record_collapse_failure(void);
void fragmentation_diag_record_vassal_release(void);
void fragmentation_diag_record_vassal_peaceful_independence(void);
void fragmentation_diag_record_vassal_independence_war(void);
void fragmentation_diag_snapshot(FragmentationDiagnostics *out);

#endif
