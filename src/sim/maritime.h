#ifndef WORLD_SIM_MARITIME_H
#define WORLD_SIM_MARITIME_H

#include "core/game_types.h"

typedef struct {
    int port_candidate_regions;
    int shallow_reachable_regions;
    int maritime_reachable_regions;
    int deep_reachable_regions;
    int blocked_no_port;
    int blocked_no_sea_entry;
    int blocked_same_shallow_region;
    int blocked_city_at_capital;
    int blocked_no_path;
    int blocked_low_score;
    int blocked_city_cap;
} MaritimeExpansionDiagnostics;

void maritime_reset(void);
void maritime_mark_routes_dirty(void);
void maritime_ensure_routes(void);
int maritime_route_revision(void);
void maritime_rebuild_routes(void);
void maritime_update_migration(void);
int maritime_route_between_cities(int city_a, int city_b, int *distance);
int maritime_has_contact(int civ_a, int civ_b);
int maritime_trade_bonus(int civ_a, int civ_b);
void maritime_expansion_diagnostics(int civ_id, int resource_score, MaritimeExpansionDiagnostics *out);
void maritime_try_overseas_expansion(int civ_id, int resource_score, char *log, size_t log_size);

#endif
