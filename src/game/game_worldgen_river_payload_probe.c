#include "game/game_worldgen_river_payload_probe.h"

#include "core/game_types.h"
#include "core/render_snapshot_keys.h"
#include "core/render_snapshot_river.h"
#include "io/map_save.h"
#include "io/map_save_load_river_preflight.h"
#include "io/map_save_river_paths.h"
#include "io/map_save_world_physical.h"
#include "world/river_path_validation.h"
#include "world/river_presentation_state.h"
#include "world/rivers.h"

#include <stdlib.h>
#include <string.h>

enum {
    RIVER_PAYLOAD_PROBE_COUNT = 12289,
    RIVER_PREFLIGHT_PROBE_W = 8,
    RIVER_PREFLIGHT_PROBE_H = 8,
    RIVER_PREFLIGHT_PROBE_COUNT = 3
};

typedef struct {
    char tag[8];
    uint32_t version;
    uint32_t item_size;
    uint32_t map_w;
    uint32_t map_h;
    uint32_t count;
    uint32_t checksum;
} ProbePhysicalHeader;

_Static_assert(sizeof(ProbePhysicalHeader) == 32,
               "PHY20 probe header contract changed");

static uint64_t hash_bytes(const void *data, size_t size) {
    const unsigned char *bytes = (const unsigned char *)data;
    uint64_t hash = UINT64_C(1469598103934665603);
    size_t i;
    for (i = 0; i < size; i++) {
        hash ^= bytes[i];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static uint32_t physical_checksum(const unsigned char *tile, int tile_count) {
    uint32_t checksum = UINT32_C(2166136261);
    int index;
    size_t byte;
    for (index = 0; index < tile_count; index++) {
        for (byte = 0; byte < 12; byte++) {
            checksum ^= tile[byte];
            checksum *= UINT32_C(16777619);
        }
    }
    return checksum;
}

static void fill_paths(RiverPath *paths, int count, int width, int height) {
    int i;
    for (i = 0; i < count; i++) {
        int x = i % (width - 1);
        int y = (i / (width - 1)) % height;
        paths[i].active = 1;
        paths[i].point_count = 2;
        paths[i].width = 1 + i % 5;
        paths[i].flow = 100 + i;
        paths[i].order = 1 + i % 8;
        paths[i].points[0].x = x;
        paths[i].points[0].y = y;
        paths[i].points[1].x = x + 1;
        paths[i].points[1].y = y;
    }
}

static int write_preflight_body(FILE *file, const RiverPath *paths,
                                int paths_to_write, int valid_physical) {
    static const char tag[8] = {'P', 'H', 'Y', '2', '0', '\0', '\0', '\0'};
    ProbePhysicalHeader header = {0};
    Tile tile = {0};
    unsigned char physical_tile[12] = {0};
    int tile_count = RIVER_PREFLIGHT_PROBE_W * RIVER_PREFLIGHT_PROBE_H;
    int i;
    memcpy(header.tag, tag, sizeof(tag));
    header.version = MAP_SAVE_WORLD_PHYSICAL_BLOCK_VERSION;
    header.item_size = sizeof(physical_tile);
    header.map_w = RIVER_PREFLIGHT_PROBE_W;
    header.map_h = RIVER_PREFLIGHT_PROBE_H;
    header.count = tile_count;
    header.checksum = valid_physical ?
        physical_checksum(physical_tile, tile_count) : 0;
    for (i = 0; i < tile_count; i++) {
        if (fwrite(&tile, sizeof(tile), 1, file) != 1) return 0;
    }
    if (fwrite(&header, sizeof(header), 1, file) != 1) return 0;
    for (i = 0; i < tile_count; i++) {
        if (fwrite(physical_tile, sizeof(physical_tile), 1, file) != 1) return 0;
    }
    return paths_to_write == 0 ||
           fwrite(paths, sizeof(*paths), (size_t)paths_to_write, file) ==
               (size_t)paths_to_write;
}

static int save_preflight_probe(RiverPath *live_paths, int live_count,
                                uint64_t live_hash, size_t live_bytes) {
    RiverPath valid_paths[RIVER_PREFLIGHT_PROBE_COUNT] = {0};
    RiverPath invalid_paths[RIVER_PREFLIGHT_PROBE_COUNT] = {0};
    MapSaveRiverPathsStage stage = {0};
    FILE *valid = tmpfile();
    FILE *truncated = tmpfile();
    FILE *invalid = tmpfile();
    FILE *invalid_physical = tmpfile();
    MapSaveRiverPathsStatus valid_status = MAP_SAVE_RIVER_PATHS_INVALID_ARGUMENT;
    MapSaveRiverPathsStatus truncated_status = MAP_SAVE_RIVER_PATHS_INVALID_ARGUMENT;
    MapSaveRiverPathsStatus invalid_status = MAP_SAVE_RIVER_PATHS_INVALID_ARGUMENT;
    MapSaveRiverPathsStatus physical_status = MAP_SAVE_RIVER_PATHS_INVALID_ARGUMENT;
    MapSaveRiverPathsStatus oversized_status = MAP_SAVE_RIVER_PATHS_INVALID_ARGUMENT;
    size_t limit_bytes = 0;
    int practical_limit = river_path_count_limit(MAX_MAP_W, MAX_MAP_H);
    int live_limit = river_path_count_limit(map_w, map_h);
    int bound_ok = practical_limit > 14265 &&
                   river_path_storage_byte_limit() == 128u * 1024u * 1024u &&
                   river_path_storage_bytes_checked(practical_limit, &limit_bytes) &&
                   limit_bytes <= river_path_storage_byte_limit() &&
                   !river_path_storage_bytes_checked(practical_limit + 1,
                                                      &limit_bytes);
    int valid_advance = 0;
    int valid_restored = 0;
    int live_unchanged;
    int ok;

    fill_paths(valid_paths, RIVER_PREFLIGHT_PROBE_COUNT,
               RIVER_PREFLIGHT_PROBE_W, RIVER_PREFLIGHT_PROBE_H);
    memcpy(invalid_paths, valid_paths, sizeof(valid_paths));
    invalid_paths[1].point_count = 1;
    if (valid && write_preflight_body(valid, valid_paths,
                                      RIVER_PREFLIGHT_PROBE_COUNT, 1)) {
        rewind(valid);
        valid_status = map_save_load_river_preflight(
            valid, RIVER_PREFLIGHT_PROBE_W, RIVER_PREFLIGHT_PROBE_H,
            RIVER_PREFLIGHT_PROBE_COUNT, &stage);
        valid_restored = ftell(valid) == 0;
        if (valid_status == MAP_SAVE_RIVER_PATHS_OK &&
            fseek(valid, stage.file_offset, SEEK_SET) == 0) {
            valid_advance = map_save_river_paths_stage_advance(valid, &stage) &&
                ftell(valid) == stage.file_offset + (long)stage.bytes;
        }
        map_save_river_paths_stage_release(&stage);
    }
    if (truncated && write_preflight_body(truncated, valid_paths,
                                          RIVER_PREFLIGHT_PROBE_COUNT - 1, 1)) {
        rewind(truncated);
        truncated_status = map_save_load_river_preflight(
            truncated, RIVER_PREFLIGHT_PROBE_W, RIVER_PREFLIGHT_PROBE_H,
            RIVER_PREFLIGHT_PROBE_COUNT, &stage);
    }
    if (invalid && write_preflight_body(invalid, invalid_paths,
                                        RIVER_PREFLIGHT_PROBE_COUNT, 1)) {
        rewind(invalid);
        invalid_status = map_save_load_river_preflight(
            invalid, RIVER_PREFLIGHT_PROBE_W, RIVER_PREFLIGHT_PROBE_H,
            RIVER_PREFLIGHT_PROBE_COUNT, &stage);
    }
    if (invalid_physical && write_preflight_body(
            invalid_physical, valid_paths, RIVER_PREFLIGHT_PROBE_COUNT, 0)) {
        rewind(invalid_physical);
        physical_status = map_save_load_river_preflight(
            invalid_physical, RIVER_PREFLIGHT_PROBE_W,
            RIVER_PREFLIGHT_PROBE_H, RIVER_PREFLIGHT_PROBE_COUNT, &stage);
    }
    if (valid && live_limit >= 0 && fseek(valid, 0, SEEK_SET) == 0) {
        oversized_status = map_save_load_river_preflight(
            valid, map_w, map_h, live_limit + 1, &stage);
    }
    live_unchanged = river_paths == live_paths && river_path_count == live_count &&
        hash_bytes(river_paths, live_bytes) == live_hash && !stage.paths;
    ok = bound_ok && valid_status == MAP_SAVE_RIVER_PATHS_OK && valid_restored &&
         valid_advance && truncated_status == MAP_SAVE_RIVER_PATHS_TRUNCATED &&
          invalid_status == MAP_SAVE_RIVER_PATHS_INVALID_PAYLOAD &&
          physical_status == MAP_SAVE_RIVER_PATHS_PREFIX_INVALID &&
          oversized_status == MAP_SAVE_RIVER_PATHS_TOO_LARGE && live_unchanged;
    if (valid) fclose(valid);
    if (truncated) fclose(truncated);
    if (invalid) fclose(invalid);
    if (invalid_physical) fclose(invalid_physical);
    map_save_river_paths_stage_release(&stage);
    return ok;
}

int game_worldgen_river_payload_probe_run(FILE *file) {
    SnapshotRiverField snapshot = {0};
    SnapshotRiverField clone = {0};
    SnapshotRiverField post_load = {0};
    RiverPath *owned = NULL;
    RiverPath *loaded = NULL;
    RiverPath *first_live = NULL;
    FILE *payload_file = NULL;
    uint64_t raw_hash = 0;
    uint64_t loaded_hash = 0;
    uint64_t snapshot_hash = 0;
    uint64_t clone_hash = 0;
    uint64_t clone_after_release_hash = 0;
    size_t raw_bytes = (size_t)RIVER_PAYLOAD_PROBE_COUNT * sizeof(RiverPath);
    size_t snapshot_bytes = (size_t)RIVER_PAYLOAD_PROBE_COUNT * sizeof(SnapshotRiverPath);
    long file_bytes = -1;
    int dimensions_ok = map_w > 1 && map_h > 0;
    int count_ok = dimensions_ok && river_path_count_valid(
        RIVER_PAYLOAD_PROBE_COUNT, map_w, map_h);
    int source_valid = 0;
    int ownership_ok = 0;
    int retained_ok = 0;
    int header_ok = 0;
    int snapshot_ok = 0;
    int clone_ok = 0;
    int write_ok = 0;
    int read_ok = 0;
    int read_equal = 0;
    int read_adopt_ok = 0;
    int snapshot_survives_adopt = 0;
    int clone_survives_release = 0;
    int post_load_ok = 0;
    int revision_monotonic = 0;
    int revision_keys_changed = 0;
    int payload_changed = 0;
    int committed_before_replace = 0;
    int committed_invalidated = 0;
    int preflight_ok = 0;
    uint32_t revision_before = river_presentation_state_revision();
    uint32_t revision_first = 0;
    uint32_t revision_second = 0;
    int key_first = 0;
    int key_second = 0;
    int ok;

    if (count_ok) owned = (RiverPath *)calloc(
        (size_t)RIVER_PAYLOAD_PROBE_COUNT, sizeof(*owned));
    if (owned) {
        fill_paths(owned, RIVER_PAYLOAD_PROBE_COUNT, map_w, map_h);
        source_valid = river_paths_validate(owned, RIVER_PAYLOAD_PROBE_COUNT,
                                            map_w, map_h);
    }
    if (source_valid) {
        ownership_ok = river_presentation_state_adopt(
            &owned, RIVER_PAYLOAD_PROBE_COUNT, map_w, map_h) && !owned &&
            river_path_count == RIVER_PAYLOAD_PROBE_COUNT;
    }
    if (ownership_ok) {
        RiverGenerationDiagnostics synthetic = {0};
        first_live = river_paths;
        revision_first = river_presentation_state_revision();
        key_first = render_snapshot_river_revision_key();
        synthetic.legacy_paths_required = RIVER_PAYLOAD_PROBE_COUNT;
        river_generation_note_commit(&synthetic);
        committed_before_replace =
            river_generation_committed_diagnostics(&synthetic);
        retained_ok = river_presentation_state_retained_bytes() == raw_bytes;
        raw_hash = hash_bytes(river_paths, raw_bytes);
        header_ok = map_save_probe_river_header_roundtrip(
            RIVER_PAYLOAD_PROBE_COUNT);
        preflight_ok = save_preflight_probe(first_live, river_path_count,
                                            raw_hash, raw_bytes);
        snapshot_ok = render_snapshot_river_copy(&snapshot, key_first, map_w, map_h) &&
                       snapshot.path_count == RIVER_PAYLOAD_PROBE_COUNT;
    }
    if (snapshot_ok) {
        snapshot_hash = hash_bytes(snapshot.paths, snapshot_bytes);
        clone_ok = render_snapshot_river_clone(&clone, &snapshot) &&
                   clone.paths != snapshot.paths &&
                   clone.path_count == snapshot.path_count;
        if (clone_ok) {
            clone_hash = hash_bytes(clone.paths, snapshot_bytes);
            clone_ok = clone_hash == snapshot_hash;
        }
    }
    if (ownership_ok) payload_file = tmpfile();
    if (payload_file) {
        write_ok = map_save_river_paths_write(payload_file, river_paths,
                                               river_path_count, map_w, map_h);
        if (write_ok) file_bytes = ftell(payload_file);
        rewind(payload_file);
        if (map_save_river_paths_allocate(&loaded, RIVER_PAYLOAD_PROBE_COUNT,
                                          map_w, map_h)) {
            read_ok = map_save_river_paths_read(payload_file, loaded,
                                                RIVER_PAYLOAD_PROBE_COUNT,
                                                map_w, map_h);
        }
    }
    if (read_ok) {
        loaded_hash = hash_bytes(loaded, raw_bytes);
        read_equal = loaded_hash == raw_hash &&
                     memcmp(loaded, river_paths, raw_bytes) == 0;
        loaded[0].flow++;
        payload_changed = river_paths_validate(
            loaded, RIVER_PAYLOAD_PROBE_COUNT, map_w, map_h) &&
            hash_bytes(loaded, raw_bytes) != raw_hash;
        read_adopt_ok = river_presentation_state_adopt(
            &loaded, RIVER_PAYLOAD_PROBE_COUNT, map_w, map_h) && !loaded &&
            river_paths != first_live && river_path_count == RIVER_PAYLOAD_PROBE_COUNT;
        revision_second = river_presentation_state_revision();
        key_second = render_snapshot_river_revision_key();
        revision_monotonic =
            revision_first == (revision_before == UINT32_MAX ? 1u :
                               revision_before + 1u) &&
            revision_second == (revision_first == UINT32_MAX ? 1u :
                                revision_first + 1u);
        revision_keys_changed = key_first != key_second;
        {
            RiverGenerationDiagnostics stale;
            committed_invalidated =
                !river_generation_committed_diagnostics(&stale);
        }
    }
    if (read_adopt_ok && snapshot_ok) {
        snapshot_survives_adopt =
            hash_bytes(snapshot.paths, snapshot_bytes) == snapshot_hash;
    }
    render_snapshot_river_release(&snapshot);
    if (clone_ok) {
        clone_after_release_hash = hash_bytes(clone.paths, snapshot_bytes);
        clone_survives_release = clone_after_release_hash == clone_hash;
    }
    if (read_adopt_ok) {
        post_load_ok = render_snapshot_river_copy(&post_load, key_second,
                                                  map_w, map_h) &&
                       post_load.path_count == RIVER_PAYLOAD_PROBE_COUNT &&
                       hash_bytes(post_load.paths, snapshot_bytes) != snapshot_hash;
    }
    ok = count_ok && source_valid && ownership_ok && retained_ok && header_ok &&
         snapshot_ok && clone_ok && write_ok && read_ok && read_equal &&
          read_adopt_ok && snapshot_survives_adopt && clone_survives_release &&
           post_load_ok && preflight_ok && payload_changed && revision_monotonic &&
           revision_keys_changed && committed_before_replace &&
          committed_invalidated && file_bytes == (long)raw_bytes &&
         raw_bytes == UINT64_C(9683732);
    fprintf(file,
            "case=river_payload_v20 count=%d bytes=%llu file_bytes=%ld valid=%d "
            "ownership=%d retained=%d header=%d snapshot=%d clone=%d write=%d "
            "read=%d equal=%d read_adopt=%d snapshot_survives=%d "
            "clone_survives=%d post_load=%d payload_changed=%d "
            "revision=%u/%u/%u keys=%d/%d changed=%d monotonic=%d "
            "preflight=%d committed_before=%d stale_invalidated=%d "
            "hash=%016llx ok=%d\n",
            RIVER_PAYLOAD_PROBE_COUNT, (unsigned long long)raw_bytes, file_bytes,
            source_valid, ownership_ok, retained_ok, header_ok, snapshot_ok,
            clone_ok, write_ok, read_ok, read_equal, read_adopt_ok,
            snapshot_survives_adopt, clone_survives_release, post_load_ok,
            payload_changed,
            revision_before, revision_first, revision_second,
            key_first, key_second, revision_keys_changed, revision_monotonic,
            preflight_ok, committed_before_replace,
            committed_invalidated,
            (unsigned long long)raw_hash, ok);
    if (payload_file) fclose(payload_file);
    free(owned);
    free(loaded);
    render_snapshot_river_release(&snapshot);
    render_snapshot_river_release(&clone);
    render_snapshot_river_release(&post_load);
    river_presentation_state_clear();
    return ok;
}
