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
    X(GEO_OCEAN, "Ocean", "æµ·æ´‹", 0, 0, 0, 0, 0, 10, 0, 4, 0, 0, 0, "Sailing, climate, and trade water.", "èˆªæµ·ã€æ°”å€™å’Œè´¸æ˜“æ°´åŸŸã€‚") \
    X(GEO_COAST, "Coast", "æµ·å²¸", 4, 1, 2, 1, 1, 8, 4, 7, 6, 0, 1, "Trade, ports, shipbuilding, and coastal settlement.", "é€‚åˆè´¸æ˜“ã€æ¸¯å£ã€é€ èˆ¹å’Œæ²¿æµ·èšè½ã€‚") \
    X(GEO_PLAIN, "Plain", "å¹³åŽŸ", 6, 5, 1, 1, 1, 4, 6, 3, 7, -1, 0, "Agriculture, cities, and population core.", "å†œä¸šã€åŸŽå¸‚å’Œäººå£æ ¸å¿ƒã€‚") \
    X(GEO_HILL, "Hill", "ä¸˜é™µ", 3, 4, 3, 5, 4, 3, 3, 3, 4, 1, 2, "Balanced resource land for towns and defense.", "èµ„æºè¾ƒå‡è¡¡ï¼Œé€‚åˆå°åŸŽé•‡å’Œé˜²å¾¡ã€‚") \
    X(GEO_MOUNTAIN, "Mountain", "å±±è„‰", 1, 1, 2, 7, 8, 3, 1, 3, 2, 2, 4, "Mining, stone, passes, and natural borders.", "çŸ¿ä¸šã€çŸ³æ–™ã€å…³éš˜å’Œå¤©ç„¶è¾¹ç•Œã€‚") \
    X(GEO_PLATEAU, "Plateau", "é«˜åŽŸ", 2, 5, 2, 5, 5, 3, 2, 2, 4, 1, 2, "Pasture and isolated highland kingdoms.", "ç•œç‰§å’Œå°é—­é«˜åœ°æ–‡æ˜Žã€‚") \
    X(GEO_BASIN, "Basin", "ç›†åœ°", 4, 3, 2, 3, 3, 5, 4, 3, 5, 0, 1, "Closed settlement bowl, often fertile if watered.", "å°é—­èšè½åŒºåŸŸï¼Œæœ‰æ°´æ—¶è¾ƒè‚¥æ²ƒã€‚") \
    X(GEO_CANYON, "Canyon", "å³¡è°·", 1, 1, 1, 6, 5, 3, 1, 2, 2, 2, 4, "Difficult terrain, ambushes, and hard borders.", "å¤©é™©åœ°å½¢ï¼Œé€‚åˆä¼å‡»å’Œé˜²å¾¡å…³å£ã€‚") \
    X(GEO_VOLCANO, "Volcano", "ç«å±±", 1, 1, 1, 7, 8, 2, 1, 4, 2, 2, 3, "Dangerous but rich in stone and minerals.", "å±é™©ä½†çŸ³æ–™å’ŒçŸ¿çŸ³ä¸°å¯Œã€‚") \
    X(GEO_LAKE, "Lake", "æ¹–æ³Š", 3, 0, 1, 0, 1, 10, 2, 3, 2, 0, 1, "Fresh water, fishery, and stable settlements.", "æ·¡æ°´ã€æ¸”ä¸šå’Œç¨³å®šèšè½ã€‚") \
    X(GEO_BAY, "Bay", "æµ·æ¹¾", 3, 0, 1, 1, 1, 9, 3, 7, 4, 0, 1, "Natural harbor and commercial city site.", "å¤©ç„¶æ¸¯å£ï¼Œé€‚åˆå•†ä¸šåŸŽå¸‚ã€‚") \
    X(GEO_DELTA, "Delta", "ä¸‰è§’æ´²", 8, 2, 2, 1, 1, 9, 8, 5, 9, -1, 1, "Granary land and dense population zone.", "ç²®ä»“åœ°åŒºï¼Œé€‚åˆé«˜äººå£å¤§åŸŽå¸‚ã€‚") \
    X(GEO_WETLAND, "Wetland", "æ¹¿åœ°", 3, 1, 3, 1, 1, 9, 2, 2, 4, 0, 3, "Water-rich defensive habitat.", "æ°´èµ„æºä¸°å¯Œï¼Œé˜²å¾¡å’Œç”Ÿæ€å¼ºã€‚") \
    X(GEO_OASIS, "Oasis", "ç»¿æ´²", 5, 1, 1, 1, 1, 8, 4, 6, 7, 0, 1, "Desert survival and strategic trade center.", "æ²™æ¼ ä¸­çš„ç”Ÿå­˜å’Œæˆ˜ç•¥è´¸æ˜“æ ¸å¿ƒã€‚") \
    X(GEO_ISLAND, "Island", "å²›å±¿", 3, 1, 2, 2, 2, 7, 3, 6, 5, 0, 2, "Single island landform with isolated culture and naval trade.", "å•ä¸ªå²›å±¿åœ°è²Œï¼Œé€‚åˆå­¤ç«‹æ–‡åŒ–å’Œæµ·è´¸å‘å±•ã€‚")

