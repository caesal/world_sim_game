#include "data/province_names.h"

#include "core/game_types.h"
#include "core/sim_types.h"
#include "sim/regions.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PROVINCE_NAME_MAX 1100
#define PROVINCE_NAME_TOP_CHOICES 12

typedef struct {
    int id;
    ProvinceNameType type;
    unsigned int tags;
    char en[96];
    char zh[96];
} ProvinceNameEntry;

static ProvinceNameEntry entries[CIV_HERITAGE_COUNT][PROVINCE_NAME_MAX];
static int entry_counts[CIV_HERITAGE_COUNT];
static int loaded;
static int lint_warnings;

static int valid_heritage_or_default(int heritage) {
    return heritage >= 0 && heritage < CIV_HERITAGE_COUNT ? heritage : CIV_HERITAGE_WESTERN;
}

static int normalize_heritage(int heritage) {
    heritage = valid_heritage_or_default(heritage);
    return heritage == CIV_HERITAGE_EASTERN || heritage == CIV_HERITAGE_SOUTHERN ?
        CIV_HERITAGE_EASTERN : CIV_HERITAGE_WESTERN;
}

static int contains_text(const char *text, const char *needle) {
    return text && needle && strstr(text, needle) != NULL;
}

static void lower_ascii(char *out, size_t out_size, const char *in) {
    size_t i;
    if (out_size == 0) return;
    for (i = 0; i + 1 < out_size && in && in[i]; i++) out[i] = (char)tolower((unsigned char)in[i]);
    out[i] = '\0';
}

static int contains_ascii_word(const char *lower, const char *needle) {
    return contains_text(lower, needle);
}

static void infer_entry_tags(ProvinceNameEntry *entry) {
    char lower[128];
    unsigned int tags = entry->tags;
    ProvinceNameType type = PROVINCE_NAME_MIXED;
    lower_ascii(lower, sizeof(lower), entry->en);
    if (contains_ascii_word(lower, "range") || contains_ascii_word(lower, "mount") ||
        contains_ascii_word(lower, "fells") || contains_text(entry->zh, "山")) tags |= PROVINCE_NAME_TAG_MOUNTAIN;
    if (contains_ascii_word(lower, "high") || contains_text(entry->zh, "高地")) tags |= PROVINCE_NAME_TAG_HIGHLAND;
    if (contains_ascii_word(lower, "low") || contains_text(entry->zh, "低地")) tags |= PROVINCE_NAME_TAG_LOWLAND;
    if (contains_ascii_word(lower, "forest") || contains_ascii_word(lower, "grove") ||
        contains_text(entry->zh, "林")) tags |= PROVINCE_NAME_TAG_FOREST;
    if (contains_ascii_word(lower, "wet") || contains_ascii_word(lower, "swamp") ||
        contains_text(entry->zh, "湿")) tags |= PROVINCE_NAME_TAG_WETLAND;
    if (contains_ascii_word(lower, "desert") || contains_ascii_word(lower, "barren") ||
        contains_text(entry->zh, "沙") || contains_text(entry->zh, "荒")) tags |= PROVINCE_NAME_TAG_DESERT;
    if (contains_ascii_word(lower, "shore") || contains_ascii_word(lower, "coast") ||
        contains_ascii_word(lower, "harbor") || contains_text(entry->zh, "海岸")) tags |= PROVINCE_NAME_TAG_COAST;
    if (contains_ascii_word(lower, "harbor") || contains_text(entry->zh, "港")) tags |= PROVINCE_NAME_TAG_HARBOR;
    if (contains_ascii_word(lower, "river") || contains_ascii_word(lower, "headwater") ||
        contains_ascii_word(lower, "flow") || contains_text(entry->zh, "河")) tags |= PROVINCE_NAME_TAG_RIVER;
    if (contains_ascii_word(lower, "estuary") || contains_text(entry->zh, "入海口")) tags |= PROVINCE_NAME_TAG_ESTUARY;
    if (contains_ascii_word(lower, "lake") || contains_text(entry->zh, "湖")) tags |= PROVINCE_NAME_TAG_LAKE;
    if (contains_ascii_word(lower, "march") || contains_ascii_word(lower, "gate") ||
        contains_ascii_word(lower, "frontier") || contains_ascii_word(lower, "watch") ||
        contains_ascii_word(lower, "banner") || contains_text(entry->zh, "边境") ||
        contains_text(entry->zh, "关门") || contains_text(entry->zh, "守望")) tags |= PROVINCE_NAME_TAG_BORDER;
    if (contains_ascii_word(lower, "crown") || contains_ascii_word(lower, "sanctuary") ||
        contains_ascii_word(lower, "citadel") || contains_ascii_word(lower, "throne") ||
        contains_text(entry->zh, "王畿") || contains_text(entry->zh, "圣域")) tags |= PROVINCE_NAME_TAG_CORE;

    if (tags & (PROVINCE_NAME_TAG_MOUNTAIN | PROVINCE_NAME_TAG_FOREST | PROVINCE_NAME_TAG_LAKE |
                PROVINCE_NAME_TAG_HIGHLAND | PROVINCE_NAME_TAG_LOWLAND | PROVINCE_NAME_TAG_DESERT)) {
        type = PROVINCE_NAME_GEOGRAPHIC;
    }
    if (tags & (PROVINCE_NAME_TAG_RIVER | PROVINCE_NAME_TAG_ESTUARY)) type = PROVINCE_NAME_RIVER;
    if (tags & PROVINCE_NAME_TAG_BORDER) type = PROVINCE_NAME_MARCH;
    if (contains_ascii_word(lower, "north") || contains_ascii_word(lower, "south") ||
        contains_ascii_word(lower, "east") || contains_ascii_word(lower, "west") ||
        contains_ascii_word(lower, "upper") || contains_ascii_word(lower, "lower")) type = PROVINCE_NAME_PLACE;
    if (contains_ascii_word(lower, "covenant") || contains_ascii_word(lower, "crown") ||
        contains_ascii_word(lower, "citadel") || contains_ascii_word(lower, "dragon") ||
        contains_ascii_word(lower, "spire") || contains_ascii_word(lower, "throne")) type = PROVINCE_NAME_EPIC;
    if (contains_ascii_word(lower, " and the ") || contains_ascii_word(lower, "vale of")) type = PROVINCE_NAME_MIXED;
    entry->type = type;
    entry->tags = tags;
}

