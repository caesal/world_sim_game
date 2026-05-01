#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAP_W 60
#define MAP_H 24
#define MAX_CIVS 8
#define NAME_LEN 32
#define MAX_DAYS 9999

typedef enum {
    WATER,
    COAST,
    GRASS,
    FOREST,
    DESERT,
    MOUNTAIN
} Terrain;

typedef struct {
    Terrain terrain;
    int owner;
} Tile;

typedef struct {
    char name[NAME_LEN];
    char symbol;
    int alive;
    int population;
    int territory;
    int aggression;
    int expansion;
    int defense;
    int culture;
} Civilization;

static Tile world[MAP_H][MAP_W];
static Civilization civs[MAX_CIVS];
static int civ_count = 0;
static int day = 0;

static int clamp(int value, int min, int max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

static int rnd(int max) {
    if (max <= 0) return 0;
    return rand() % max;
}

static void append_log(char *log, size_t log_size, const char *format, ...) {
    size_t used = strlen(log);
    va_list args;

    if (used >= log_size) return;

    va_start(args, format);
    vsnprintf(log + used, log_size - used, format, args);
    va_end(args);
}

static void trim_newline(char *text) {
    size_t len = strlen(text);
    if (len > 0 && text[len - 1] == '\n') {
        text[len - 1] = '\0';
    }
}

static void read_line(const char *prompt, char *buffer, size_t size) {
    printf("%s", prompt);
    if (fgets(buffer, (int)size, stdin) == NULL) {
        buffer[0] = '\0';
        return;
    }
    trim_newline(buffer);
}

static int read_int(const char *prompt, int min, int max, int fallback) {
    char line[64];
    char *end = NULL;
    long value;

    printf("%s", prompt);
    if (fgets(line, sizeof(line), stdin) == NULL) {
        return fallback;
    }

    value = strtol(line, &end, 10);
    if (end == line) {
        return fallback;
    }

    return clamp((int)value, min, max);
}

static int terrain_cost(Terrain terrain) {
    switch (terrain) {
        case GRASS: return 2;
        case FOREST: return 3;
        case DESERT: return 4;
        case MOUNTAIN: return 6;
        default: return 99;
    }
}

static char terrain_char(Terrain terrain) {
    switch (terrain) {
        case WATER: return '~';
        case COAST: return '.';
        case GRASS: return ',';
        case FOREST: return 'T';
        case DESERT: return ':';
        case MOUNTAIN: return '^';
        default: return '?';
    }
}

static const char *terrain_name(Terrain terrain) {
    switch (terrain) {
        case WATER: return "Ocean";
        case COAST: return "Coast";
        case GRASS: return "Grassland";
        case FOREST: return "Forest";
        case DESERT: return "Desert";
        case MOUNTAIN: return "Mountain";
        default: return "Unknown";
    }
}

static int is_land(Terrain terrain) {
    return terrain == GRASS || terrain == FOREST || terrain == DESERT || terrain == MOUNTAIN;
}

static int nearby_land_count(int x, int y) {
    int count = 0;
    int dy, dx;

    for (dy = -1; dy <= 1; dy++) {
        for (dx = -1; dx <= 1; dx++) {
            int nx = x + dx;
            int ny = y + dy;
            if (dx == 0 && dy == 0) continue;
            if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) continue;
            if (is_land(world[ny][nx].terrain)) count++;
        }
    }

    return count;
}

static void recalculate_territory(void) {
    int i, y, x;
    for (i = 0; i < civ_count; i++) {
        civs[i].territory = 0;
    }

    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int owner = world[y][x].owner;
            if (owner >= 0 && owner < civ_count) {
                civs[owner].territory++;
            }
        }
    }

    for (i = 0; i < civ_count; i++) {
        if (civs[i].territory <= 0 || civs[i].population <= 0) {
            civs[i].alive = 0;
        }
    }
}

