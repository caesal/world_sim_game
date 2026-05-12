#include "sim/simulation.h"

#include <stdio.h>

static char default_symbol_for_index(int index) {
    static const char symbols[] = {
        'R', 'B', 'G', 'Y', 'V', 'O', 'T', 'M',
        'I', 'K', 'A', 'Z', 'C', 'S', 'J', 'P',
        'Q', 'L', 'D', 'U', 'H', 'N', 'W', 'E',
        'F', 'X'
    };
    if (index < (int)(sizeof(symbols) / sizeof(symbols[0]))) return symbols[index];
    return (char)('A' + (index % 26));
}

static int default_trait_for_index(int index, int metric) {
    static const int traits[][7] = {
        {8, 7, 4, 3, 6, 5, 4}, {3, 4, 8, 6, 5, 5, 6},
        {5, 6, 5, 7, 6, 6, 7}, {4, 6, 5, 8, 6, 8, 7},
        {6, 5, 6, 6, 6, 6, 6}, {7, 5, 5, 4, 6, 5, 4},
        {4, 7, 4, 7, 5, 7, 6}, {5, 5, 7, 5, 6, 5, 6},
        {2, 6, 8, 8, 6, 7, 8}, {9, 5, 5, 3, 6, 4, 4},
        {5, 8, 4, 6, 7, 7, 5}, {3, 7, 6, 7, 6, 7, 7},
        {8, 6, 4, 5, 6, 5, 5}, {4, 4, 9, 8, 6, 6, 8},
        {3, 6, 5, 9, 5, 7, 8}, {6, 7, 6, 4, 7, 6, 5}
    };
    int base_count = (int)(sizeof(traits) / sizeof(traits[0]));
    if (index < base_count) return traits[index][metric];
    return 4 + ((index * 3 + metric * 5) % 5);
}

void simulation_seed_default_civilizations(void) {
    int requested = initial_civ_count;
    int count = clamp(requested, 0, MAX_CIVS);
    int placed = 0;
    int i;

    if (requested > MAX_CIVS) {
        event_log_push_structured(EVENT_TYPE_WORLD_GENERATION_NOTICE, EVENT_SEVERITY_WARNING,
                                  -1, -1, 1, -1, requested, MAX_CIVS, "");
    }
    for (i = 0; i < count; i++) {
        int ok = add_civilization_at("", default_symbol_for_index(i),
                                     default_trait_for_index(i, 0), default_trait_for_index(i, 1),
                                     default_trait_for_index(i, 2), default_trait_for_index(i, 3),
                                     default_trait_for_index(i, 4), default_trait_for_index(i, 5),
                                     default_trait_for_index(i, 6), -1, -1);
        if (ok) placed++;
    }
    if (placed < count) {
        event_log_push_structured(EVENT_TYPE_WORLD_GENERATION_NOTICE, EVENT_SEVERITY_WARNING,
                                  -1, -1, 2, -1, placed, count, "");
    }
}
