#include "map_save.h"
#include "core/game_types.h"
#include "core/load_progress.h"
#include "game/game.h"
#include "game/game_loop.h"
#include "io/map_save_legacy.h"
#include "io/map_save_civs.h"
#include "io/map_save_load_river_preflight.h"
#include "io/map_save_regions.h"
#include "io/map_save_river_paths.h"
#include "io/map_save_state.h"
#include "io/map_save_validation.h"
#include "io/map_save_war_history.h"
#include "io/map_save_world_physical.h"
#include "sim/civilization_uid.h"
#include "sim/disorder.h"
#include "sim/regions.h"
#include "sim/regions_port_policy.h"
#include "sim/regions_settlement.h"
#include "sim/simulation.h"
#include "sim/war.h"
#include "ui/ui_types.h"
#include "world/world_physical_state.h"
#include "world/river_presentation_state.h"
#include <commdlg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MAP_SAVE_VERSION 21
#define MAP_SAVE_PATH_MAX 1024

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

_Static_assert(sizeof(MapSaveHeader) == 112, "save-v21 header contract changed");
_Static_assert(offsetof(MapSaveHeader, river_path_count) == 44,
               "save-v21 river count offset changed");

static const char MAP_SAVE_MAGIC[8] = {'W', 'S', 'G', 'M', 'A', 'P', '1', '\0'};
static char save_folder[MAP_SAVE_PATH_MAX];
int map_save_current_version(void) { return MAP_SAVE_VERSION; }
int map_save_version_supported(int version) { return version == MAP_SAVE_VERSION; }
static const char *localized_text(const char *en, const char *zh) {
    return ui_language == UI_LANG_ZH ? zh : en;
}
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
    if (map_save_validation_silent()) return;
    MultiByteToWideChar(CP_UTF8, 0, text, -1, wide_text, (int)(sizeof(wide_text) / sizeof(wide_text[0])));
    MultiByteToWideChar(CP_UTF8, 0, title, -1, wide_title, (int)(sizeof(wide_title) / sizeof(wide_title[0])));
    MessageBoxW(hwnd, wide_text, wide_title, flags);
}
static void load_repaint(void *user_data) {
    HWND hwnd = (HWND)user_data; if (hwnd) { InvalidateRect(hwnd, NULL, FALSE); UpdateWindow(hwnd); }
}
static int write_block(FILE *file, const void *data, size_t size, size_t count) {
    if (count == 0) return 1;
    if (!data) return 0;
    return fwrite(data, size, count, file) == count;
}
static int read_block(FILE *file, void *data, size_t size, size_t count) {
    if (count == 0) return 1;
    if (!data) return 0;
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
int map_save_probe_fog_header_roundtrip(int value) {
    MapSaveHeader written;
    MapSaveHeader restored;
    FILE *file = tmpfile();
    int previous = plague_fog_alpha;
    int ok = 0;
    plague_fog_alpha = value;
    fill_header(&written);
    if (file && write_block(file, &written, sizeof(written), 1)) {
        rewind(file);
        if (read_block(file, &restored, sizeof(restored), 1)) {
            plague_fog_alpha = restored.plague_fog_alpha;
            ok = plague_fog_alpha == value;
        }
    }
    if (file) fclose(file);
    plague_fog_alpha = previous;
    return ok;
}
int map_save_probe_river_header_roundtrip(int expected_count) {
    MapSaveHeader written;
    MapSaveHeader restored;
    FILE *file = tmpfile();
    int ok = 0;
    fill_header(&written);
    if (file && written.river_path_count == expected_count &&
        write_block(file, &written, sizeof(written), 1)) {
        rewind(file);
        ok = read_block(file, &restored, sizeof(restored), 1) &&
             restored.version == MAP_SAVE_VERSION &&
             restored.river_path_count == expected_count;
    }
    if (file) fclose(file);
    return ok;
}
static int validate_header(const MapSaveHeader *header) {
    return memcmp(header->magic, MAP_SAVE_MAGIC, sizeof(header->magic)) == 0 &&
           map_save_version_supported(header->version) &&
           header->map_w > 0 && header->map_w <= MAX_MAP_W &&
           header->map_h > 0 && header->map_h <= MAX_MAP_H &&
           header->year >= 0 && header->month >= 1 && header->month <= 12 &&
           header->civ_count >= 0 && header->civ_count <= MAX_CIVS &&
           header->city_count >= 0 && header->city_count <= MAX_CITIES &&
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
static int pick_save_destination(HWND hwnd, char *path, DWORD path_size) {
    OPENFILENAMEA save_file;
    if (map_save_validation_path(path, path_size)) return 1;
    make_save_filename(path, path_size);
    if (path[0] == '\0') return 0;
    memset(&save_file, 0, sizeof(save_file));
    save_file.lStructSize = sizeof(save_file);
    save_file.hwndOwner = hwnd;
    save_file.lpstrFilter = "World Sim Map (*.wsgmap)\0*.wsgmap\0All Files\0*.*\0";
    save_file.lpstrInitialDir = save_folder;
    save_file.lpstrFile = path;
    save_file.nMaxFile = path_size;
    save_file.lpstrDefExt = "wsgmap";
    save_file.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
    return GetSaveFileNameA(&save_file) ? 1 : 0;
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
    river_presentation_state_clear();
    memset(maritime_routes, 0, sizeof(maritime_routes));
    memset(natural_regions, 0, sizeof(natural_regions));
    memset(civs, 0, sizeof(civs));
    memset(cities, 0, sizeof(cities)); disorder_reset_runtime();
    world_physical_state_reset();
}
static int read_world_rows(FILE *file) {
    int y;
    for (y = 0; y < map_h; y++) {
        if (!read_block(file, world[y], sizeof(Tile), (size_t)map_w)) return 0;
        if ((y & 7) == 0 || y + 1 == map_h) load_progress_update(LOAD_STAGE_WORLD_TILES, y + 1, map_h);
    }
    return 1;
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
    if (!pick_save_destination(hwnd, path, sizeof(path))) {
        if (CommDlgExtendedError() == 0) return 0;
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
        !map_save_war_history_write(file) ||
        !write_world_rows(file) ||
        !map_save_world_physical_write(file, map_w, map_h) ||
        !map_save_river_paths_write(file, river_paths, river_path_count, map_w, map_h) ||
        !write_block(file, maritime_routes, sizeof(MaritimeRoute), (size_t)maritime_route_count) ||
        !map_save_write_natural_regions(file) ||
        !write_block(file, civs, sizeof(Civilization), (size_t)civ_count) ||
        !write_block(file, cities, sizeof(City), (size_t)city_count) ||
        !map_save_write_dynamic_state(file)) {
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
    if (map_save_validation_path(path, path_size)) return 1;
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
    MapSaveRiverPathsStage river_stage = {0};
    MapSaveWarHistoryStage history_stage = {0};
    MapSaveRiverPathsStatus river_status;
    FILE *file;
    int storage_cleared = 0, worker_quiesced = 0;
    char path[MAP_SAVE_PATH_MAX];
#define LOAD_FAIL(message) do { map_save_river_paths_stage_release(&river_stage); map_save_war_history_stage_release(&history_stage); \
    fclose(file); if (worker_quiesced) { if (storage_cleared) game_request_recover_failed_map_load(); else game_loop_resume_after_aborted_hard_reset(); } \
    load_progress_fail(); load_progress_set_repaint_callback(NULL, NULL); \
    show_utf8_message(hwnd, message, localized_text("Load Map", "读取地图"), \
                      MB_OK | MB_ICONERROR); return 0; } while (0)

    if (!ensure_map_save_folder() || (!map_save_validation_path(path, sizeof(path)) && !any_save_files())) {
        show_utf8_message(hwnd, "No saved maps found.", "Load Map", MB_OK | MB_ICONINFORMATION);
        return 0;
    }
    if (!pick_save_file(hwnd, path, sizeof(path))) return 0;
    file = fopen(path, "rb");
    if (!file) {
        show_utf8_message(hwnd, "Could not open selected map save.", "Load Map", MB_OK | MB_ICONERROR);
        return 0;
    }
    load_progress_set_repaint_callback(load_repaint, hwnd);
    load_progress_begin();
    load_progress_update(LOAD_STAGE_OPEN_VALIDATE, 0, 1);
    if (!read_block(file, &header, sizeof(header), 1) || !validate_header(&header) ||
        !map_save_war_history_stage_read(file, header.version, header.year, header.month,
                                         header.civ_count, &history_stage)) {
        LOAD_FAIL(localized_text("The selected file is not a compatible map save.",
                                 "所选文件不是兼容的地图存档。"));
    }
    river_status = map_save_load_river_preflight(
        file, header.map_w, header.map_h, header.river_path_count,
        &river_stage);
    if (river_status != MAP_SAVE_RIVER_PATHS_OK) {
        LOAD_FAIL(map_save_load_river_error_text(
            river_status, ui_language == UI_LANG_ZH));
    }
    load_progress_update(LOAD_STAGE_OPEN_VALIDATE, 1, 1);
    if (!game_loop_quiesce_for_hard_reset()) LOAD_FAIL(localized_text("Could not pause simulation for map load.", "无法暂停模拟以读取地图。"));
    worker_quiesced = 1; load_progress_update(LOAD_STAGE_CLEAR_STORAGE, 0, 1);
    clear_loaded_storage(); storage_cleared = 1;
    load_progress_update(LOAD_STAGE_CLEAR_STORAGE, 1, 1);
    map_w = header.map_w;
    map_h = header.map_h;
    map_size_index = clamp(header.map_size_index, 0, MAP_SIZE_COUNT - 1);
    pending_map_size = clamp(header.pending_map_size, 0, MAP_SIZE_COUNT - 1);
    year = header.year;
    month = header.month;
    civ_count = header.civ_count;
    city_count = header.city_count;
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
    load_progress_update(LOAD_STAGE_WORLD_TILES, 0, max(1, map_h));
    if (!read_world_rows(file)) LOAD_FAIL("Could not read the full map save.");
    if (!map_save_world_physical_read(file, map_w, map_h)) {
        LOAD_FAIL(localized_text("The map's physical-world data is incompatible or corrupted.",
                                 "地图的物理世界数据不兼容或已损坏。"));
    }
    load_progress_update(LOAD_STAGE_RIVERS, 0, 1);
    if (!map_save_river_paths_stage_advance(file, &river_stage)) {
        LOAD_FAIL(localized_text("The map's river data is incompatible or corrupted.",
                                 "地图的河流数据不兼容或已损坏。"));
    }
    load_progress_update(LOAD_STAGE_RIVERS, 1, 1);
    load_progress_update(LOAD_STAGE_MARITIME, 0, 1);
    if (!read_block(file, maritime_routes, sizeof(MaritimeRoute), (size_t)maritime_route_count)) LOAD_FAIL("Could not read the full map save.");
    load_progress_update(LOAD_STAGE_MARITIME, 1, 1);
    load_progress_update(LOAD_STAGE_REGIONS, 0, 1);
    if (!map_save_read_natural_regions(file, header.version)) LOAD_FAIL("Could not read the full map save.");
    load_progress_update(LOAD_STAGE_REGIONS, 1, 1);
    load_progress_update(LOAD_STAGE_CIVS, 0, 1);
    if (!map_save_read_civilizations(file, header.version, civ_count)) LOAD_FAIL("Could not read the full map save.");
    load_progress_update(LOAD_STAGE_CIVS, 1, 1);
    load_progress_update(LOAD_STAGE_CITIES, 0, 1);
    if (!read_block(file, cities, sizeof(City), (size_t)city_count)) LOAD_FAIL("Could not read the full map save.");
    load_progress_update(LOAD_STAGE_CITIES, 1, 1);
    {
        int dynamic_result;
        load_progress_update(LOAD_STAGE_DYNAMIC_STATE, 0, 1);
        dynamic_result = map_save_read_dynamic_state_with_history(
            file, header.version, map_save_war_history_stage_state(&history_stage));
        if (dynamic_result < 0) {
            LOAD_FAIL("Could not read dynamic world state.");
        }
        load_progress_update(LOAD_STAGE_DYNAMIC_STATE, 1, 1);
        header.version = dynamic_result > 0 ? header.version : 7;
    }
    if (!river_presentation_state_adopt(&river_stage.paths,
                                        river_stage.count,
                                        map_w, map_h)) {
        LOAD_FAIL(localized_text("The map's river data is incompatible or corrupted.",
                                 "地图的河流数据不兼容或已损坏。"));
    }
    map_save_normalize_loaded_civilizations(header.version);
    civilization_migrate_loaded_names();
    civilization_repair_loaded_uids();
    if (!map_save_war_history_stage_commit(&history_stage)) LOAD_FAIL("Could not restore war history.");
    map_save_war_history_stage_release(&history_stage);
    fclose(file);
    regions_repair_local_city_slots(1);
    regions_port_policy_apply_all();
    regions_refresh_province_ids_from_regions();
    game_request_after_load_map(hwnd, header.version >= 8);
    load_progress_finish();
    load_progress_set_repaint_callback(NULL, NULL);
    show_utf8_message(hwnd, "Loaded map save successfully.", "Load Map", MB_OK | MB_ICONINFORMATION);
#undef LOAD_FAIL
    return 1;
}
