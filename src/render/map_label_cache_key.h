#ifndef WORLD_SIM_MAP_LABEL_CACHE_KEY_H
#define WORLD_SIM_MAP_LABEL_CACHE_KEY_H

typedef enum {
    MAP_LABEL_DISPLAY_STANDARD = 0,
    MAP_LABEL_DISPLAY_ALLIANCE,
    MAP_LABEL_DISPLAY_REGIONS
} MapLabelDisplayFamily;

typedef struct {
    int label_revision;
    int country_revision;
    int city_revision;
    int map_w;
    int map_h;
    int city_visual_revision;
    int regions_revision;
    int world_generated;
    int alliance_count;
    int alliance_revision;
    int display_mode;
    int language;
} MapLabelSourceKeyInput;

typedef struct {
    unsigned int source_key;
    int display_mode;
    int zoom_percent;
    int tile_size;
    int map_x;
    int map_y;
    int draw_w;
    int draw_h;
    int viewport_left;
    int viewport_top;
    int viewport_w;
    int viewport_h;
    int selected_civ;
    int selected_x;
    int selected_y;
} MapLabelPlacementKeyInput;

MapLabelDisplayFamily map_label_cache_display_family(int display_mode);
int map_label_cache_display_family_changed(int old_mode, int new_mode);
int map_label_cache_content_tile_size(int tile_size);
unsigned int map_label_cache_source_key(const MapLabelSourceKeyInput *input);
unsigned int map_label_cache_placement_key(const MapLabelPlacementKeyInput *input);

#endif
