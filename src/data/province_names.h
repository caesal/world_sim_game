#ifndef WORLD_SIM_PROVINCE_NAMES_H
#define WORLD_SIM_PROVINCE_NAMES_H

typedef enum {
    PROVINCE_NAME_GEOGRAPHIC = 0,
    PROVINCE_NAME_EPIC,
    PROVINCE_NAME_PLACE,
    PROVINCE_NAME_MARCH,
    PROVINCE_NAME_RIVER,
    PROVINCE_NAME_MIXED
} ProvinceNameType;

#define PROVINCE_NAME_TAG_COAST    (1u << 0)
#define PROVINCE_NAME_TAG_HARBOR   (1u << 1)
#define PROVINCE_NAME_TAG_RIVER    (1u << 2)
#define PROVINCE_NAME_TAG_ESTUARY  (1u << 3)
#define PROVINCE_NAME_TAG_MOUNTAIN (1u << 4)
#define PROVINCE_NAME_TAG_FOREST   (1u << 5)
#define PROVINCE_NAME_TAG_WETLAND  (1u << 6)
#define PROVINCE_NAME_TAG_DESERT   (1u << 7)
#define PROVINCE_NAME_TAG_BORDER   (1u << 8)
#define PROVINCE_NAME_TAG_CORE     (1u << 9)
#define PROVINCE_NAME_TAG_LAKE     (1u << 10)
#define PROVINCE_NAME_TAG_HIGHLAND (1u << 11)
#define PROVINCE_NAME_TAG_LOWLAND  (1u << 12)

int province_name_count(void);
int province_name_lint_warnings(void);
int province_name_valid_id(int name_id);
const char *province_name_localized(int name_id, int language);
const char *province_display_name(int region_id, int language);
void province_names_assign_all(void);
void province_names_assign_missing(void);

#endif
