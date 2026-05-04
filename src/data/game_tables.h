#ifndef WORLD_SIM_GAME_TABLES_H
#define WORLD_SIM_GAME_TABLES_H

#include "core/game_types.h"

typedef struct {
    const char *en;
    const char *zh;
} LocalizedText;

typedef struct {
    int food;
    int livestock;
    int wood;
    int stone;
    int minerals;
    int water;
    int population;
    int money;
    int habitability;
    int attack;
    int defense;
} TableStats;

typedef struct {
    LocalizedText name;
    TableStats stats;
    LocalizedText note;
} GeographyRule;

typedef struct {
    LocalizedText name;
    TableStats stats;
    LocalizedText note;
} ClimateRule;

typedef struct {
    LocalizedText name;
    TableStats stats;
    LocalizedText note;
} EcologyRule;

typedef struct {
    LocalizedText name;
    TableStats stats;
    LocalizedText note;
} ResourceFeatureRule;

typedef struct {
    const char *id;
    LocalizedText name;
    LocalizedText ability;
    LocalizedText high_effect;
    LocalizedText low_effect;
} CivilizationMetricRule;

typedef struct {
    LocalizedText name;
    int governance;
    int cohesion;
    int production;
    int military;
    int commerce;
    int logistics;
    int innovation;
    LocalizedText note;
} BirthEnvironmentRule;

/*
World model quick-edit reference.

Generation order:
1. Geography skeleton: continents, ocean, elevation, mountains, rivers, lakes, coast.
2. Climate: latitude, elevation, sea distance, moisture, drought, and mountain rain shadow.
3. Ecology and land features: climate plus geography decide forest, grassland, wetland, desert, tundra, etc.
4. Resources: geography plus climate plus ecology produce food, herd, wood, stone, ore, water, population, money.
5. Civilizations grow from good map positions; the map should not bend itself around civilizations.

Geography rows currently used:
Ocean | Coast | Plain | Hill | Mountain | Plateau | Basin | Canyon | Volcano | Lake | Bay | Delta | Wetland | Oasis | Island
Island means one island landform only; there is no multi-island geography category.

Climate rows currently used:
Tropical Rainforest | Tropical Monsoon | Tropical Savanna | Desert | Semi-Arid | Mediterranean | Oceanic |
Temperate Monsoon | Continental | Subarctic | Tundra | Ice Cap | Alpine | Highland Plateau

Resource columns:
food | herd | wood | stone | ore | water | population | money | habitability | attack | defense

Core civilization metrics:
governance: order, city administration, taxes, and controlled expansion.
cohesion: cultural identity, stability, assimilation, and resistance to collapse.
production: building, tools, gathering, and infrastructure.
military: army organization, weapons, and war readiness.
commerce: money, markets, ports, exchange, and trade routes.
logistics: movement, supply, migration, and distant expansion.
innovation: learning, better tools, systems, and late growth.

Dynamic civilization state:
adaptation is not a core metric. It changes over time from resources, terrain, climate, culture, disorder,
population pressure, and how well a civilization survives outside its original comfort zone.
*/

