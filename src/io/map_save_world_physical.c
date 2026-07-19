#include "io/map_save_world_physical.h"

#include "world/world_physical_state.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define PHYSICAL_SAVE_CHUNK 512

typedef struct {
    char tag[8];
    uint32_t version;
    uint32_t item_size;
    uint32_t map_w;
    uint32_t map_h;
    uint32_t count;
    uint32_t checksum;
} PhysicalSaveHeader;

typedef struct {
    uint32_t river_flow;
    uint16_t river_width;
    uint8_t wind_direction16;
    uint8_t wind_speed;
    uint8_t soil_fertility;
    uint8_t river_order;
    uint16_t river_flags;
} PhysicalSaveTile;

_Static_assert(sizeof(PhysicalSaveHeader) == 32, "PHY20 header contract changed");
_Static_assert(sizeof(PhysicalSaveTile) == 12, "PHY20 tile contract changed");

static const char PHYSICAL_SAVE_TAG[8] = {'P', 'H', 'Y', '2', '0', '\0', '\0', '\0'};

static uint32_t checksum_bytes(uint32_t checksum, const void *data, size_t size) {
    const unsigned char *bytes = (const unsigned char *)data;
    size_t i;
    for (i = 0; i < size; i++) {
        checksum ^= bytes[i];
        checksum *= 16777619u;
    }
    return checksum;
}

static PhysicalSaveTile disk_tile(const WorldPhysicalTileState *source) {
    PhysicalSaveTile result;
    result.river_flow = source->river_flow;
    result.river_width = source->river_width;
    result.wind_direction16 = source->wind_direction16;
    result.wind_speed = source->wind_speed;
    result.soil_fertility = source->soil_fertility;
    result.river_order = source->river_order;
    result.river_flags = source->river_flags;
    return result;
}

static WorldPhysicalTileState state_tile(const PhysicalSaveTile *source) {
    WorldPhysicalTileState result;
    result.river_flow = source->river_flow;
    result.river_width = source->river_width;
    result.wind_direction16 = source->wind_direction16;
    result.wind_speed = source->wind_speed;
    result.soil_fertility = source->soil_fertility;
    result.river_order = source->river_order;
    result.river_flags = source->river_flags;
    return result;
}

static uint32_t payload_checksum(const WorldPhysicalTileState *tiles, int count) {
    uint32_t checksum = 2166136261u;
    int i;
    for (i = 0; i < count; i++) {
        PhysicalSaveTile tile = disk_tile(&tiles[i]);
        checksum = checksum_bytes(checksum, &tile, sizeof(tile));
    }
    return checksum;
}

int map_save_world_physical_write(FILE *file, int map_w, int map_h) {
    PhysicalSaveHeader header;
    PhysicalSaveTile chunk[PHYSICAL_SAVE_CHUNK];
    const WorldPhysicalTileState *tiles = world_physical_state_tiles();
    int count = world_physical_state_tile_count();
    int offset;
    if (!file || !tiles || map_w != world_physical_state_width() ||
        map_h != world_physical_state_height() ||
        !world_physical_state_validate(map_w, map_h, tiles, count)) return 0;
    memset(&header, 0, sizeof(header));
    memcpy(header.tag, PHYSICAL_SAVE_TAG, sizeof(header.tag));
    header.version = MAP_SAVE_WORLD_PHYSICAL_BLOCK_VERSION;
    header.item_size = sizeof(PhysicalSaveTile);
    header.map_w = (uint32_t)map_w;
    header.map_h = (uint32_t)map_h;
    header.count = (uint32_t)count;
    header.checksum = payload_checksum(tiles, count);
    if (fwrite(&header, sizeof(header), 1, file) != 1) return 0;
    for (offset = 0; offset < count; offset += PHYSICAL_SAVE_CHUNK) {
        int i;
        int remaining = count - offset;
        int batch = remaining < PHYSICAL_SAVE_CHUNK ? remaining : PHYSICAL_SAVE_CHUNK;
        for (i = 0; i < batch; i++) chunk[i] = disk_tile(&tiles[offset + i]);
        if (fwrite(chunk, sizeof(chunk[0]), (size_t)batch, file) != (size_t)batch) return 0;
    }
    return 1;
}