static void generate_world(void) {
    int y, x, pass;
    int centers = 6 + rnd(4);
    int cx[10];
    int cy[10];
    int radius[10];

    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            world[y][x].terrain = WATER;
            world[y][x].owner = -1;
        }
    }

    for (x = 0; x < centers; x++) {
        cx[x] = 6 + rnd(MAP_W - 12);
        cy[x] = 4 + rnd(MAP_H - 8);
        radius[x] = 6 + rnd(10);
    }

    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int score = -8 + rnd(7);
            int i;
            for (i = 0; i < centers; i++) {
                int dx = x - cx[i];
                int dy = (y - cy[i]) * 2;
                int dist = abs(dx) + abs(dy);
                score += radius[i] - dist / 2;
            }

            if (score > 7) {
                int roll = rnd(100);
                if (roll < 12) world[y][x].terrain = MOUNTAIN;
                else if (roll < 28) world[y][x].terrain = FOREST;
                else if (roll < 42) world[y][x].terrain = DESERT;
                else world[y][x].terrain = GRASS;
            }
        }
    }

    for (pass = 0; pass < 2; pass++) {
        Tile next[MAP_H][MAP_W];
        memcpy(next, world, sizeof(world));
        for (y = 1; y < MAP_H - 1; y++) {
            for (x = 1; x < MAP_W - 1; x++) {
                int land = nearby_land_count(x, y);
                if (is_land(world[y][x].terrain) && land <= 2) next[y][x].terrain = WATER;
                if (world[y][x].terrain == WATER && land >= 6) next[y][x].terrain = GRASS;
            }
        }
        memcpy(world, next, sizeof(world));
    }

    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            if (world[y][x].terrain == WATER && nearby_land_count(x, y) > 0) {
                world[y][x].terrain = COAST;
            }
        }
    }

    day = 0;
    civ_count = 0;
}

static int find_empty_land(int *out_x, int *out_y) {
    int tries;
    for (tries = 0; tries < 3000; tries++) {
        int x = rnd(MAP_W);
        int y = rnd(MAP_H);
        if (is_land(world[y][x].terrain) && world[y][x].owner == -1) {
            *out_x = x;
            *out_y = y;
            return 1;
        }
    }
    return 0;
}

static int add_civilization(const char *name, char symbol, int aggression, int expansion, int defense, int culture) {
    int x, y;
    Civilization *civ;

    if (civ_count >= MAX_CIVS) {
        printf("Civilization limit reached.\n");
        return 0;
    }
    if (!find_empty_land(&x, &y)) {
        printf("No empty land found for a new civilization.\n");
        return 0;
    }

    civ = &civs[civ_count];
    memset(civ, 0, sizeof(*civ));
    strncpy(civ->name, name, NAME_LEN - 1);
    civ->symbol = symbol;
    civ->alive = 1;
    civ->population = 80 + rnd(60);
    civ->territory = 1;
    civ->aggression = clamp(aggression, 0, 10);
    civ->expansion = clamp(expansion, 0, 10);
    civ->defense = clamp(defense, 0, 10);
    civ->culture = clamp(culture, 0, 10);

    world[y][x].owner = civ_count;
    civ_count++;
    return 1;
}

static void seed_default_civilizations(void) {
    add_civilization("Red Dominion", 'R', 8, 7, 4, 3);
    add_civilization("Blue League", 'B', 3, 4, 8, 6);
    add_civilization("Green Pact", 'G', 5, 6, 5, 7);
}

static void print_map(void) {
    int y, x;
    printf("\nDay %d World Map\n", day);
    printf("Legend: ~=Ocean .=Coast ,=Grass T=Forest :=Desert ^=Mountain | Letters=Countries\n\n");

    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int owner = world[y][x].owner;
            if (owner >= 0 && owner < civ_count && civs[owner].alive) {
                putchar(civs[owner].symbol);
            } else {
                putchar(terrain_char(world[y][x].terrain));
            }
        }
        putchar('\n');
    }
}

static void print_civilizations(void) {
    int i;
    printf("\nCivilizations\n");
    printf("ID  Sym  Name                         Pop   Land  A E D C  State\n");
    for (i = 0; i < civ_count; i++) {
        printf("%-3d %-4c %-28s %-5d %-5d %d %d %d %d  %s\n",
               i + 1,
               civs[i].symbol,
               civs[i].name,
               civs[i].population,
               civs[i].territory,
               civs[i].aggression,
               civs[i].expansion,
               civs[i].defense,
               civs[i].culture,
               civs[i].alive ? "Alive" : "Fallen");
    }
}

static void inspect_tile(void) {
    int x = read_int("x (0-59): ", 0, MAP_W - 1, 0);
    int y = read_int("y (0-23): ", 0, MAP_H - 1, 0);
    int owner = world[y][x].owner;

    printf("Tile (%d,%d): %s", x, y, terrain_name(world[y][x].terrain));
    if (owner >= 0 && owner < civ_count) {
        printf(", controlled by %s", civs[owner].name);
    }
    printf(".\n");
}