/*
Editable climate effect table.

Columns:
id, English, Chinese, food, livestock, wood, stone, ore, water, population,
money, habitability, attack, defense, note_en, note_zh
*/
#define WORLD_CLIMATE_RULE_ROWS(X) \
    X(CLIMATE_TROPICAL_RAINFOREST, "Tropical Rainforest", "çƒ­å¸¦é›¨æž—", 2, -1, 5, 0, 1, 3, 3, 2, 1, 0, 2, "Warm, wet, rich in wood and water.", "å…¨å¹´é«˜æ¸©å¤šé›¨ï¼Œæœ¨æå’Œæ°´ä¸°å¯Œã€‚") \
    X(CLIMATE_TROPICAL_MONSOON, "Tropical Monsoon", "çƒ­å¸¦å­£é£Ž", 4, 0, 2, 0, 1, 2, 4, 3, 2, 0, 1, "Wet season agriculture and flood plains.", "é›¨å­£å†œä¸šå’Œå­£èŠ‚æ€§æ´ªæ°´ã€‚") \
    X(CLIMATE_TROPICAL_SAVANNA, "Tropical Savanna", "çƒ­å¸¦è‰åŽŸ", 3, 3, -1, 0, 1, 0, 3, 3, 2, -1, 0, "Warm grassland, herds, and migration.", "æ¸©æš–è‰åŽŸï¼Œé€‚åˆå…½ç¾¤å’Œè¿å¾™ã€‚") \
    X(CLIMATE_DESERT, "Desert", "æ²™æ¼ ", -4, -2, -4, 2, 2, -4, -4, 2, -4, 2, 0, "Extremely dry, low population, trade routes.", "æžåº¦å¹²ç‡¥ï¼Œä½Žäººå£ï¼Œé€‚åˆå•†è·¯ã€‚") \
    X(CLIMATE_SEMI_ARID, "Semi-Arid", "åŠå¹²æ—±", -2, 3, -2, 1, 1, -2, -2, 2, -2, 1, 0, "Pasture edge between grassland and desert.", "è‰åŽŸå’Œæ²™æ¼ ä¹‹é—´çš„ç‰§åœºè¾¹ç¼˜ã€‚") \
    X(CLIMATE_MEDITERRANEAN, "Mediterranean", "åœ°ä¸­æµ·", 3, 1, 1, 1, 1, 0, 4, 5, 3, 0, 0, "Comfortable coastal city climate.", "é€‚åˆæ²¿æµ·åŸŽé‚¦å’Œè´¸æ˜“ã€‚") \
    X(CLIMATE_OCEANIC, "Oceanic", "æ¸©å¸¦æµ·æ´‹", 3, 2, 4, 0, 1, 3, 5, 5, 3, 0, 1, "Moist, mild, ports and forests.", "æ¸©å’Œæ¹¿æ¶¦ï¼Œé€‚åˆæ¸¯å£å’Œæ£®æž—ã€‚") \
    X(CLIMATE_TEMPERATE_MONSOON, "Temperate Monsoon", "æ¸©å¸¦å­£é£Ž", 4, 1, 2, 0, 1, 2, 5, 3, 3, 0, 0, "Dense farming and city growth.", "å†œè€•æ–‡æ˜Žå’Œé«˜äººå£å¯†åº¦ã€‚") \
    X(CLIMATE_CONTINENTAL, "Continental", "æ¸©å¸¦å¤§é™†", 2, 2, 1, 1, 1, 0, 4, 2, 1, 0, 0, "Inland farms, plains, and frontier shifts.", "å†…é™†å†œä¸šã€å¹³åŽŸå’Œè¾¹ç–†è¿‡æ¸¡ã€‚") \
    X(CLIMATE_SUBARCTIC, "Subarctic", "äºšå¯’å¸¦", -1, -1, 3, 1, 1, 0, -2, -1, -1, 0, 1, "Cold forests and low population.", "å¯’å†·é’ˆå¶æž—ï¼Œäººå£è¾ƒä½Žã€‚") \
    X(CLIMATE_TUNDRA, "Tundra", "è‹”åŽŸ", -3, 0, -3, 1, 1, -1, -4, -2, -2, 1, 1, "Frozen soil, herding, and hard living.", "å†»åœŸå¹¿å¸ƒï¼Œé€‚åˆå¯’åœ°éƒ¨è½ã€‚") \
    X(CLIMATE_ICE_CAP, "Ice Cap", "å†°åŽŸ", -4, -3, -4, 1, 1, 0, -5, -3, -4, 2, 1, "Permanent cold and nearly no farming.", "ç»ˆå¹´ä¸¥å¯’ï¼Œå‡ ä¹Žæ— æ³•å†œä¸šã€‚") \
    X(CLIMATE_ALPINE, "Alpine", "é«˜å±±", -2, -1, 0, 3, 3, 1, -3, 0, -3, 1, 3, "Altitude cold, mining, and defense.", "æµ·æ‹”é™æ¸©ï¼ŒçŸ¿äº§å’Œé˜²å¾¡å¼ºã€‚") \
    X(CLIMATE_HIGHLAND_PLATEAU, "Highland Plateau", "é«˜åŽŸ", -1, 3, 0, 2, 2, 0, -2, 0, -1, 0, 1, "Thin air, pasture, and isolated culture.", "ç©ºæ°”ç¨€è–„ï¼Œé€‚åˆé«˜åŽŸç‰§åœºã€‚")