static int header_valid(const PhysicalSaveHeader *header, int map_w, int map_h) {
    uint64_t expected_count;
    if (!header || map_w <= 0 || map_w > MAX_MAP_W ||
        map_h <= 0 || map_h > MAX_MAP_H) return 0;
    expected_count = (uint64_t)(unsigned int)map_w * (uint64_t)(unsigned int)map_h;
    return memcmp(header->tag, PHYSICAL_SAVE_TAG, sizeof(header->tag)) == 0 &&
           header->version == MAP_SAVE_WORLD_PHYSICAL_BLOCK_VERSION &&
           header->item_size == sizeof(PhysicalSaveTile) &&
           header->map_w == (uint32_t)map_w && header->map_h == (uint32_t)map_h &&
           header->count == expected_count && expected_count <= MAX_MAP_W * MAX_MAP_H;
}

int map_save_world_physical_skip(FILE *file,
                                 int expected_map_w, int expected_map_h) {
    PhysicalSaveHeader header;
    uint64_t payload_bytes;
    if (!file || fread(&header, sizeof(header), 1, file) != 1 ||
        !header_valid(&header, expected_map_w, expected_map_h)) return 0;
    payload_bytes = (uint64_t)header.count * sizeof(PhysicalSaveTile);
    return payload_bytes <= LONG_MAX &&
           fseek(file, (long)payload_bytes, SEEK_CUR) == 0;
}

static int read_physical_payload(FILE *file, int expected_map_w,
                                 int expected_map_h, int commit) {
    PhysicalSaveHeader header;
    PhysicalSaveTile chunk[PHYSICAL_SAVE_CHUNK];
    WorldPhysicalTileState *tiles;
    uint32_t checksum = 2166136261u;
    int count;
    int offset;
    int ok = 0;
    if (!file || fread(&header, sizeof(header), 1, file) != 1 ||
        !header_valid(&header, expected_map_w, expected_map_h)) return 0;
    count = (int)header.count;
    tiles = (WorldPhysicalTileState *)malloc((size_t)count * sizeof(*tiles));
    if (!tiles) return 0;
    for (offset = 0; offset < count; offset += PHYSICAL_SAVE_CHUNK) {
        int i;
        int remaining = count - offset;
        int batch = remaining < PHYSICAL_SAVE_CHUNK ? remaining : PHYSICAL_SAVE_CHUNK;
        if (fread(chunk, sizeof(chunk[0]), (size_t)batch, file) != (size_t)batch) goto done;
        checksum = checksum_bytes(checksum, chunk, (size_t)batch * sizeof(chunk[0]));
        for (i = 0; i < batch; i++) {
            tiles[offset + i] = state_tile(&chunk[i]);
        }
    }
    if (checksum != header.checksum ||
        !world_physical_state_validate(expected_map_w, expected_map_h, tiles, count)) goto done;
    ok = !commit ||
        world_physical_state_commit(expected_map_w, expected_map_h, tiles, count);
done:
    free(tiles);
    return ok;
}

int map_save_world_physical_validate(FILE *file,
                                     int expected_map_w, int expected_map_h) {
    long offset;
    int valid;
    if (!file) return 0;
    offset = ftell(file);
    if (offset < 0) return 0;
    valid = read_physical_payload(file, expected_map_w, expected_map_h, 0);
    if (fseek(file, offset, SEEK_SET) != 0) return 0;
    return valid;
}

int map_save_world_physical_read(FILE *file, int expected_map_w,
                                 int expected_map_h) {
    return read_physical_payload(file, expected_map_w, expected_map_h, 1);
}