/*
Editable geography and resource table.

Columns:
id, English, Chinese, food, livestock, wood, stone, ore, water, population,
money, habitability, attack, defense, note_en, note_zh
*/
#define WORLD_GEOGRAPHY_RULE_ROWS(X) \
    X(GEO_OCEAN, "Ocean", "海洋", 0, 0, 0, 0, 0, 10, 0, 4, 0, 0, 0, "Sailing, climate, and trade water.", "航海、气候和贸易水域。") \
    X(GEO_COAST, "Coast", "海岸", 4, 1, 2, 1, 1, 8, 4, 7, 6, 0, 1, "Trade, ports, shipbuilding, and coastal settlement.", "适合贸易、港口、造船和沿海聚落。") \
    X(GEO_PLAIN, "Plain", "平原", 6, 5, 1, 1, 1, 4, 6, 3, 7, -1, 0, "Agriculture, cities, and population core.", "农业、城市和人口核心。") \
    X(GEO_HILL, "Hill", "丘陵", 3, 4, 3, 5, 4, 3, 3, 3, 4, 1, 2, "Balanced resource land for towns and defense.", "资源较均衡，适合小城镇和防御。") \
    X(GEO_MOUNTAIN, "Mountain", "山脉", 1, 1, 2, 7, 8, 3, 1, 3, 2, 2, 4, "Mining, stone, passes, and natural borders.", "矿业、石料、关隘和天然边界。") \
    X(GEO_PLATEAU, "Plateau", "高原", 2, 5, 2, 5, 5, 3, 2, 2, 4, 1, 2, "Pasture and isolated highland kingdoms.", "畜牧和封闭高地文明。") \
    X(GEO_BASIN, "Basin", "盆地", 4, 3, 2, 3, 3, 5, 4, 3, 5, 0, 1, "Closed settlement bowl, often fertile if watered.", "封闭聚落区域，有水时较肥沃。") \
    X(GEO_CANYON, "Canyon", "峡谷", 1, 1, 1, 6, 5, 3, 1, 2, 2, 2, 4, "Difficult terrain, ambushes, and hard borders.", "天险地形，适合伏击和防御关口。") \
    X(GEO_VOLCANO, "Volcano", "火山", 1, 1, 1, 7, 8, 2, 1, 4, 2, 2, 3, "Dangerous but rich in stone and minerals.", "危险但石料和矿石丰富。") \
    X(GEO_LAKE, "Lake", "湖泊", 3, 0, 1, 0, 1, 10, 2, 3, 2, 0, 1, "Fresh water, fishery, and stable settlements.", "淡水、渔业和稳定聚落。") \
    X(GEO_BAY, "Bay", "海湾", 3, 0, 1, 1, 1, 9, 3, 7, 4, 0, 1, "Natural harbor and commercial city site.", "天然港口，适合商业城市。") \
    X(GEO_DELTA, "Delta", "三角洲", 8, 2, 2, 1, 1, 9, 8, 5, 9, -1, 1, "Granary land and dense population zone.", "粮仓地区，适合高人口大城市。") \
    X(GEO_WETLAND, "Wetland", "湿地", 3, 1, 3, 1, 1, 9, 2, 2, 4, 0, 3, "Water-rich defensive habitat.", "水资源丰富，防御和生态强。") \
    X(GEO_OASIS, "Oasis", "绿洲", 5, 1, 1, 1, 1, 8, 4, 6, 7, 0, 1, "Desert survival and strategic trade center.", "沙漠中的生存和战略贸易核心。") \
    X(GEO_ISLAND, "Island", "岛屿", 3, 1, 2, 2, 2, 7, 3, 6, 5, 0, 2, "Single island landform with isolated culture and naval trade.", "单个岛屿地貌，适合孤立文化和海贸发展。")

/*
Editable climate effect table.

Columns:
id, English, Chinese, food, livestock, wood, stone, ore, water, population,
money, habitability, attack, defense, note_en, note_zh
*/
#define WORLD_CLIMATE_RULE_ROWS(X) \
    X(CLIMATE_TROPICAL_RAINFOREST, "Tropical Rainforest", "热带雨林", 2, -1, 5, 0, 1, 3, 3, 2, 1, 0, 2, "Warm, wet, rich in wood and water.", "全年高温多雨，木材和水丰富。") \
    X(CLIMATE_TROPICAL_MONSOON, "Tropical Monsoon", "热带季风", 4, 0, 2, 0, 1, 2, 4, 3, 2, 0, 1, "Wet season agriculture and flood plains.", "雨季农业和季节性洪水。") \
    X(CLIMATE_TROPICAL_SAVANNA, "Tropical Savanna", "热带草原", 3, 3, -1, 0, 1, 0, 3, 3, 2, -1, 0, "Warm grassland, herds, and migration.", "温暖草原，适合兽群和迁徙。") \
    X(CLIMATE_DESERT, "Desert", "沙漠", -4, -2, -4, 2, 2, -4, -4, 2, -4, 2, 0, "Extremely dry, low population, trade routes.", "极度干燥，低人口，适合商路。") \
    X(CLIMATE_SEMI_ARID, "Semi-Arid", "半干旱", -2, 3, -2, 1, 1, -2, -2, 2, -2, 1, 0, "Pasture edge between grassland and desert.", "草原和沙漠之间的牧场边缘。") \
    X(CLIMATE_MEDITERRANEAN, "Mediterranean", "地中海", 3, 1, 1, 1, 1, 0, 4, 5, 3, 0, 0, "Comfortable coastal city climate.", "适合沿海城邦和贸易。") \
    X(CLIMATE_OCEANIC, "Oceanic", "温带海洋", 3, 2, 4, 0, 1, 3, 5, 5, 3, 0, 1, "Moist, mild, ports and forests.", "温和湿润，适合港口和森林。") \
    X(CLIMATE_TEMPERATE_MONSOON, "Temperate Monsoon", "温带季风", 4, 1, 2, 0, 1, 2, 5, 3, 3, 0, 0, "Dense farming and city growth.", "农耕文明和高人口密度。") \
    X(CLIMATE_CONTINENTAL, "Continental", "温带大陆", 2, 2, 1, 1, 1, 0, 4, 2, 1, 0, 0, "Inland farms, plains, and frontier shifts.", "内陆农业、平原和边疆过渡。") \
    X(CLIMATE_SUBARCTIC, "Subarctic", "亚寒带", -1, -1, 3, 1, 1, 0, -2, -1, -1, 0, 1, "Cold forests and low population.", "寒冷针叶林，人口较低。") \
    X(CLIMATE_TUNDRA, "Tundra", "苔原", -3, 0, -3, 1, 1, -1, -4, -2, -2, 1, 1, "Frozen soil, herding, and hard living.", "冻土广布，适合寒地部落。") \
    X(CLIMATE_ICE_CAP, "Ice Cap", "冰原", -4, -3, -4, 1, 1, 0, -5, -3, -4, 2, 1, "Permanent cold and nearly no farming.", "终年严寒，几乎无法农业。") \
    X(CLIMATE_ALPINE, "Alpine", "高山", -2, -1, 0, 3, 3, 1, -3, 0, -3, 1, 3, "Altitude cold, mining, and defense.", "海拔降温，矿产和防御强。") \
    X(CLIMATE_HIGHLAND_PLATEAU, "Highland Plateau", "高原", -1, 3, 0, 2, 2, 0, -2, 0, -1, 0, 1, "Thin air, pasture, and isolated culture.", "空气稀薄，适合高原牧场。")

