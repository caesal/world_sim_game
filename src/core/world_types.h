#ifndef WORLD_SIM_WORLD_TYPES_H
#define WORLD_SIM_WORLD_TYPES_H

#include "constants.h"

typedef enum {
    GEO_OCEAN,
    GEO_COAST,
    GEO_PLAIN,
    GEO_HILL,
    GEO_MOUNTAIN,
    GEO_PLATEAU,
    GEO_BASIN,
    GEO_CANYON,
    GEO_VOLCANO,
    GEO_LAKE,
    GEO_BAY,
    GEO_DELTA,
    GEO_WETLAND,
    GEO_OASIS,
    GEO_ISLAND,
    GEO_COUNT
} Geography;

typedef enum {
    CLIMATE_TROPICAL_RAINFOREST,
    CLIMATE_TROPICAL_MONSOON,
    CLIMATE_TROPICAL_SAVANNA,
    CLIMATE_DESERT,
    CLIMATE_SEMI_ARID,
    CLIMATE_MEDITERRANEAN,
    CLIMATE_OCEANIC,
    CLIMATE_TEMPERATE_MONSOON,
    CLIMATE_CONTINENTAL,
    CLIMATE_SUBARCTIC,
    CLIMATE_TUNDRA,
    CLIMATE_ICE_CAP,
    CLIMATE_ALPINE,
    CLIMATE_HIGHLAND_PLATEAU,
    CLIMATE_COUNT
} Climate;

typedef enum {
    ECO_NONE,
    ECO_FOREST,
    ECO_RAINFOREST,
    ECO_GRASSLAND,
    ECO_DESERT,
    ECO_TUNDRA,
    ECO_SWAMP,
    ECO_BAMBOO,
    ECO_MANGROVE,
    ECO_COUNT
} Ecology;

typedef enum {
    RESOURCE_FEATURE_NONE,
    RESOURCE_FEATURE_FARMLAND,
    RESOURCE_FEATURE_PASTURE,
    RESOURCE_FEATURE_FOREST,
    RESOURCE_FEATURE_MINE,
    RESOURCE_FEATURE_FISHERY,
    RESOURCE_FEATURE_SALT_LAKE,
    RESOURCE_FEATURE_GEOTHERMAL,
    RESOURCE_FEATURE_COUNT
} ResourceFeature;

typedef enum {
    RESOURCE_FOOD,
    RESOURCE_LIVESTOCK,
    RESOURCE_WOOD,
    RESOURCE_STONE,
    RESOURCE_ORE,
    RESOURCE_WATER,
    RESOURCE_POPULATION,
    RESOURCE_MONEY,
    RESOURCE_TYPE_COUNT
} ResourceType;

typedef struct {
    int value[RESOURCE_TYPE_COUNT];
} ResourceVector;

typedef struct {
    Geography geography;
    Climate climate;
    Ecology ecology;
    ResourceFeature resource;
    int owner;
    int province_id;
    int elevation;
    int moisture;
    int temperature;
    int resource_variation;
    int river;
} Tile;

typedef struct {
    int x;
    int y;
} RiverPoint;

typedef struct {
    int active;
    int point_count;
    int width;
    int flow;
    int order;
    RiverPoint points[MAX_RIVER_POINTS];
} RiverPath;

typedef struct {
    int food;
    int livestock;
    int wood;
    int stone;
    int minerals;
    int water;
    int pop_capacity;
    int money;
    int habitability;
    int attack;
    int defense;
} TerrainStats;

typedef struct {
    int city_id;
    int tiles;
    int population;
    int food;
    int livestock;
    int wood;
    int stone;
    int minerals;
    int water;
    int pop_capacity;
    int money;
    int habitability;
    int attack;
    int defense;
} RegionSummary;

typedef struct {
    int population;
    int territory;
    int cities;
    int ports;
    int food;
    int livestock;
    int wood;
    int stone;
    int minerals;
    int water;
    int pop_capacity;
    int money;
    int habitability;
    int resource_score;
} CountrySummary;

#endif
