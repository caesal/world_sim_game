#include "render/panel_country_diplomacy_hits.h"

#include "render/snapshot_ui.h"

typedef struct {
    RECT rect;
    int civ_id;
} DiplomacyHitRect;

static DiplomacyHitRect diplomacy_hits[MAX_CIVS * 2];
static int diplomacy_hit_count = 0;

void country_diplomacy_hit_reset(void) { diplomacy_hit_count = 0; }

void country_diplomacy_hit_add(RECT rect, int civ_id) {
    if (civ_id < 0 || civ_id >= snapshot_ui_civ_count() ||
        diplomacy_hit_count >= (int)(sizeof(diplomacy_hits) / sizeof(diplomacy_hits[0]))) {
        return;
    }
    diplomacy_hits[diplomacy_hit_count].rect = rect;
    diplomacy_hits[diplomacy_hit_count].civ_id = civ_id;
    diplomacy_hit_count++;
}

int country_diplomacy_civ_hit_test(int mouse_x, int mouse_y) {
    int i;
    for (i = diplomacy_hit_count - 1; i >= 0; i--) {
        if (point_in_rect(diplomacy_hits[i].rect, mouse_x, mouse_y)) return diplomacy_hits[i].civ_id;
    }
    return -1;
}
