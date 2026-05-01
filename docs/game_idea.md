# Game Idea

This is a small civilization sandbox simulation game.

The player creates a world map and places different civilizations on it. Each civilization has its own behavior style. They can expand, fight, survive, or disappear over time.

## Current Version Goal

The first version is a text based simulation.

It will include:

1. A small world map
2. Three civilizations
3. Population
4. Territory size
5. Simple random events
6. Simple battle logic
7. Daily simulation output
8. Player-created civilizations
9. Civilization behavior preferences
10. Tile inspection

## Civilizations

### Red Tribe

Aggressive and fast expanding.

### Blue Tribe

Defensive and stable.

### Green Tribe

Balanced and adaptive.

## Terrain

The map uses simple symbols:

1. `~` Ocean
2. `.` Coast
3. `,` Grassland
4. `T` Forest
5. `:` Desert
6. `^` Mountain

Civilizations are shown as capital letters on top of land tiles.

## Preferences

Each civilization has four 0-10 traits:

1. Aggression: how likely it is to attack another country
2. Expansion: how quickly it settles nearby land
3. Defense: how well it holds territory
4. Culture: how quickly population grows and how stable the country feels

## Core Loop

Each day:

1. Each civilization may gain population
2. Each civilization may expand territory
3. Civilizations may fight if they meet
4. The game prints the result
5. The simulation continues until one civilization wins