/*
Editable ecology and resource feature tables.
*/
#define WORLD_ECOLOGY_RULE_ROWS(X) \
    X(ECO_NONE, "None", "无", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "No special ecology.", "没有特殊生态。") \
    X(ECO_FOREST, "Forest", "森林", 0, 0, 4, 0, 0, 1, 1, 1, 0, 0, 1, "Wood, hunting, and cover.", "木材、狩猎和隐蔽。") \
    X(ECO_RAINFOREST, "Rainforest", "雨林", 1, -1, 5, 0, 1, 2, 1, 1, -1, 0, 2, "Dense wet forest, rich but difficult.", "高温潮湿，资源多但开发困难。") \
    X(ECO_GRASSLAND, "Grassland", "草原", 2, 3, -1, 0, 0, 0, 2, 2, 1, -1, 0, "Open pasture and mobile cultures.", "开阔牧场和迁徙文明。") \
    X(ECO_DESERT, "Desert Ecology", "沙漠生态", -2, -1, -2, 1, 1, -2, -2, 1, -2, 1, 0, "Sparse ecology and hard survival.", "生态稀疏，生存艰难。") \
    X(ECO_TUNDRA, "Tundra Ecology", "冻原生态", -1, 1, -2, 0, 1, 0, -2, -1, -1, 1, 0, "Low plants and cold adaptation.", "低矮植被和寒地适应。") \
    X(ECO_SWAMP, "Swamp", "沼泽", 0, -1, 2, 0, 0, 3, -1, 0, -2, 0, 2, "Wet defensive ground.", "潮湿泥泞，防御强。") \
    X(ECO_BAMBOO, "Bamboo", "竹林", 1, 0, 3, 0, 0, 1, 1, 1, 0, 0, 1, "Fast-growing material and cover.", "快速生长的材料和伏兵地形。") \
    X(ECO_MANGROVE, "Mangrove", "红树林", 1, 0, 2, 0, 0, 2, 1, 2, 0, 0, 2, "Tropical coastal wetland.", "热带海岸湿地。")

