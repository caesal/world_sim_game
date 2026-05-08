#include "map_save.h"

#include "core/game_types.h"
#include "game/game.h"
#include "sim/regions.h"
#include "sim/simulation.h"

#include <commdlg.h>
#include <stdio.h>
#include <string.h>

#define MAP_SAVE_VERSION 2
#define MAP_SAVE_PATH_MAX 1024

typedef struct {
    char name[NAME_LEN];
    char symbol;
    Color32 color;
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
    int tech_stage;
    int tech_progress;
    int deep_sea_route_unlocked_event_done;
    int capital_city;
    int disorder;
    int disorder_resource;
    int disorder_plague;
    int disorder_migration;
    int disorder_stability;
} LegacyCivilizationV1;

typedef struct {
    char magic[8];
    int version;
    int map_w;
    int map_h;
    int map_size_index;
    int pending_map_size;
    int year;
    int month;
    int civ_count;
    int city_count;
    int river_path_count;
    int maritime_route_count;
    int region_count;
    int world_generated;
    int ocean_slider;
    int continent_slider;
    int relief_slider;
    int moisture_slider;
    int drought_slider;
    int vegetation_slider;
    int bias_forest_slider;
    int bias_desert_slider;
    int bias_mountain_slider;
    int bias_wetland_slider;
    int initial_civ_count;
    int region_size_slider;
    int plague_fog_alpha;
} MapSaveHeader;

static const char MAP_SAVE_MAGIC[8] = {'W', 'S', 'G', 'M', 'A', 'P', '1', '\0'};
static char save_folder[MAP_SAVE_PATH_MAX];

static int path_exists(const char *path) {
    DWORD attributes = GetFileAttributesA(path);
    return attributes != INVALID_FILE_ATTRIBUTES;
}

static int join_path(char *out, size_t out_size, const char *folder, const char *name) {
    size_t folder_len = strlen(folder);
    size_t name_len = strlen(name);
    int needs_slash = folder_len > 0 && folder[folder_len - 1] != '\\';
    size_t total = folder_len + (needs_slash ? 1u : 0u) + name_len;

    if (total + 1 > out_size) return 0;
    memcpy(out, folder, folder_len);
    if (needs_slash) out[folder_len++] = '\\';
    memcpy(out + folder_len, name, name_len);
    out[total] = '\0';
    return 1;
}

static int build_save_folder_path(char *path, size_t path_size) {
    char module_path[MAP_SAVE_PATH_MAX];
    char *last_slash;

    if (!GetModuleFileNameA(NULL, module_path, sizeof(module_path))) return 0;
    last_slash = strrchr(module_path, '\\');
    if (!last_slash) return 0;
    *last_slash = '\0';
    if (!join_path(path, path_size, module_path, "saves")) return 0;
    return join_path(path, path_size, path, "maps");
}

static int ensure_parent_folder(const char *folder) {
    char parent[MAP_SAVE_PATH_MAX];
    char *last_slash;

    snprintf(parent, sizeof(parent), "%s", folder);
    last_slash = strrchr(parent, '\\');
    if (!last_slash) return 0;
    *last_slash = '\0';
    if (!path_exists(parent) && !CreateDirectoryA(parent, NULL)) return 0;
    return 1;
}

static void migrate_legacy_nested_saves(void) {
    char nested_folder[MAP_SAVE_PATH_MAX];
    char pattern[MAP_SAVE_PATH_MAX];
    WIN32_FIND_DATAA find_data;
    HANDLE find;

    if (!join_path(nested_folder, sizeof(nested_folder), save_folder, "saves")) return;
    if (!join_path(nested_folder, sizeof(nested_folder), nested_folder, "maps")) return;
    if (!join_path(pattern, sizeof(pattern), nested_folder, "*.wsgmap")) return;
    find = FindFirstFileA(pattern, &find_data);
    if (find == INVALID_HANDLE_VALUE) return;
    do {
        char src[MAP_SAVE_PATH_MAX];
        char dst[MAP_SAVE_PATH_MAX];

        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        if (!join_path(src, sizeof(src), nested_folder, find_data.cFileName)) continue;
        if (!join_path(dst, sizeof(dst), save_folder, find_data.cFileName)) continue;
        if (!path_exists(dst)) CopyFileA(src, dst, TRUE);
    } while (FindNextFileA(find, &find_data));
    FindClose(find);
}

