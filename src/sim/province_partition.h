#ifndef WORLD_SIM_PROVINCE_PARTITION_H
#define WORLD_SIM_PROVINCE_PARTITION_H

void province_mark_partition_dirty(int owner);
void province_mark_all_partitions_dirty(void);
void province_repartition_owner(int owner);
void province_repartition_dirty(void);

#endif