/*
Editable ecology and resource feature tables.
*/
#define WORLD_ECOLOGY_RULE_ROWS(X) \
    X(ECO_NONE, "None", "æ— ", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "No special ecology.", "æ²¡æœ‰ç‰¹æ®Šç”Ÿæ€ã€‚") \
    X(ECO_FOREST, "Forest", "æ£®æž—", 0, 0, 4, 0, 0, 1, 1, 1, 0, 0, 1, "Wood, hunting, and cover.", "æœ¨æã€ç‹©çŒŽå’Œéšè”½ã€‚") \
    X(ECO_RAINFOREST, "Rainforest", "é›¨æž—", 1, -1, 5, 0, 1, 2, 1, 1, -1, 0, 2, "Dense wet forest, rich but difficult.", "é«˜æ¸©æ½®æ¹¿ï¼Œèµ„æºå¤šä½†å¼€å‘å›°éš¾ã€‚") \
    X(ECO_GRASSLAND, "Grassland", "è‰åŽŸ", 2, 3, -1, 0, 0, 0, 2, 2, 1, -1, 0, "Open pasture and mobile cultures.", "å¼€é˜”ç‰§åœºå’Œè¿å¾™æ–‡æ˜Žã€‚") \
    X(ECO_DESERT, "Desert Ecology", "æ²™æ¼ ç”Ÿæ€", -2, -1, -2, 1, 1, -2, -2, 1, -2, 1, 0, "Sparse ecology and hard survival.", "ç”Ÿæ€ç¨€ç–ï¼Œç”Ÿå­˜è‰°éš¾ã€‚") \
    X(ECO_TUNDRA, "Tundra Ecology", "å†»åŽŸç”Ÿæ€", -1, 1, -2, 0, 1, 0, -2, -1, -1, 1, 0, "Low plants and cold adaptation.", "ä½ŽçŸ®æ¤è¢«å’Œå¯’åœ°é€‚åº”ã€‚") \
    X(ECO_SWAMP, "Swamp", "æ²¼æ³½", 0, -1, 2, 0, 0, 3, -1, 0, -2, 0, 2, "Wet defensive ground.", "æ½®æ¹¿æ³¥æ³žï¼Œé˜²å¾¡å¼ºã€‚") \
    X(ECO_BAMBOO, "Bamboo", "ç«¹æž—", 1, 0, 3, 0, 0, 1, 1, 1, 0, 0, 1, "Fast-growing material and cover.", "å¿«é€Ÿç”Ÿé•¿çš„ææ–™å’Œä¼å…µåœ°å½¢ã€‚") \
    X(ECO_MANGROVE, "Mangrove", "çº¢æ ‘æž—", 1, 0, 2, 0, 0, 2, 1, 2, 0, 0, 2, "Tropical coastal wetland.", "çƒ­å¸¦æµ·å²¸æ¹¿åœ°ã€‚")

