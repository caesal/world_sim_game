#ifndef WORLD_SIM_ALLIANCE_NAMES_H
#define WORLD_SIM_ALLIANCE_NAMES_H

#include <stddef.h>

#define ALLIANCE_NAME_BASE_COUNT 192

int alliance_name_base_count(void);
const char *alliance_name_base_en(int index);
const char *alliance_name_base_zh(int index);
void alliance_format_name(int base_index, int suffix_number,
                          char *out_en, size_t out_en_size,
                          char *out_zh, size_t out_zh_size);

#endif