static FILE *open_name_file(int heritage) {
    const char *name = normalize_heritage(heritage) == CIV_HERITAGE_EASTERN ?
        "province_names_eastern_1000_bilingual.tsv" : "province_names_western_1000_bilingual.tsv";
    char path[160];
    FILE *file;
    snprintf(path, sizeof(path), "data/%s", name);
    file = fopen(path, "rb");
    if (file) return file;
    snprintf(path, sizeof(path), "../data/%s", name);
    return fopen(path, "rb");
}

static void copy_field(char *out, size_t out_size, const char *field) {
    size_t len = field ? strlen(field) : 0;
    while (len > 0 && (field[len - 1] == '\n' || field[len - 1] == '\r')) len--;
    if (len >= out_size) len = out_size - 1;
    memcpy(out, field ? field : "", len);
    out[len] = '\0';
    if (len == 0) lint_warnings++;
}

static void load_pool(int heritage) {
    char line[320];
    FILE *file = open_name_file(heritage);
    ProvinceNameEntry *pool;
    int *count;
    heritage = normalize_heritage(heritage);
    if (!file) return;
    pool = entries[heritage];
    count = &entry_counts[heritage];
    while (*count < PROVINCE_NAME_MAX && fgets(line, sizeof(line), file)) {
        char *id = strtok(line, "\t");
        char *type = strtok(NULL, "\t");
        char *tags = strtok(NULL, "\t");
        char *en = strtok(NULL, "\t");
        char *zh = strtok(NULL, "\t");
        ProvinceNameEntry *entry;
        (void)type;
        if (!id || !tags || !en || !zh) { lint_warnings++; continue; }
        entry = &pool[*count];
        memset(entry, 0, sizeof(*entry));
        entry->id = atoi(id);
        entry->tags = (unsigned int)strtoul(tags, NULL, 0);
        copy_field(entry->en, sizeof(entry->en), en);
        copy_field(entry->zh, sizeof(entry->zh), zh);
        infer_entry_tags(entry);
        (*count)++;
    }
    fclose(file);
    for (int i = 0; i < *count; i++) {
        for (int j = i + 1; j < *count; j++) {
            if (strcmp(pool[i].en, pool[j].en) == 0 ||
                strcmp(pool[i].zh, pool[j].zh) == 0) lint_warnings++;
        }
    }
}

static void load_names(void) {
    if (loaded) return;
    loaded = 1;
    load_pool(CIV_HERITAGE_WESTERN);
    load_pool(CIV_HERITAGE_EASTERN);
}

int province_name_count(void) {
    return province_name_count_for_heritage(CIV_HERITAGE_WESTERN);
}

