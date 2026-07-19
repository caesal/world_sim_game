#include "render/map_label_cache_key.h"

#include "ui/ui_types.h"

static unsigned int mix_key(unsigned int key, int value) {
    return key * 1000003u ^ (unsigned int)value;
}

MapLabelDisplayFamily map_label_cache_display_family(int display_mode) {
    if (display_mode == DISPLAY_ALLIANCE) return MAP_LABEL_DISPLAY_ALLIANCE;
    if (display_mode == DISPLAY_REGIONS) return MAP_LABEL_DISPLAY_REGIONS;
    return MAP_LABEL_DISPLAY_STANDARD;
}

int map_label_cache_display_family_changed(int old_mode, int new_mode) {
    return map_label_cache_display_family(old_mode) !=
           map_label_cache_display_family(new_mode);
}

int map_label_cache_content_tile_size(int tile_size) {
    static const unsigned char thresholds[] = {
        1, 2, 3, 4, 5, 6, 7, 8, 10, 12, 14, 16, 18, 20, 22, 24
    };
    int i;
    int result = 0;
    for (i = 0; i < (int)(sizeof(thresholds) / sizeof(thresholds[0])); i++) {
        if (tile_size < thresholds[i]) break;
        result = thresholds[i];
    }
    return result;
}

unsigned int map_label_cache_source_key(const MapLabelSourceKeyInput *input) {
    unsigned int key;
    MapLabelDisplayFamily family;
    if (!input) return 0;
    family = map_label_cache_display_family(input->display_mode);
    key = (unsigned int)input->label_revision;
    key = mix_key(key, input->country_revision);
    key = mix_key(key, input->city_revision);
    key = mix_key(key, input->map_w * 4099 + input->map_h);
    key = mix_key(key, input->city_visual_revision);
    key = mix_key(key, input->regions_revision);
    key = mix_key(key, input->world_generated);
    key = mix_key(key, input->alliance_count);
    if (family == MAP_LABEL_DISPLAY_ALLIANCE)
        key = mix_key(key, input->alliance_revision);
    key = mix_key(key, family);
    return mix_key(key, input->language);
}

unsigned int map_label_cache_placement_key(const MapLabelPlacementKeyInput *input) {
    unsigned int key;
    int zoom_lod;
    int tile_lod;
    int center_bucket_x;
    int center_bucket_y;
    if (!input) return 0;
    zoom_lod = input->zoom_percent >= 135 ? 4 :
               input->zoom_percent >= 120 ? 3 :
               input->zoom_percent >= 90 ? 2 :
               input->zoom_percent >= 70 ? 1 : 0;
    tile_lod = map_label_cache_content_tile_size(input->tile_size);
    center_bucket_x = input->draw_w > 0 ?
        ((input->viewport_left + input->viewport_w / 2 - input->map_x) * 8 +
            input->draw_w / 2) /
            input->draw_w : 0;
    center_bucket_y = input->draw_h > 0 ?
        ((input->viewport_top + input->viewport_h / 2 - input->map_y) * 8 +
            input->draw_h / 2) /
            input->draw_h : 0;
    center_bucket_x = clamp(center_bucket_x, -8, 16);
    center_bucket_y = clamp(center_bucket_y, -8, 16);
    key = input->source_key;
    key = mix_key(key, map_label_cache_display_family(input->display_mode));
    key = mix_key(key, zoom_lod);
    key = mix_key(key, tile_lod);
    key = mix_key(key, input->viewport_w);
    key = mix_key(key, input->viewport_h);
    key = mix_key(key, center_bucket_x);
    key = mix_key(key, center_bucket_y);
    key = mix_key(key, input->selected_civ);
    key = mix_key(key, input->selected_x);
    return mix_key(key, input->selected_y);
}
