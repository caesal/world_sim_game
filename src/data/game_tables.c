#include "game_tables.h"

#define LOCALIZED_LANGUAGE_ZH 1

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

const ResourceFeatureRule RESOURCE_FEATURE_RULES[RESOURCE_FEATURE_COUNT] = {
    WORLD_RESOURCE_FEATURE_RULE_ROWS(MAKE_RESOURCE_RULE)
};

const CivilizationMetricRule CIVILIZATION_METRIC_RULES[CIV_METRIC_COUNT] = {
    CIVILIZATION_METRIC_ROWS(MAKE_METRIC_RULE)
};

const TechnologyStageRule TECHNOLOGY_STAGE_RULES[10] = {
    {{"Settlement Age", "聚落时代"}, {"Expansion speed x1.1.", "扩张速度 x1.1。"}},
    {{"Regional Age", "区域时代"}, {"Expansion speed x1.2.", "扩张速度 x1.2。"}},
    {{"Craft Age", "工艺时代"}, {"Resource output x1.05.", "资源倍率 x1.05。"}},
    {{"Market Age", "市场时代"}, {"Resource output x1.10.", "资源倍率 x1.10。"}},
    {{"Administrative Age", "行政时代"}, {"Resource output up to x1.20.", "资源倍率最高 x1.20。"}},
    {{"Ocean Age", "远洋时代"}, {"Deep sea routes unlock at 50% stability.", "解锁深海航线，稳定性 50%。"}},
    {{"Navigation Age", "航海时代"}, {"Deep sea route stability rises to 80%.", "深海航线稳定性提升到 80%。"}},
    {{"Engineering Age", "工程时代"}, {"Deep sea 90%, tech +10%, defense army x1.2.", "深海 90%，科技 +10%，防御军队 x1.2。"}},
    {{"Strategic Age", "战略时代"}, {"Deep sea 99%, battle chance +15%.", "深海 99%，战斗胜率机会 +15%。"}},
    {{"Imperial Age", "帝国时代"}, {"Long-held vassals can be annexed.", "长期附庸可被吞并。"}}
};

const char *localized_text(LocalizedText text, int language) {
    return language == LOCALIZED_LANGUAGE_ZH ? text.zh : text.en;
}