int province_name_count_for_heritage(int heritage) {
    load_names();
    return entry_counts[normalize_heritage(heritage)];
}

int province_name_lint_warnings(void) {
    load_names();
    return lint_warnings;
}

int province_name_valid_id(int name_id) {
    return province_name_valid_id_for_heritage(CIV_HERITAGE_WESTERN, name_id);
}

int province_name_valid_id_for_heritage(int heritage, int name_id) {
    load_names();
    heritage = normalize_heritage(heritage);
    return name_id >= 0 && name_id < entry_counts[heritage];
}

const char *province_name_localized(int name_id, int language) {
    return province_name_localized_for_heritage(CIV_HERITAGE_WESTERN, name_id, language);
}

const char *province_name_localized_for_heritage(int heritage, int name_id, int language) {
    load_names();
    heritage = normalize_heritage(heritage);
    if (!province_name_valid_id_for_heritage(heritage, name_id)) return "";
    return language == 1 ? entries[heritage][name_id].zh : entries[heritage][name_id].en;
}

static unsigned int region_context_tags(const NaturalRegion *region) {
    unsigned int tags = 0;
    if (region->has_port_site) tags |= PROVINCE_NAME_TAG_COAST | PROVINCE_NAME_TAG_HARBOR;
    if (region->dominant_geography == GEO_MOUNTAIN || region->dominant_geography == GEO_PLATEAU) tags |= PROVINCE_NAME_TAG_MOUNTAIN | PROVINCE_NAME_TAG_HIGHLAND;
    if (region->dominant_geography == GEO_HILL) tags |= PROVINCE_NAME_TAG_HIGHLAND;
    if (region->dominant_geography == GEO_BASIN) tags |= PROVINCE_NAME_TAG_LOWLAND;
    if (region->dominant_geography == GEO_LAKE) tags |= PROVINCE_NAME_TAG_LAKE;
    if (region->dominant_geography == GEO_WETLAND || region->dominant_ecology == ECO_SWAMP) tags |= PROVINCE_NAME_TAG_WETLAND;
    if (region->dominant_climate == CLIMATE_DESERT || region->dominant_climate == CLIMATE_SEMI_ARID ||
        region->dominant_ecology == ECO_DESERT) tags |= PROVINCE_NAME_TAG_DESERT;
    if (region->dominant_ecology == ECO_FOREST || region->dominant_ecology == ECO_RAINFOREST ||
        region->dominant_ecology == ECO_BAMBOO) tags |= PROVINCE_NAME_TAG_FOREST;
    if (region->average_stats.water >= 6) tags |= PROVINCE_NAME_TAG_RIVER;
    if (region->development_score >= 120) tags |= PROVINCE_NAME_TAG_CORE;
    if (region->neighbor_count <= 2 || region->natural_defense >= 14) tags |= PROVINCE_NAME_TAG_BORDER;
    return tags;
}

static int entry_score(const ProvinceNameEntry *entry, const NaturalRegion *region, int region_id) {
    unsigned int context = region_context_tags(region);
    unsigned int matched = entry->tags & context;
    int score = 10;
    int bit;
    for (bit = 0; bit < 13; bit++) if (matched & (1u << bit)) score += 20;
    if (entry->type == PROVINCE_NAME_GEOGRAPHIC) score += 6;
    if ((context & PROVINCE_NAME_TAG_BORDER) && entry->type == PROVINCE_NAME_MARCH) score += 18;
    if ((context & PROVINCE_NAME_TAG_RIVER) && entry->type == PROVINCE_NAME_RIVER) score += 18;
    if ((context & PROVINCE_NAME_TAG_CORE) && entry->type == PROVINCE_NAME_EPIC) score += 10;
    if (!(context & (PROVINCE_NAME_TAG_COAST | PROVINCE_NAME_TAG_MOUNTAIN |
                     PROVINCE_NAME_TAG_RIVER | PROVINCE_NAME_TAG_BORDER)) &&
        (entry->type == PROVINCE_NAME_PLACE || entry->type == PROVINCE_NAME_MIXED)) score += 8;
    score += (int)((region_id * 1103515245u + (unsigned int)entry->id * 2654435761u) % 17u);
    return score;
}

static void remember_top(int *ids, int *scores, int *count, int id, int score) {
    int pos;
    if (*count < PROVINCE_NAME_TOP_CHOICES) {
        pos = (*count)++;
    } else {
        int worst = 0;
        for (int i = 1; i < *count; i++) if (scores[i] < scores[worst]) worst = i;
        if (score <= scores[worst]) return;
        pos = worst;
    }
    ids[pos] = id;
    scores[pos] = score;
}