#define WORLD_RESOURCE_FEATURE_RULE_ROWS(X) \
    X(RESOURCE_FEATURE_NONE, "None", "æ— ", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "No special resource site.", "æ²¡æœ‰ç‰¹æ®Šèµ„æºç‚¹ã€‚") \
    X(RESOURCE_FEATURE_FARMLAND, "Farmland", "å†œç”°", 3, 0, 0, 0, 0, 1, 3, 1, 1, 0, 0, "Stable food base.", "ç¨³å®šç²®é£Ÿæ¥æºã€‚") \
    X(RESOURCE_FEATURE_PASTURE, "Pasture", "ç‰§åœº", 0, 3, 0, 0, 0, 0, 1, 2, 0, 0, 0, "Livestock, mounts, and trade.", "ç•œç‰§ã€åéª‘å’Œè´¸æ˜“ã€‚") \
    X(RESOURCE_FEATURE_FOREST, "Forest Resource", "æ£®æž—èµ„æºåŒº", 0, 0, 3, 0, 0, 1, 1, 1, 0, 0, 1, "Building material, herbs, and game.", "å»ºç­‘ã€è¯è‰å’Œé‡Žå‘³ã€‚") \
    X(RESOURCE_FEATURE_MINE, "Mine", "çŸ¿å±±", 0, 0, 0, 2, 4, 0, 0, 3, 0, 0, 1, "Ore, stone, wealth, and conflict.", "çŸ¿çŸ³ã€çŸ³æ–™ã€è´¢å¯Œå’Œäº‰å¤ºã€‚") \
    X(RESOURCE_FEATURE_FISHERY, "Fishery", "æ¸”åœº", 3, 0, 0, 0, 0, 1, 1, 2, 0, 0, 0, "Food and coastal economy.", "æ°´äº§å’Œæ²¿æµ·ç»æµŽã€‚") \
    X(RESOURCE_FEATURE_SALT_LAKE, "Salt Lake", "ç›æ¹–", 0, 0, 0, 1, 2, 1, 0, 3, 0, 0, 0, "Salt, preservation, and trade value.", "ç›ã€é£Ÿç‰©ä¿å­˜å’Œè´¸æ˜“ä»·å€¼ã€‚") \
    X(RESOURCE_FEATURE_GEOTHERMAL, "Geothermal", "æ¸©æ³‰", 0, 0, 0, 1, 1, 1, 1, 2, 1, 0, 0, "Healing, tourism, and sacred sites.", "ç–—å…»ã€æ—…æ¸¸å’Œå®—æ•™åœ£åœ°ã€‚")