#define WORLD_RESOURCE_FEATURE_RULE_ROWS(X) \
    X(RESOURCE_FEATURE_NONE, "None", "无", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "No special resource site.", "没有特殊资源点。") \
    X(RESOURCE_FEATURE_FARMLAND, "Farmland", "农田", 3, 0, 0, 0, 0, 1, 3, 1, 1, 0, 0, "Stable food base.", "稳定粮食来源。") \
    X(RESOURCE_FEATURE_PASTURE, "Pasture", "牧场", 0, 3, 0, 0, 0, 0, 1, 2, 0, 0, 0, "Livestock, mounts, and trade.", "畜牧、坐骑和贸易。") \
    X(RESOURCE_FEATURE_FOREST, "Forest Resource", "森林资源区", 0, 0, 3, 0, 0, 1, 1, 1, 0, 0, 1, "Building material, herbs, and game.", "建筑、药草和野味。") \
    X(RESOURCE_FEATURE_MINE, "Mine", "矿山", 0, 0, 0, 2, 4, 0, 0, 3, 0, 0, 1, "Ore, stone, wealth, and conflict.", "矿石、石料、财富和争夺。") \
    X(RESOURCE_FEATURE_FISHERY, "Fishery", "渔场", 3, 0, 0, 0, 0, 1, 1, 2, 0, 0, 0, "Food and coastal economy.", "水产和沿海经济。") \
    X(RESOURCE_FEATURE_SALT_LAKE, "Salt Lake", "盐湖", 0, 0, 0, 1, 2, 1, 0, 3, 0, 0, 0, "Salt, preservation, and trade value.", "盐、食物保存和贸易价值。") \
    X(RESOURCE_FEATURE_GEOTHERMAL, "Geothermal", "温泉", 0, 0, 0, 1, 1, 1, 1, 2, 1, 0, 0, "Healing, tourism, and sacred sites.", "疗养、旅游和宗教圣地。")

/*
Civilization metric table.
Metric, Chinese, ability, high result, low result.
Adaptation is not listed here because it is a dynamic state derived from
environment, resources, culture, and disorder instead of a fixed core metric.
*/
#define CIVILIZATION_METRIC_ROWS(X) \
    X(CIV_METRIC_GOVERNANCE, "governance", "治理力", "Manages population, cities, taxes, and order.", "管理人口、城市、税收、秩序的能力。", "More cities, higher tax, stable expansion.", "城市更多、税收更高、扩张更稳定。", "Many cities cause disorder and weak control.", "城市一多就混乱，税收低，占领地难控制。") \
    X(CIV_METRIC_COHESION, "cohesion", "凝聚力", "Cultural identity, stability, and assimilation.", "文化认同、社会稳定、同化能力。", "Stable society and strong cultural influence.", "社会稳定，同化快，文化影响强。", "Rebellion risk, weak culture, unstable occupation.", "容易叛乱，文化弱，占领地不稳定。") \
    X(CIV_METRIC_PRODUCTION, "production", "生产力", "Building, crafting, gathering, and infrastructure.", "建造、制造、采集、基础建设能力。", "Fast construction and resource development.", "建设快，资源开发快，城防和设施强。", "Slow building and weak resource development.", "建设慢，资源开发差，城市成长慢。") \
    X(CIV_METRIC_MILITARY, "military", "军备力", "Army organization, weapons, and war readiness.", "组织军队、制造武器、发动战争的能力。", "Strong troops and high war pressure.", "兵种强，战争潜力高，敢争夺资源。", "Weak army and little active war pressure.", "军队弱，不适合主动战争。") \
    X(CIV_METRIC_COMMERCE, "commerce", "贸易力", "Money, exchange, markets, and trade routes.", "赚钱、交换资源、建立市场网络的能力。", "More money, stronger ports and markets.", "金钱多，贸易强，市场和港口价值高。", "Weak economy and poor resource conversion.", "经济弱，资源转换能力差。") \
    X(CIV_METRIC_LOGISTICS, "logistics", "后勤力", "Movement, supply, distance, and migration.", "移动、补给、远征、迁徙的能力。", "Expands farther and supplies armies better.", "行军远，扩张快，补给强，迁徙能力强。", "Low efficiency away from home.", "离家远效率低，远征困难。") \
    X(CIV_METRIC_INNOVATION, "innovation", "技术力", "Learning, tools, systems, and later growth.", "学习、改良工具、解锁新制度的能力。", "Faster unlocks and stronger late growth.", "解锁快，效率高，后期成长强。", "Slow technology and late-game weakness.", "技术慢，容易被后期文明超过。")

extern const GeographyRule GEOGRAPHY_RULES[GEO_COUNT];
extern const ClimateRule CLIMATE_RULES[CLIMATE_COUNT];
extern const EcologyRule ECOLOGY_RULES[ECO_COUNT];
extern const ResourceFeatureRule RESOURCE_FEATURE_RULES[RESOURCE_FEATURE_COUNT];
extern const CivilizationMetricRule CIVILIZATION_METRIC_RULES[CIV_METRIC_COUNT];

const char *localized_text(LocalizedText text, int language);

#endif