static int choose_name_id(int heritage, const NaturalRegion *region, int region_id, unsigned char *used) {
    int ids[PROVINCE_NAME_TOP_CHOICES];
    int scores[PROVINCE_NAME_TOP_CHOICES];
    int count = 0;
    int fallback = -1;
    ProvinceNameEntry *pool;
    int pool_count;
    heritage = normalize_heritage(heritage);
    load_names();
    pool = entries[heritage];
    pool_count = entry_counts[heritage];
    for (int i = 0; i < pool_count; i++) {
        int score;
        if (used && used[i]) continue;
        score = entry_score(&pool[i], region, region_id);
        remember_top(ids, scores, &count, i, score);
        if (fallback < 0) fallback = i;
    }
    if (count > 0) {
        unsigned int pick = (unsigned int)(region_id * 977 + region->tile_count * 37 + region->development_score);
        return ids[pick % (unsigned int)count];
    }
    return fallback >= 0 ? fallback : (pool_count > 0 ? region_id % pool_count : -1);
}

static void collect_used(int heritage, unsigned char *used) {
    int pool_heritage = normalize_heritage(heritage);
    memset(used, 0, PROVINCE_NAME_MAX);
    for (int i = 0; i < region_count; i++) {
        int id = natural_regions[i].name_id;
        if (normalize_heritage(natural_regions[i].name_heritage) != pool_heritage) continue;
        if (province_name_valid_id_for_heritage(heritage, id)) used[id] = 1;
    }
}

static void assign_names(int overwrite_all) {
    static unsigned char used[PROVINCE_NAME_MAX];
    load_names();
    for (int h = 0; h < CIV_HERITAGE_COUNT; h++) {
        memset(used, 0, sizeof(used));
        for (int i = 0; i < region_count; i++) {
            int id = natural_regions[i].name_id;
            if (natural_regions[i].name_heritage != h) continue;
            if (overwrite_all || !province_name_valid_id_for_heritage(h, id) || used[id]) natural_regions[i].name_id = -1;
            else used[id] = 1;
        }
        for (int i = 0; i < region_count; i++) {
            NaturalRegion *region = &natural_regions[i];
            int id;
            if (!region->alive || region->tile_count <= 0 || region->name_id >= 0 ||
                region->name_heritage != h) continue;
            id = choose_name_id(h, region, i, used);
            region->name_id = id;
            if (province_name_valid_id_for_heritage(h, id)) used[id] = 1;
        }
    }
}

void province_names_assign_all(void) {
    assign_names(1);
}

void province_names_assign_missing(void) {
    assign_names(0);
}

int province_names_assign_region_for_heritage(int region_id, int heritage) {
    static unsigned char used[PROVINCE_NAME_MAX];
    NaturalRegion *region;
    int id;
    heritage = valid_heritage_or_default(heritage);
    if (region_id < 0 || region_id >= region_count) return 0;
    region = &natural_regions[region_id];
    if (!region->alive || region->tile_count <= 0) return 0;
    if (province_name_valid_id_for_heritage(region->name_heritage, region->name_id)) return 1;
    load_names();
    collect_used(heritage, used);
    id = choose_name_id(heritage, region, region_id, used);
    if (!province_name_valid_id_for_heritage(heritage, id)) return 0;
    region->name_heritage = heritage;
    region->name_id = id;
    return 1;
}

void province_names_assign_missing_for_owned_regions(void) {
    for (int i = 0; i < region_count; i++) {
        NaturalRegion *region = &natural_regions[i];
        int owner = region->owner_civ;
        if (!region->alive || region->tile_count <= 0 || region->name_id >= 0) continue;
        if (owner >= 0 && owner < MAX_CIVS) province_names_assign_region_for_heritage(i, civs[owner].heritage);
    }
}

const char *province_display_name(int region_id, int language) {
    static char fallback[64];
    const NaturalRegion *region = regions_get(region_id);
    if (region && province_name_valid_id_for_heritage(region->name_heritage, region->name_id)) {
        return province_name_localized_for_heritage(region->name_heritage, region->name_id, language);
    }
    if (language == 1) {
        snprintf(fallback, sizeof(fallback), "自然区域 %d", region_id + 1);
        return fallback;
    }
    snprintf(fallback, sizeof(fallback), language == 1 ? "自然区域 %d" : "Natural Region %d", region_id + 1);
    return fallback;
}
