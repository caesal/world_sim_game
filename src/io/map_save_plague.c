#include "io/map_save_plague.h"

#include "sim/plague.h"
#include "sim/plague_state.h"

#include <string.h>

typedef struct {
    char tag[8];
    int version;
    int item_size;
    int count;
    int aux_a;
    int aux_b;
} PlagueSaveBlockHeader;

static PlagueModelState save_state;

static int write_all(FILE *file, const void *data, size_t size, size_t count) {
    return file && fwrite(data, size, count, file) == count;
}

static int read_all(FILE *file, void *data, size_t size, size_t count) {
    return file && fread(data, size, count, file) == count;
}

int map_save_plague_write(FILE *file) {
    PlagueSaveBlockHeader header;
    if (!file) return 0;
    memset(&header, 0, sizeof(header));
    memcpy(header.tag, "PLG19", 6);
    header.version = MAP_SAVE_PLAGUE_BLOCK_VERSION;
    header.item_size = (int)sizeof(save_state);
    header.count = 1;
    plague_state_copy(&save_state);
    return write_all(file, &header, sizeof(header), 1) &&
           write_all(file, &save_state, sizeof(save_state), 1);
}

int map_save_plague_read(FILE *file) {
    PlagueSaveBlockHeader header;
    if (!file || !read_all(file, &header, sizeof(header), 1)) return 0;
    if (strncmp(header.tag, "PLG19", sizeof(header.tag)) != 0 ||
        header.version != MAP_SAVE_PLAGUE_BLOCK_VERSION ||
        header.item_size != (int)sizeof(save_state) ||
        header.count != 1 || header.aux_a != 0 || header.aux_b != 0) return 0;
    memset(&save_state, 0, sizeof(save_state));
    if (!read_all(file, &save_state, sizeof(save_state), 1)) return 0;
    if (!plague_state_restore(&save_state)) return 0;
    plague_after_restore();
    return 1;
}
