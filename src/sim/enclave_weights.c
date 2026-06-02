#include "sim/enclave_resolution.h"

static int clamp_roll(int roll) {
    if (roll < 0) return 0;
    if (roll > 99) return 99;
    return roll;
}

EnclaveResolutionEvent enclave_resolution_pick_event_for_roll(int component_count,
                                                              int owner_tech_stage,
                                                              int roll) {
    int independent;
    int vassal;
    int join;
    int unowned;

    roll = clamp_roll(roll);
    if (component_count <= 1 && owner_tech_stage < 6) {
        unowned = 50; independent = 17; vassal = 16; join = 17;
        if (roll < unowned) return ENCLAVE_RESOLUTION_UNOWNED;
        roll -= unowned;
        if (roll < independent) return ENCLAVE_RESOLUTION_INDEPENDENT;
        roll -= independent;
        return roll < vassal ? ENCLAVE_RESOLUTION_ORIGINAL_VASSAL : ENCLAVE_RESOLUTION_JOIN_LAND_NEIGHBOR;
    }
    if (component_count <= 1) {
        join = 45; vassal = 20; independent = 20; unowned = 15;
        if (roll < join) return ENCLAVE_RESOLUTION_JOIN_LAND_NEIGHBOR;
        roll -= join;
        if (roll < vassal) return ENCLAVE_RESOLUTION_ORIGINAL_VASSAL;
        roll -= vassal;
        return roll < independent ? ENCLAVE_RESOLUTION_INDEPENDENT : ENCLAVE_RESOLUTION_UNOWNED;
    }
    if (owner_tech_stage < 6) {
        independent = 30; vassal = 25; join = 25; unowned = 20;
        if (roll < independent) return ENCLAVE_RESOLUTION_INDEPENDENT;
        roll -= independent;
        if (roll < vassal) return ENCLAVE_RESOLUTION_ORIGINAL_VASSAL;
        roll -= vassal;
        return roll < join ? ENCLAVE_RESOLUTION_JOIN_LAND_NEIGHBOR : ENCLAVE_RESOLUTION_UNOWNED;
    }
    join = 40; vassal = 25; independent = 25; unowned = 10;
    if (roll < join) return ENCLAVE_RESOLUTION_JOIN_LAND_NEIGHBOR;
    roll -= join;
    if (roll < vassal) return ENCLAVE_RESOLUTION_ORIGINAL_VASSAL;
    roll -= vassal;
    return roll < independent ? ENCLAVE_RESOLUTION_INDEPENDENT : ENCLAVE_RESOLUTION_UNOWNED;
}
