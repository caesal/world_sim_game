#ifndef WORLD_SIM_ENCLAVE_RESOLUTION_H
#define WORLD_SIM_ENCLAVE_RESOLUTION_H

typedef enum {
    ENCLAVE_RESOLUTION_INDEPENDENT = 0,
    ENCLAVE_RESOLUTION_ORIGINAL_VASSAL,
    ENCLAVE_RESOLUTION_JOIN_LAND_NEIGHBOR,
    ENCLAVE_RESOLUTION_UNOWNED
} EnclaveResolutionEvent;

EnclaveResolutionEvent enclave_resolution_pick_event_for_roll(int component_count,
                                                              int owner_tech_stage,
                                                              int roll);
int enclave_resolve_component(int owner, const int *regions, int count, int months);

#endif