int ensure_map_save_folder(void) {
    if (!build_save_folder_path(save_folder, sizeof(save_folder))) return 0;
    if (!ensure_parent_folder(save_folder)) return 0;
    if (!path_exists(save_folder) && !CreateDirectoryA(save_folder, NULL)) return 0;
    migrate_legacy_nested_saves();
    return 1;
}

static void show_utf8_message(HWND hwnd, const char *text, const char *title, UINT flags) {
    WCHAR wide_text[1024];
    WCHAR wide_title[128];

    MultiByteToWideChar(CP_UTF8, 0, text, -1, wide_text, (int)(sizeof(wide_text) / sizeof(wide_text[0])));
    MultiByteToWideChar(CP_UTF8, 0, title, -1, wide_title, (int)(sizeof(wide_title) / sizeof(wide_title[0])));
    MessageBoxW(hwnd, wide_text, wide_title, flags);
}

static int write_block(FILE *file, const void *data, size_t size, size_t count) {
    return fwrite(data, size, count, file) == count;
}

static int read_block(FILE *file, void *data, size_t size, size_t count) {
    return fread(data, size, count, file) == count;
}

static void fill_header(MapSaveHeader *header) {
    memset(header, 0, sizeof(*header));
    memcpy(header->magic, MAP_SAVE_MAGIC, sizeof(header->magic));
    header->version = MAP_SAVE_VERSION;
    header->map_w = map_w;
    header->map_h = map_h;
    header->map_size_index = map_size_index;
    header->pending_map_size = pending_map_size;
    header->year = year;
    header->month = month;
    header->civ_count = civ_count;
    header->city_count = city_count;
    header->river_path_count = river_path_count;
    header->maritime_route_count = maritime_route_count;
    header->region_count = region_count;
    header->world_generated = world_generated;
    header->ocean_slider = ocean_slider;
    header->continent_slider = continent_slider;
    header->relief_slider = relief_slider;
    header->moisture_slider = moisture_slider;
    header->drought_slider = drought_slider;
    header->vegetation_slider = vegetation_slider;
    header->bias_forest_slider = bias_forest_slider;
    header->bias_desert_slider = bias_desert_slider;
    header->bias_mountain_slider = bias_mountain_slider;
    header->bias_wetland_slider = bias_wetland_slider;
    header->initial_civ_count = initial_civ_count;
    header->region_size_slider = region_size_slider;
    header->plague_fog_alpha = plague_fog_alpha;
}

static int validate_header(const MapSaveHeader *header) {
    return memcmp(header->magic, MAP_SAVE_MAGIC, sizeof(header->magic)) == 0 &&
           (header->version == 1 || header->version == MAP_SAVE_VERSION) &&
           header->map_w > 0 && header->map_w <= MAX_MAP_W &&
           header->map_h > 0 && header->map_h <= MAX_MAP_H &&
           header->civ_count >= 0 && header->civ_count <= MAX_CIVS &&
           header->city_count >= 0 && header->city_count <= MAX_CITIES &&
           header->river_path_count >= 0 && header->river_path_count <= MAX_RIVER_PATHS &&
           header->maritime_route_count >= 0 && header->maritime_route_count <= MAX_MARITIME_ROUTES &&
           header->region_count >= 0 && header->region_count <= MAX_NATURAL_REGIONS;
}

static void make_save_filename(char *path, size_t path_size) {
    SYSTEMTIME time_now;
    char file_name[80];

    GetLocalTime(&time_now);
    snprintf(file_name, sizeof(file_name), "world_map_%04d_%02d_%02d_%02d%02d%02d.wsgmap",
             time_now.wYear, time_now.wMonth, time_now.wDay,
             time_now.wHour, time_now.wMinute, time_now.wSecond);
    if (!join_path(path, path_size, save_folder, file_name)) path[0] = '\0';
}

static int write_world_rows(FILE *file) {
    int y;

    for (y = 0; y < map_h; y++) {
        if (!write_block(file, world[y], sizeof(Tile), (size_t)map_w)) return 0;
    }
    return 1;
}

static void clear_loaded_storage(void) {
    int x;
    int y;

    for (y = 0; y < MAX_MAP_H; y++) {
        for (x = 0; x < MAX_MAP_W; x++) {
            memset(&world[y][x], 0, sizeof(world[y][x]));
            world[y][x].geography = GEO_OCEAN;
            world[y][x].climate = CLIMATE_OCEANIC;
            world[y][x].owner = -1;
            world[y][x].province_id = -1;
            world[y][x].region_id = -1;
        }
    }
    memset(river_paths, 0, sizeof(river_paths));
    memset(maritime_routes, 0, sizeof(maritime_routes));
    memset(natural_regions, 0, sizeof(natural_regions));
    memset(civs, 0, sizeof(civs));
    memset(cities, 0, sizeof(cities));
}

