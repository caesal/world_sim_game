#ifndef WORLD_SIM_SIM_TYPES_H
#define WORLD_SIM_SIM_TYPES_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "constants.h"
#include "world_types.h"

typedef enum {
    CIV_METRIC_GOVERNANCE,
    CIV_METRIC_COHESION,
    CIV_METRIC_PRODUCTION,
    CIV_METRIC_MILITARY,
    CIV_METRIC_COMMERCE,
    CIV_METRIC_LOGISTICS,
    CIV_METRIC_INNOVATION,
    CIV_METRIC_COUNT
} CivilizationMetric;

typedef struct {
    int active;
    int from_city;
    int to_city;
    int sea_region;
    int distance;
    int point_count;
    POINT points[MAX_MARITIME_ROUTE_POINTS];
} MaritimeRoute;

typedef enum {
    POP_AGE_0_4,
    POP_AGE_5_17,
    POP_AGE_18_24,
    POP_AGE_25_39,
    POP_AGE_40_54,
    POP_AGE_55_64,
    POP_AGE_65_74,
    POP_AGE_75_PLUS
} PopulationAgeBand;

typedef struct {
    int male;
    int female;
} PopulationCohort;

typedef struct {
    PopulationCohort cohorts[POP_COHORT_COUNT];
    int total;
    int male;
    int female;
    int children;
    int working;
    int fertile;
    int recruitable;
    int elder;
    int carrying_capacity;
    int pressure;
} PopulationSummary;

typedef struct {
    char name[NAME_LEN];
    char symbol;
    COLORREF color;
    int alive;
    int population;
    int territory;
    int aggression;
    int expansion;
    int defense;
    int culture;
    int governance;
    int cohesion;
    int production;
    int military;
    int commerce;
    int logistics;
    int innovation;
    int adaptation;
    int capital_city;
    int disorder;
    int disorder_resource;
    int disorder_plague;
    int disorder_migration;
    int disorder_stability;
} Civilization;

typedef struct {
    int alive;
    int owner;
    char name[NAME_LEN];
    int x;
    int y;
    int population;
    int radius;
    int capital;
    int port;
    int port_x;
    int port_y;
    int port_region;
    int population_ready;
    PopulationCohort population_cohorts[POP_COHORT_COUNT];
} City;

#endif
