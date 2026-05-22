#ifndef WORLD_SIM_COUNTRY_NAMES_H
#define WORLD_SIM_COUNTRY_NAMES_H

#include "core/sim_types.h"

#define COUNTRY_NAME_COUNT 200

typedef struct {
    const char *en;
    const char *zh;
} CountryNameRule;

int country_name_count_for_heritage(int heritage);
int country_name_valid_for_heritage(int heritage, int name_id);
const char *country_name_localized_for_heritage(int heritage, int name_id, int language);
int country_name_find_by_text_any(const char *name, int *out_heritage, int *out_name_id);
const char *country_name_localized(int name_id, int language);
int country_name_find_by_text(const char *name);

#endif