static void create_custom_civilization(void) {
    char name[NAME_LEN];
    char symbol_line[16];
    char symbol;
    int aggression, expansion, defense, culture;

    read_line("Civilization name: ", name, sizeof(name));
    if (name[0] == '\0') {
        strcpy(name, "New Realm");
    }

    read_line("Map symbol (one letter): ", symbol_line, sizeof(symbol_line));
    symbol = (char)toupper((unsigned char)symbol_line[0]);
    if (!isalpha((unsigned char)symbol)) {
        symbol = (char)('A' + civ_count);
    }

    aggression = read_int("Aggression 0-10: ", 0, 10, 5);
    expansion = read_int("Expansion 0-10: ", 0, 10, 5);
    defense = read_int("Defense 0-10: ", 0, 10, 5);
    culture = read_int("Culture 0-10: ", 0, 10, 5);

    if (add_civilization(name, symbol, aggression, expansion, defense, culture)) {
        printf("%s has entered the world.\n", name);
    }
}

static void edit_civilization(void) {
    int id;
    Civilization *civ;

    if (civ_count == 0) {
        printf("No civilizations exist yet.\n");
        return;
    }

    print_civilizations();
    id = read_int("Choose civilization ID: ", 1, civ_count, 1) - 1;
    civ = &civs[id];

    printf("Editing %s. Press Enter or type invalid text to keep a default-ish value.\n", civ->name);
    civ->aggression = read_int("Aggression 0-10: ", 0, 10, civ->aggression);
    civ->expansion = read_int("Expansion 0-10: ", 0, 10, civ->expansion);
    civ->defense = read_int("Defense 0-10: ", 0, 10, civ->defense);
    civ->culture = read_int("Culture 0-10: ", 0, 10, civ->culture);
}

static int pick_owned_border(int civ_id, int *from_x, int *from_y, int *to_x, int *to_y) {
    int tries;
    const int dir[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};

    for (tries = 0; tries < 2000; tries++) {
        int x = rnd(MAP_W);
        int y = rnd(MAP_H);
        int d = rnd(4);
        int nx = x + dir[d][0];
        int ny = y + dir[d][1];

        if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) continue;
        if (world[y][x].owner != civ_id) continue;
        if (!is_land(world[ny][nx].terrain)) continue;
        if (world[ny][nx].owner == civ_id) continue;

        *from_x = x;
        *from_y = y;
        *to_x = nx;
        *to_y = ny;
        return 1;
    }

    return 0;
}

static void resolve_expansion(int civ_id, char *log, size_t log_size) {
    int fx, fy, tx, ty;
    int target_owner;
    Civilization *civ = &civs[civ_id];
    int drive;
    int cost;

    if (!civ->alive) return;
    if (!pick_owned_border(civ_id, &fx, &fy, &tx, &ty)) return;

    target_owner = world[ty][tx].owner;
    cost = terrain_cost(world[ty][tx].terrain);
    drive = civ->expansion * 8 + civ->culture * 2 + rnd(45) + civ->population / 30 - cost * 5;

    if (target_owner == -1) {
        if (drive > 25) {
            world[ty][tx].owner = civ_id;
            append_log(log, log_size,
                       "  %s settled new %s land.\n", civ->name, terrain_name(world[ty][tx].terrain));
        }
        return;
    }

    if (target_owner >= 0 && target_owner < civ_count && civs[target_owner].alive) {
        Civilization *enemy = &civs[target_owner];
        int attack = civ->aggression * 10 + civ->population / 12 + rnd(60);
        int defense = enemy->defense * 11 + enemy->population / 14 + terrain_cost(world[ty][tx].terrain) * 5 + rnd(60);

        if (attack > defense) {
            int loss = 6 + rnd(18);
            world[ty][tx].owner = civ_id;
            civ->population = clamp(civ->population - loss, 1, 99999);
            enemy->population = clamp(enemy->population - loss * 2, 0, 99999);
            append_log(log, log_size,
                       "  %s captured land from %s.\n", civ->name, enemy->name);
        } else if (rnd(100) < 35 + civ->aggression * 3) {
            int loss = 4 + rnd(14);
            civ->population = clamp(civ->population - loss, 0, 99999);
            enemy->population = clamp(enemy->population - loss / 2, 0, 99999);
            append_log(log, log_size,
                       "  %s failed to break %s's border.\n", civ->name, enemy->name);
        }
    }
}

