#ifndef WORLD_SIM_TERRITORY_INTEGRITY_H
#define WORLD_SIM_TERRITORY_INTEGRITY_H

typedef struct {
    int capital_region;
    int owned_regions;
    int capital_connected_regions;
    int disconnected_components;
    int longest_disconnected_months;
    int best_candidate_civ;
    int best_candidate_score;
    int capital_connected_percent;
    int capital_core_port_count;
    int capital_core_network_count;
    int disconnected_has_port;
    int disconnected_has_network;
    int disconnected_network_matches_capital;
} TerritoryIntegrityStats;

void territory_integrity_reset(void);
void territory_integrity_repair_capitals(void);
void territory_integrity_update_year(void);
void territory_integrity_get_stats(int civ_id, TerritoryIntegrityStats *out);
int territory_integrity_region_is_capital_connected(int civ_id, int region_id);
int territory_integrity_region_score(int civ_id, int region_id);

#endif
