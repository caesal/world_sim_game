#include "data/country_names.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COUNTRY_NAME_POOL_MAX 240

typedef struct {
    char en[96];
    char zh[96];
} CountryNameEntry;

static CountryNameEntry entries[CIV_HERITAGE_COUNT][COUNTRY_NAME_POOL_MAX];
static int entry_counts[CIV_HERITAGE_COUNT];
static int loaded;

static int normalize_heritage(int heritage) {
    return heritage == CIV_HERITAGE_EASTERN ? CIV_HERITAGE_EASTERN : CIV_HERITAGE_WESTERN;
}

static FILE *open_data_file(const char *name) {
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
}

static void load_pool(int heritage, const char *file_name) {
    char line[256];
    FILE *file = open_data_file(file_name);
    heritage = normalize_heritage(heritage);
    if (!file) return;
    while (entry_counts[heritage] < COUNTRY_NAME_POOL_MAX && fgets(line, sizeof(line), file)) {
        char *id = strtok(line, "\t");
        char *en = strtok(NULL, "\t");
        char *zh = strtok(NULL, "\t");
        CountryNameEntry *entry;
        if (!id || !en || !zh || atoi(id) <= 0) continue;
        entry = &entries[heritage][entry_counts[heritage]++];
        copy_field(entry->en, sizeof(entry->en), en);
        copy_field(entry->zh, sizeof(entry->zh), zh);
    }
    fclose(file);
}

static void load_country_names(void) {
    if (loaded) return;
    loaded = 1;
    load_pool(CIV_HERITAGE_WESTERN, "country_names_western_200_bilingual.tsv");
    load_pool(CIV_HERITAGE_EASTERN, "country_names_eastern_200_bilingual.tsv");
}

int country_name_count_for_heritage(int heritage) {
    load_country_names();
    return entry_counts[normalize_heritage(heritage)];
}

int country_name_valid_for_heritage(int heritage, int name_id) {
    load_country_names();
    heritage = normalize_heritage(heritage);
    return name_id >= 0 && name_id < entry_counts[heritage];
}

const char *country_name_localized_for_heritage(int heritage, int name_id, int language) {
    load_country_names();
    heritage = normalize_heritage(heritage);
    if (!country_name_valid_for_heritage(heritage, name_id)) return "";
    return language == 1 ? entries[heritage][name_id].zh : entries[heritage][name_id].en;
}

int country_name_find_by_text_any(const char *name, int *out_heritage, int *out_name_id) {
    int h;
    int i;
    if (!name || !name[0]) return 0;
    load_country_names();
    for (h = 0; h < CIV_HERITAGE_COUNT; h++) {
        for (i = 0; i < entry_counts[h]; i++) {
            if (strcmp(name, entries[h][i].en) == 0 || strcmp(name, entries[h][i].zh) == 0) {
                if (out_heritage) *out_heritage = h;
                if (out_name_id) *out_name_id = i;
                return 1;
            }
        }
    }
    return 0;
}

const char *country_name_localized(int name_id, int language) {
    return country_name_localized_for_heritage(CIV_HERITAGE_WESTERN, name_id, language);
}

int country_name_find_by_text(const char *name) {
    int heritage;
    int name_id;
    if (!country_name_find_by_text_any(name, &heritage, &name_id)) return -1;
    return heritage == CIV_HERITAGE_WESTERN ? name_id : -1;
}