static int read_world_rows(FILE *file) {
    int y;

    for (y = 0; y < map_h; y++) {
        if (!read_block(file, world[y], sizeof(Tile), (size_t)map_w)) return 0;
    }
    return 1;
}

static int read_civilizations(FILE *file, int save_version) {
    if (save_version >= 2) {
        return read_block(file, civs, sizeof(Civilization), (size_t)civ_count);
    }
    {
        LegacyCivilizationV1 legacy[MAX_CIVS];
        int i;

        if (!read_block(file, legacy, sizeof(LegacyCivilizationV1), (size_t)civ_count)) return 0;
        for (i = 0; i < civ_count; i++) {
            memset(&civs[i], 0, sizeof(civs[i]));
            memcpy(civs[i].name, legacy[i].name, NAME_LEN);
            civs[i].name[NAME_LEN - 1] = '\0';
            civs[i].name_id = -1;
            civs[i].custom_name = 1;
            civs[i].symbol = legacy[i].symbol;
            civs[i].color = legacy[i].color;
            civs[i].alive = legacy[i].alive;
            civs[i].population = legacy[i].population;
            civs[i].territory = legacy[i].territory;
            civs[i].aggression = legacy[i].aggression;
            civs[i].expansion = legacy[i].expansion;
            civs[i].defense = legacy[i].defense;
            civs[i].culture = legacy[i].culture;
            civs[i].governance = legacy[i].governance;
            civs[i].cohesion = legacy[i].cohesion;
            civs[i].production = legacy[i].production;
            civs[i].military = legacy[i].military;
            civs[i].commerce = legacy[i].commerce;
            civs[i].logistics = legacy[i].logistics;
            civs[i].innovation = legacy[i].innovation;
            civs[i].adaptation = legacy[i].adaptation;
            civs[i].tech_stage = legacy[i].tech_stage;
            civs[i].tech_progress = legacy[i].tech_progress;
            civs[i].deep_sea_route_unlocked_event_done = legacy[i].deep_sea_route_unlocked_event_done;
            civs[i].capital_city = legacy[i].capital_city;
            civs[i].disorder = legacy[i].disorder;
            civs[i].disorder_resource = legacy[i].disorder_resource;
            civs[i].disorder_plague = legacy[i].disorder_plague;
            civs[i].disorder_migration = legacy[i].disorder_migration;
            civs[i].disorder_stability = legacy[i].disorder_stability;
        }
        return 1;
    }
}

static void normalize_loaded_technology(void) {
    int i;

    for (i = 0; i < civ_count; i++) {
        civs[i].tech_stage = clamp(civs[i].tech_stage, 0, 10);
        if (civs[i].tech_progress < 0) civs[i].tech_progress = 0;
        if (civs[i].tech_stage >= 10) civs[i].tech_progress = 0;
    }
}

int save_current_map(HWND hwnd) {
    MapSaveHeader header;
    FILE *file;
    char path[MAP_SAVE_PATH_MAX];
    char message[MAP_SAVE_PATH_MAX + 96];

    if (!world_generated) {
        show_utf8_message(hwnd, "No generated map to save.", "Save Map", MB_OK | MB_ICONINFORMATION);
        return 0;
    }
    if (!ensure_map_save_folder()) {
        show_utf8_message(hwnd, "Could not create saves/maps.", "Save Map", MB_OK | MB_ICONERROR);
        return 0;
    }
    make_save_filename(path, sizeof(path));
    if (path[0] == '\0') {
        show_utf8_message(hwnd, "The map save path is too long.", "Save Map", MB_OK | MB_ICONERROR);
        return 0;
    }
    file = fopen(path, "wb");
    if (!file) {
        show_utf8_message(hwnd, "Could not open map save file.", "Save Map", MB_OK | MB_ICONERROR);
        return 0;
    }
    fill_header(&header);
    if (!write_block(file, &header, sizeof(header), 1) ||
        !write_world_rows(file) ||
        !write_block(file, river_paths, sizeof(RiverPath), (size_t)river_path_count) ||
        !write_block(file, maritime_routes, sizeof(MaritimeRoute), (size_t)maritime_route_count) ||
        !write_block(file, natural_regions, sizeof(NaturalRegion), (size_t)region_count) ||
        !write_block(file, civs, sizeof(Civilization), (size_t)civ_count) ||
        !write_block(file, cities, sizeof(City), (size_t)city_count)) {
        fclose(file);
        show_utf8_message(hwnd, "Could not write the full map save.", "Save Map", MB_OK | MB_ICONERROR);
        return 0;
    }
    fclose(file);
    snprintf(message, sizeof(message), "Saved map:\n%s", path);
    show_utf8_message(hwnd, message, "Save Map", MB_OK | MB_ICONINFORMATION);
    return 1;
}

