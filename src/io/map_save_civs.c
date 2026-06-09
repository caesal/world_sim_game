#include "io/map_save_civs.h"

#include "core/game_state.h"
#include "io/map_save_legacy.h"
#include "sim/economy.h"

#include <stddef.h>
#include <string.h>

static int read_block(FILE *file, void *data, size_t size, size_t count) {
    return fread(data, size, count, file) == count;
}

static void apply_loaded_defaults(int civ_id, int old_save) {
    if (civ_id < 0 || civ_id >= civ_count) return;
    if (civs[civ_id].heritage < 0 || civs[civ_id].heritage >= CIV_HERITAGE_COUNT) {
        civs[civ_id].heritage = CIV_HERITAGE_WESTERN;
    }
    civs[civ_id].tech_stage = clamp(civs[civ_id].tech_stage, 0, 10);
    if (civs[civ_id].tech_progress < 0) civs[civ_id].tech_progress = 0;
    if (civs[civ_id].tech_stage >= 10) civs[civ_id].tech_progress = 0;
    economy_normalize_civ(civ_id);
    if (old_save && civs[civ_id].treasury_cap > 0 && civs[civ_id].treasury == 0 &&
        civs[civ_id].treasury_pending_surplus == 0 && civs[civ_id].resource_pressure == 0) {
        civs[civ_id].treasury = civs[civ_id].treasury_cap / 4;
    }
}

int map_save_read_civilizations(FILE *file, int save_version, int count) {
    int i;
    if (save_version >= 12) return read_block(file, civs, sizeof(Civilization), (size_t)count);
    if (save_version >= 9) {
        size_t old_size = offsetof(Civilization, treasury);
        for (i = 0; i < count; i++) {
            memset(&civs[i], 0, sizeof(civs[i]));
            if (!read_block(file, &civs[i], old_size, 1)) return 0;
        }
        return 1;
    }
    if (save_version >= 6) {
        size_t old_size = offsetof(Civilization, heritage);
        for (i = 0; i < count; i++) {
            memset(&civs[i], 0, sizeof(civs[i]));
            if (!read_block(file, &civs[i], old_size, 1)) return 0;
            civs[i].heritage = CIV_HERITAGE_WESTERN;
        }
        return 1;
    }
    if (save_version >= 4) {
        LegacyCivilizationV5 legacy[MAX_CIVS];
        if (!read_block(file, legacy, sizeof(LegacyCivilizationV5), (size_t)count)) return 0;
        for (i = 0; i < count; i++) {
            memset(&civs[i], 0, sizeof(civs[i]));
            memcpy(civs[i].name, legacy[i].name, NAME_LEN);
            civs[i].name_id = legacy[i].name_id;
            civs[i].custom_name = legacy[i].custom_name;
            memcpy(((char *)&civs[i]) + offsetof(Civilization, symbol),
                   ((char *)&legacy[i]) + offsetof(LegacyCivilizationV5, symbol),
                   sizeof(LegacyCivilizationV5) - offsetof(LegacyCivilizationV5, symbol));
        }
        return 1;
    }
    if (save_version == 3) {
        LegacyCivilizationV3 legacy[MAX_CIVS];
        if (!read_block(file, legacy, sizeof(LegacyCivilizationV3), (size_t)count)) return 0;
        for (i = 0; i < count; i++) { memset(&civs[i], 0, sizeof(civs[i])); memcpy(&civs[i], &legacy[i], sizeof(legacy[i])); }
        return 1;
    }
    if (save_version == 2) {
        LegacyCivilizationV2 legacy[MAX_CIVS];
        if (!read_block(file, legacy, sizeof(LegacyCivilizationV2), (size_t)count)) return 0;
        for (i = 0; i < count; i++) {
            memset(&civs[i], 0, sizeof(civs[i]));
            memcpy(&civs[i], &legacy[i], sizeof(legacy[i]));
            civs[i].plague_recovery_months = 36;
            civs[i].war_recovery_months = 36;
        }
        return 1;
    }
    {
        LegacyCivilizationV1 legacy[MAX_CIVS];
        if (!read_block(file, legacy, sizeof(LegacyCivilizationV1), (size_t)count)) return 0;
        for (i = 0; i < count; i++) {
            memset(&civs[i], 0, sizeof(civs[i]));
            memcpy(civs[i].name, legacy[i].name, NAME_LEN);
            civs[i].name[NAME_LEN - 1] = '\0';
            civs[i].name_id = -1; civs[i].custom_name = 1; civs[i].symbol = legacy[i].symbol;
            civs[i].color = legacy[i].color; civs[i].alive = legacy[i].alive;
            civs[i].population = legacy[i].population; civs[i].territory = legacy[i].territory;
            civs[i].aggression = legacy[i].aggression; civs[i].expansion = legacy[i].expansion;
            civs[i].defense = legacy[i].defense; civs[i].culture = legacy[i].culture;
            civs[i].governance = legacy[i].governance; civs[i].cohesion = legacy[i].cohesion;
            civs[i].production = legacy[i].production; civs[i].military = legacy[i].military;
            civs[i].commerce = legacy[i].commerce; civs[i].logistics = legacy[i].logistics;
            civs[i].innovation = legacy[i].innovation; civs[i].adaptation = legacy[i].adaptation;
            civs[i].tech_stage = legacy[i].tech_stage; civs[i].tech_progress = legacy[i].tech_progress;
            civs[i].deep_sea_route_unlocked_event_done = legacy[i].deep_sea_route_unlocked_event_done;
            civs[i].capital_city = legacy[i].capital_city; civs[i].disorder = legacy[i].disorder;
            civs[i].disorder_resource = legacy[i].disorder_resource;
            civs[i].disorder_plague = legacy[i].disorder_plague;
            civs[i].disorder_migration = legacy[i].disorder_migration;
            civs[i].disorder_stability = legacy[i].disorder_stability;
            civs[i].plague_recovery_months = 36; civs[i].war_recovery_months = 36;
        }
        return 1;
    }
}

void map_save_normalize_loaded_civilizations(int save_version) {
    int i;
    for (i = 0; i < civ_count; i++) apply_loaded_defaults(i, save_version < 12);
}
