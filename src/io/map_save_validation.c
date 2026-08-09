#include "io/map_save_validation.h"

#include <stdlib.h>
#include <string.h>

#define MAP_SAVE_VALIDATION_ACK "phase2-owned-hwnd-map21"

static int acknowledged(void) {
    const char *ack = getenv("WORLD_SIM_MAP_SAVE_VALIDATION_ACK");
    return ack && strcmp(ack, MAP_SAVE_VALIDATION_ACK) == 0;
}

int map_save_validation_path(char *out, size_t out_size) {
    const char *path = getenv("WORLD_SIM_MAP_SAVE_VALIDATION_PATH");
    size_t length;
    if (!out || out_size == 0 || !acknowledged() || !path) return 0;
    length = strlen(path);
    if (length < 10 || length + 1 > out_size || path[1] != ':' ||
        (path[2] != '\\' && path[2] != '/') ||
        strcmp(path + length - 7, ".wsgmap") != 0) return 0;
    memcpy(out, path, length + 1);
    return 1;
}

int map_save_validation_silent(void) {
    char path[1024];
    return map_save_validation_path(path, sizeof(path));
}