static void random_event(char *log, size_t log_size) {
    int id;
    Civilization *civ;

    if (civ_count == 0 || rnd(100) > 24) return;
    id = rnd(civ_count);
    civ = &civs[id];
    if (!civ->alive) return;

    switch (rnd(5)) {
        case 0:
            civ->population += 20 + civ->culture * 3;
            append_log(log, log_size, "  Festival: %s gained population.\n", civ->name);
            break;
        case 1:
            civ->population = clamp(civ->population - (10 + rnd(30)), 0, 99999);
            append_log(log, log_size, "  Plague: %s lost people.\n", civ->name);
            break;
        case 2:
            civ->defense = clamp(civ->defense + 1, 0, 10);
            append_log(log, log_size, "  Fortification: %s improved defense.\n", civ->name);
            break;
        case 3:
            civ->aggression = clamp(civ->aggression + 1, 0, 10);
            append_log(log, log_size, "  Ambition: %s became more aggressive.\n", civ->name);
            break;
        default:
            civ->expansion = clamp(civ->expansion + 1, 0, 10);
            append_log(log, log_size, "  Migration: %s became more expansionist.\n", civ->name);
            break;
    }
}

static int living_civilizations(void) {
    int i;
    int living = 0;
    for (i = 0; i < civ_count; i++) {
        if (civs[i].alive) living++;
    }
    return living;
}

static void simulate_one_day(int verbose) {
    int i;
    char log[2048];
    log[0] = '\0';
    day = clamp(day + 1, 0, MAX_DAYS);

    for (i = 0; i < civ_count; i++) {
        Civilization *civ = &civs[i];
        int growth;
        if (!civ->alive) continue;

        growth = 2 + civ->culture + civ->territory / 3 + rnd(8);
        if (civ->population > civ->territory * 220) {
            growth -= 8;
        }
        civ->population = clamp(civ->population + growth, 0, 99999);
    }

    for (i = 0; i < civ_count; i++) {
        int attempts;
        if (!civs[i].alive) continue;
        attempts = 1 + civs[i].expansion / 4 + civs[i].aggression / 6;
        while (attempts-- > 0) {
            resolve_expansion(i, log, sizeof(log));
        }
    }

    random_event(log, sizeof(log));
    recalculate_territory();

    if (verbose) {
        printf("\nDay %d\n", day);
        if (log[0] == '\0') {
            printf("  The world was quiet today.\n");
        } else {
            printf("%s", log);
        }
        print_civilizations();
    }
}

static void simulate_days(void) {
    int days = read_int("How many days to simulate? ", 1, 365, 30);
    int i;
    for (i = 0; i < days; i++) {
        simulate_one_day(i == days - 1);
        if (living_civilizations() <= 1) break;
    }
    print_map();
}

static void print_menu(void) {
    printf("\n=== World Sim Game Ver0.1.0 ===\n");
    printf("1. Show map\n");
    printf("2. Show civilizations\n");
    printf("3. Create civilization\n");
    printf("4. Edit civilization preferences\n");
    printf("5. Simulate days\n");
    printf("6. Inspect tile\n");
    printf("7. Generate new world\n");
    printf("0. Quit\n");
}

int main(void) {
    int running = 1;
    srand((unsigned int)time(NULL));

    generate_world();
    seed_default_civilizations();

    printf("World Sim Game started!\n");
    printf("Civilization sandbox prototype Ver0.1.0\n");
    print_map();

    while (running) {
        int choice;
        print_menu();
        choice = read_int("Choose: ", 0, 7, 1);

        switch (choice) {
            case 1:
                print_map();
                break;
            case 2:
                print_civilizations();
                break;
            case 3:
                create_custom_civilization();
                break;
            case 4:
                edit_civilization();
                break;
            case 5:
                simulate_days();
                break;
            case 6:
                inspect_tile();
                break;
            case 7:
                generate_world();
                seed_default_civilizations();
                print_map();
                break;
            case 0:
                running = 0;
                break;
            default:
                printf("Unknown choice.\n");
                break;
        }
    }

    printf("Simulation ended on day %d.\n", day);
    return 0;
}
