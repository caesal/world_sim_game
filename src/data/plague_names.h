#ifndef WORLD_SIM_PLAGUE_NAMES_H
#define WORLD_SIM_PLAGUE_NAMES_H

#include <stddef.h>

typedef struct {
    const char *zh;
    const char *en;
} PlagueName;

int plague_names_count(void);
const PlagueName *plague_names_get(int name_id);
const char *plague_names_text(int name_id, int language);
int plague_names_format(int name_id, int cycle, int language,
                        char *out, size_t out_size);

#endif
