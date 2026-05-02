# Game Idea

This is a small civilization sandbox simulation game.

The player creates a world map and places different civilizations on it. Each civilization has its own behavior style. They can expand, form borders, survive, or disappear over time.

## Current Version Goal

The current version is a graphical sandbox simulation.

It will include:

1. A visual world map
2. Multiple civilizations
3. Population
4. Territory size
5. Simple random events
6. Peaceful border formation when civilizations meet
7. Year-by-year simulation
8. Civilization behavior preferences
9. Tile inspection
10. Country borders and village icons
11. City control regions
12. Terrain resource and local personality effects
13. Height, moisture, and temperature based geography
14. Rivers and hills

## Civilizations

### Red Tribe

Aggressive and fast expanding.

### Blue Tribe

Defensive and stable.

### Green Tribe

Balanced and adaptive.

## Terrain

The map uses visual colors:

1. Blue ocean
2. Pale coast
3. Green grassland
4. Dark green forest
5. Yellow desert
6. Pale icefield
7. Brown mountain
8. Olive hill
9. Green wetland

Civilizations are shown as colored territory on top of land tiles.

## Resources and Local Personality

Each terrain has resource values:

1. Food
2. Wood
3. Minerals
4. Water
5. Habitability

Terrain also affects local behavior:

1. Food-rich land grows larger populations but has lower combat pressure
2. Forest and wetland land gives better defense
3. Desert and icefield land supports fewer people but increases combat toughness
4. Mountain land gives minerals and strong defense

## Cities

Each civilization starts with a capital city. A city controls a nearby fixed province region. After that province shape is established, population growth and ownership changes do not reshape its borders.

Clicking any tile inside a city region shows the administrative region summary, including population and average food, wood, minerals, water, habitability, attack, and defense values.

## Geography

The map uses continuous elevation, moisture, and temperature fields. Terrain is derived from those fields so nearby tiles are more likely to be similar. Coastlines follow the elevation field, mountains form from high elevation, deserts form from dry warm regions, icefields form in cold regions, and rivers flow downhill from high wet terrain toward the sea.

## Preferences

Each civilization has four 0-10 traits:

1. Aggression: reserved for later conflict and pressure systems
2. Expansion: how quickly it settles nearby land
3. Defense: how well it holds territory
4. Culture: how quickly population grows and how stable the country feels

## Core Loop

Each day:

1. Each month, each civilization may gain population
2. Cities grow while their established province borders stay fixed
3. Each civilization may expand territory
4. Civilizations stop at each other's borders when they meet
5. The simulation continues as a sandbox world
