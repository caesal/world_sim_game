#ifndef WORLD_SIM_COUNTRY_NAMES_H
#define WORLD_SIM_COUNTRY_NAMES_H

#define COUNTRY_NAME_COUNT 200

typedef struct {
    const char *en;
    const char *zh;
} CountryNameRule;

extern const CountryNameRule COUNTRY_NAME_RULES[COUNTRY_NAME_COUNT];

const char *country_name_localized(int name_id, int language);
int country_name_find_by_text(const char *name);

#endif