/*
Civilization metric table.
Metric, Chinese, ability, high result, low result.
Adaptation is not listed here because it is a dynamic state derived from
environment, resources, culture, and disorder instead of a fixed core metric.
*/
#define CIVILIZATION_METRIC_ROWS(X) \
    X(CIV_METRIC_GOVERNANCE, "governance", "æ²»ç†åŠ›", "Manages population, cities, taxes, and order.", "ç®¡ç†äººå£ã€åŸŽå¸‚ã€ç¨Žæ”¶ã€ç§©åºçš„èƒ½åŠ›ã€‚", "More cities, higher tax, stable expansion.", "åŸŽå¸‚æ›´å¤šã€ç¨Žæ”¶æ›´é«˜ã€æ‰©å¼ æ›´ç¨³å®šã€‚", "Many cities cause disorder and weak control.", "åŸŽå¸‚ä¸€å¤šå°±æ··ä¹±ï¼Œç¨Žæ”¶ä½Žï¼Œå é¢†åœ°éš¾æŽ§åˆ¶ã€‚") \
    X(CIV_METRIC_COHESION, "cohesion", "å‡èšåŠ›", "Cultural identity, stability, and assimilation.", "æ–‡åŒ–è®¤åŒã€ç¤¾ä¼šç¨³å®šã€åŒåŒ–èƒ½åŠ›ã€‚", "Stable society and strong cultural influence.", "ç¤¾ä¼šç¨³å®šï¼ŒåŒåŒ–å¿«ï¼Œæ–‡åŒ–å½±å“å¼ºã€‚", "Rebellion risk, weak culture, unstable occupation.", "å®¹æ˜“å›ä¹±ï¼Œæ–‡åŒ–å¼±ï¼Œå é¢†åœ°ä¸ç¨³å®šã€‚") \
    X(CIV_METRIC_PRODUCTION, "production", "ç”Ÿäº§åŠ›", "Building, crafting, gathering, and infrastructure.", "å»ºé€ ã€åˆ¶é€ ã€é‡‡é›†ã€åŸºç¡€å»ºè®¾èƒ½åŠ›ã€‚", "Fast construction and resource development.", "å»ºè®¾å¿«ï¼Œèµ„æºå¼€å‘å¿«ï¼ŒåŸŽé˜²å’Œè®¾æ–½å¼ºã€‚", "Slow building and weak resource development.", "å»ºè®¾æ…¢ï¼Œèµ„æºå¼€å‘å·®ï¼ŒåŸŽå¸‚æˆé•¿æ…¢ã€‚") \
    X(CIV_METRIC_MILITARY, "military", "å†›å¤‡åŠ›", "Army organization, weapons, and war readiness.", "ç»„ç»‡å†›é˜Ÿã€åˆ¶é€ æ­¦å™¨ã€å‘åŠ¨æˆ˜äº‰çš„èƒ½åŠ›ã€‚", "Strong troops and high war pressure.", "å…µç§å¼ºï¼Œæˆ˜äº‰æ½œåŠ›é«˜ï¼Œæ•¢äº‰å¤ºèµ„æºã€‚", "Weak army and little active war pressure.", "å†›é˜Ÿå¼±ï¼Œä¸é€‚åˆä¸»åŠ¨æˆ˜äº‰ã€‚") \
    X(CIV_METRIC_COMMERCE, "commerce", "è´¸æ˜“åŠ›", "Money, exchange, markets, and trade routes.", "èµšé’±ã€äº¤æ¢èµ„æºã€å»ºç«‹å¸‚åœºç½‘ç»œçš„èƒ½åŠ›ã€‚", "More money, stronger ports and markets.", "é‡‘é’±å¤šï¼Œè´¸æ˜“å¼ºï¼Œå¸‚åœºå’Œæ¸¯å£ä»·å€¼é«˜ã€‚", "Weak economy and poor resource conversion.", "ç»æµŽå¼±ï¼Œèµ„æºè½¬æ¢èƒ½åŠ›å·®ã€‚") \
    X(CIV_METRIC_LOGISTICS, "logistics", "åŽå‹¤åŠ›", "Movement, supply, distance, and migration.", "ç§»åŠ¨ã€è¡¥ç»™ã€è¿œå¾ã€è¿å¾™çš„èƒ½åŠ›ã€‚", "Expands farther and supplies armies better.", "è¡Œå†›è¿œï¼Œæ‰©å¼ å¿«ï¼Œè¡¥ç»™å¼ºï¼Œè¿å¾™èƒ½åŠ›å¼ºã€‚", "Low efficiency away from home.", "ç¦»å®¶è¿œæ•ˆçŽ‡ä½Žï¼Œè¿œå¾å›°éš¾ã€‚") \
    X(CIV_METRIC_INNOVATION, "innovation", "æŠ€æœ¯åŠ›", "Learning, tools, systems, and later growth.", "å­¦ä¹ ã€æ”¹è‰¯å·¥å…·ã€è§£é”æ–°åˆ¶åº¦çš„èƒ½åŠ›ã€‚", "Faster unlocks and stronger late growth.", "è§£é”å¿«ï¼Œæ•ˆçŽ‡é«˜ï¼ŒåŽæœŸæˆé•¿å¼ºã€‚", "Slow technology and late-game weakness.", "æŠ€æœ¯æ…¢ï¼Œå®¹æ˜“è¢«åŽæœŸæ–‡æ˜Žè¶…è¿‡ã€‚")

extern const GeographyRule GEOGRAPHY_RULES[GEO_COUNT];
extern const ClimateRule CLIMATE_RULES[CLIMATE_COUNT];
extern const EcologyRule ECOLOGY_RULES[ECO_COUNT];
extern const ResourceFeatureRule RESOURCE_FEATURE_RULES[RESOURCE_FEATURE_COUNT];
extern const CivilizationMetricRule CIVILIZATION_METRIC_RULES[CIV_METRIC_COUNT];

const char *localized_text(LocalizedText text, int language);

#endif
