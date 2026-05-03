#include "game_tables.h"

#define MAKE_GEOGRAPHY_RULE(id, en, zh, food, livestock, wood, stone, ore, water, population, money, habitability, attack, defense, note_en, note_zh) \
    [id] = {{en, zh}, {food, livestock, wood, stone, ore, water, population, money, habitability, attack, defense}, {note_en, note_zh}},

#define MAKE_CLIMATE_RULE(id, en, zh, food, livestock, wood, stone, ore, water, population, money, habitability, attack, defense, note_en, note_zh) \
    [id] = {{en, zh}, {food, livestock, wood, stone, ore, water, population, money, habitability, attack, defense}, {note_en, note_zh}},

#define MAKE_ECOLOGY_RULE(id, en, zh, food, livestock, wood, stone, ore, water, population, money, habitability, attack, defense, note_en, note_zh) \
    [id] = {{en, zh}, {food, livestock, wood, stone, ore, water, population, money, habitability, attack, defense}, {note_en, note_zh}},

#define MAKE_RESOURCE_RULE(id, en, zh, food, livestock, wood, stone, ore, water, population, money, habitability, attack, defense, note_en, note_zh) \
    [id] = {{en, zh}, {food, livestock, wood, stone, ore, water, population, money, habitability, attack, defense}, {note_en, note_zh}},

#define MAKE_METRIC_RULE(id, metric, zh, ability_en, ability_zh, high_en, high_zh, low_en, low_zh) \
    [id] = {metric, {metric, zh}, {ability_en, ability_zh}, {high_en, high_zh}, {low_en, low_zh}},

const GeographyRule GEOGRAPHY_RULES[GEO_COUNT] = {
    WORLD_GEOGRAPHY_RULE_ROWS(MAKE_GEOGRAPHY_RULE)
};

const ClimateRule CLIMATE_RULES[CLIMATE_COUNT] = {
    WORLD_CLIMATE_RULE_ROWS(MAKE_CLIMATE_RULE)
};

const EcologyRule ECOLOGY_RULES[ECO_COUNT] = {
    WORLD_ECOLOGY_RULE_ROWS(MAKE_ECOLOGY_RULE)
};

const ResourceFeatureRule RESOURCE_FEATURE_RULES[RESOURCE_COUNT] = {
    WORLD_RESOURCE_FEATURE_RULE_ROWS(MAKE_RESOURCE_RULE)
};

const CivilizationMetricRule CIVILIZATION_METRIC_RULES[CIV_METRIC_COUNT] = {
    CIVILIZATION_METRIC_ROWS(MAKE_METRIC_RULE)
};

const char *localized_text(LocalizedText text, int language) {
    return language == UI_LANG_ZH ? text.zh : text.en;
}
