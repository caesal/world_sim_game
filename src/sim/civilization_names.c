#include "sim/simulation.h"

#include "data/country_names.h"
#include "ui/ui_types.h"

#include <stdio.h>
#include <string.h>

static int is_default_manual_name(const char *name) {
    return !name || !name[0] || strcmp(name, "New Realm") == 0;
}

int civilization_heritage_or_default(int heritage) {
    return heritage == CIV_HERITAGE_EASTERN ? CIV_HERITAGE_EASTERN : CIV_HERITAGE_WESTERN;
}

int civilization_pick_unused_name_id_for_heritage(int heritage) {
    int used[COUNTRY_NAME_COUNT];
    int count;
    int i;

    heritage = civilization_heritage_or_default(heritage);
    count = country_name_count_for_heritage(heritage);
    count = min(count, COUNTRY_NAME_COUNT);
    memset(used, 0, sizeof(used));
    for (i = 0; i < civ_count; i++) {
        if (civs[i].heritage == heritage && !civs[i].custom_name &&
            civs[i].name_id >= 0 && civs[i].name_id < COUNTRY_NAME_COUNT) {
            used[civs[i].name_id] = 1;
        }
    }
    for (i = 0; i < count; i++) {
        int candidate = rnd(max(1, count));
        if (!used[candidate]) return candidate;
    }
    for (i = 0; i < count; i++) if (!used[i]) return i;
    return -1;
}

int civilization_pick_unused_name_id(void) {
    return civilization_pick_unused_name_id_for_heritage(CIV_HERITAGE_WESTERN);
}

void civilization_assign_generated_name_for_heritage(Civilization *civ, int heritage, int name_id) {
    if (!civ) return;
    heritage = civilization_heritage_or_default(heritage);
    civ->heritage = heritage;
    civ->custom_name = 0;
    civ->name_id = name_id;
    if (country_name_valid_for_heritage(heritage, name_id)) {
        snprintf(civ->name, NAME_LEN, "%s", country_name_localized_for_heritage(heritage, name_id, 0));
    } else {
        snprintf(civ->name, NAME_LEN, "Country %d", civ_count + 1);
    }
}

void civilization_assign_generated_name(Civilization *civ, int name_id) {
    civilization_assign_generated_name_for_heritage(civ, civ ? civ->heritage : CIV_HERITAGE_WESTERN, name_id);
}

void civilization_set_custom_name(Civilization *civ, const char *name) {
    if (!civ) return;
    civ->custom_name = 1;
    civ->name_id = -1;
    snprintf(civ->name, NAME_LEN, "%s", name && name[0] ? name : "Custom Country");
}

void civilization_apply_input_name(Civilization *civ, const char *name, int pick_default, int heritage) {
    int name_id;
    int found_heritage;

    if (!civ) return;
    civ->heritage = civilization_heritage_or_default(heritage);
    if (pick_default && is_default_manual_name(name)) {
        civilization_assign_generated_name_for_heritage(civ, civ->heritage,
                                                        civilization_pick_unused_name_id_for_heritage(civ->heritage));
        return;
    }
    if (country_name_find_by_text_any(name, &found_heritage, &name_id)) {
        civilization_assign_generated_name_for_heritage(civ, found_heritage, name_id);
    } else {
        civilization_set_custom_name(civ, name);
    }
}

const char *civilization_display_name_for_language(int civ_id, int language) {
    static char fallback[4][NAME_LEN];
    static int fallback_index = 0;
    Civilization *civ;

    if (civ_id < 0 || civ_id >= civ_count) return "";
    civ = &civs[civ_id];
    if (!civ->custom_name && country_name_valid_for_heritage(civ->heritage, civ->name_id)) {
        return country_name_localized_for_heritage(civ->heritage, civ->name_id, language);
    }
    if (!civ->custom_name && civ->name_id < 0) {
        char *buffer = fallback[fallback_index++ % 4];
        snprintf(buffer, NAME_LEN, language == 1 ? "国家 %d" : "Country %d", civ_id + 1);
        return buffer;
    }
    return civ->name;
}

const char *civilization_display_name(int civ_id) {
    return civilization_display_name_for_language(civ_id, ui_language);
}

void civilization_migrate_loaded_names(void) {
    int i;

    for (i = 0; i < civ_count; i++) {
        int heritage = civs[i].heritage;
        int name_id = civs[i].name_id;
        if (country_name_valid_for_heritage(heritage, name_id)) {
            civs[i].heritage = civilization_heritage_or_default(heritage);
            civs[i].custom_name = 0;
            continue;
        }
        if (country_name_find_by_text_any(civs[i].name, &heritage, &name_id)) {
            civs[i].heritage = heritage;
            civs[i].name_id = name_id;
            civs[i].custom_name = 0;
        } else {
            civs[i].heritage = CIV_HERITAGE_WESTERN;
            civs[i].name_id = -1;
            civs[i].custom_name = 1;
        }
    }
}
