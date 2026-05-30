#ifndef WORLD_SIM_REGIONS_SETTLEMENT_H
#define WORLD_SIM_REGIONS_SETTLEMENT_H

typedef struct {
    int regions_total;
    int city_slots;
    int missing_or_invalid_city_id;
    int duplicated_region_city_id;
    int city_outside_region;
    int owner_mismatch;
    int neutral_city_count;
    int normal_city_count;
    int port_city_count;
} RegionSettlementStats;

int regions_raw_region_for_city(int city_id);
int regions_city_is_local_to_region(int city_id, int region_id);
int regions_local_city_id(int region_id);
int regions_ensure_local_city_slot(int region_id);
int regions_activate_local_city(int region_id, int owner, int population, int capital, int allow_create);
void regions_deactivate_local_city(int region_id);
void regions_repair_local_city_slots(int repair_owned_regions);
void regions_refresh_province_ids_from_regions(void);
void regions_settlement_collect_stats(RegionSettlementStats *out_stats);

#endif
