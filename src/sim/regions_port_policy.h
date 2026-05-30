#ifndef WORLD_SIM_REGIONS_PORT_POLICY_H
#define WORLD_SIM_REGIONS_PORT_POLICY_H

typedef struct {
    int island_components_checked;
    int island_components_with_port;
    int forced_island_ports;
    int port_city_count;
    int normal_city_count;
} RegionPortPolicyStats;

void regions_port_policy_apply_all(void);
void regions_port_policy_last_stats(RegionPortPolicyStats *out_stats);

#endif