static int any_save_files(void) {
    WIN32_FIND_DATAA find_data;
    char pattern[MAP_SAVE_PATH_MAX];
    HANDLE find;

    if (!join_path(pattern, sizeof(pattern), save_folder, "*.wsgmap")) return 0;
    find = FindFirstFileA(pattern, &find_data);

    if (find == INVALID_HANDLE_VALUE) return 0;
    FindClose(find);
    return 1;
}

static int pick_save_file(HWND hwnd, char *path, DWORD path_size) {
    OPENFILENAMEA open_file;

    memset(&open_file, 0, sizeof(open_file));
    path[0] = '\0';
    open_file.lStructSize = sizeof(open_file);
    open_file.hwndOwner = hwnd;
    open_file.lpstrFilter = "World Sim Map (*.wsgmap)\0*.wsgmap\0All Files\0*.*\0";
    open_file.lpstrInitialDir = save_folder;
    open_file.lpstrFile = path;
    open_file.nMaxFile = path_size;
    open_file.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    return GetOpenFileNameA(&open_file) ? 1 : 0;
}

int load_map_from_file(HWND hwnd) {
    MapSaveHeader header;
    FILE *file;
    char path[MAP_SAVE_PATH_MAX];

    if (!ensure_map_save_folder() || !any_save_files()) {
        show_utf8_message(hwnd, "No saved maps found.", "Load Map", MB_OK | MB_ICONINFORMATION);
        return 0;
    }
    if (!pick_save_file(hwnd, path, sizeof(path))) return 0;
    file = fopen(path, "rb");
    if (!file) {
        show_utf8_message(hwnd, "Could not open selected map save.", "Load Map", MB_OK | MB_ICONERROR);
        return 0;
    }
    if (!read_block(file, &header, sizeof(header), 1) || !validate_header(&header)) {
        fclose(file);
        show_utf8_message(hwnd, "The selected file is not a compatible map save.", "Load Map", MB_OK | MB_ICONERROR);
        return 0;
    }
    clear_loaded_storage();
    map_w = header.map_w;
    map_h = header.map_h;
    map_size_index = clamp(header.map_size_index, 0, MAP_SIZE_COUNT - 1);
    pending_map_size = clamp(header.pending_map_size, 0, MAP_SIZE_COUNT - 1);
    year = header.year;
    month = header.month;
    civ_count = header.civ_count;
    city_count = header.city_count;
    river_path_count = header.river_path_count;
    maritime_route_count = header.maritime_route_count;
    region_count = header.region_count;
    world_generated = header.world_generated;
    ocean_slider = header.ocean_slider;
    continent_slider = header.continent_slider;
    relief_slider = header.relief_slider;
    moisture_slider = header.moisture_slider;
    drought_slider = header.drought_slider;
    vegetation_slider = header.vegetation_slider;
    bias_forest_slider = header.bias_forest_slider;
    bias_desert_slider = header.bias_desert_slider;
    bias_mountain_slider = header.bias_mountain_slider;
    bias_wetland_slider = header.bias_wetland_slider;
    initial_civ_count = header.initial_civ_count;
    region_size_slider = header.region_size_slider;
    plague_fog_alpha = header.plague_fog_alpha;
    if (!read_world_rows(file) ||
        !read_block(file, river_paths, sizeof(RiverPath), (size_t)river_path_count) ||
        !read_block(file, maritime_routes, sizeof(MaritimeRoute), (size_t)maritime_route_count) ||
        !read_block(file, natural_regions, sizeof(NaturalRegion), (size_t)region_count) ||
        !read_civilizations(file, header.version) ||
        !read_block(file, cities, sizeof(City), (size_t)city_count)) {
        fclose(file);
        show_utf8_message(hwnd, "Could not read the full map save.", "Load Map", MB_OK | MB_ICONERROR);
        return 0;
    }
    fclose(file);
    normalize_loaded_technology();
    civilization_migrate_loaded_names();
    game_request_after_load_map();
    show_utf8_message(hwnd, "Loaded map save successfully.", "Load Map", MB_OK | MB_ICONINFORMATION);
    return 1;
}
