#include "dirty_flags.h"

enum {
    DIRTY_RENDER_TERRAIN = 1 << 0,
    DIRTY_RENDER_POLITICAL = 1 << 1,
    DIRTY_RENDER_BORDERS = 1 << 2,
    DIRTY_RENDER_COAST = 1 << 3,
    DIRTY_RENDER_LABELS = 1 << 4,
    DIRTY_RENDER_PLAGUE = 1 << 5,
    DIRTY_RENDER_MARITIME = 1 << 6,
    DIRTY_RENDER_HYDROLOGY = 1 << 7
};

static unsigned int render_dirty_flags =
    DIRTY_RENDER_TERRAIN | DIRTY_RENDER_POLITICAL | DIRTY_RENDER_BORDERS |
    DIRTY_RENDER_COAST | DIRTY_RENDER_LABELS | DIRTY_RENDER_PLAGUE |
    DIRTY_RENDER_MARITIME | DIRTY_RENDER_HYDROLOGY;

static int terrain_revision = 1;
static int coast_revision = 1;
static int ownership_revision = 1;
static int province_revision = 1;
static int route_revision = 1;
static int label_revision = 1;
static int plague_revision = 1;
static int population_revision = 1;
static int ui_revision = 1;
static int hydrology_revision = 1;

static void mark(unsigned int flags) {
    render_dirty_flags |= flags;
}

static void bump(int *revision) {
    if (++(*revision) <= 0) *revision = 1;
}

static int has(unsigned int flags) {
    return (render_dirty_flags & flags) != 0;
}

static void clear(unsigned int flags) {
    render_dirty_flags &= ~flags;
}

void dirty_reset_all(void) {
    render_dirty_flags = 0;
}

void dirty_mark_world(void) {
    mark(DIRTY_RENDER_TERRAIN | DIRTY_RENDER_COAST | DIRTY_RENDER_POLITICAL |
         DIRTY_RENDER_BORDERS | DIRTY_RENDER_LABELS | DIRTY_RENDER_PLAGUE |
         DIRTY_RENDER_MARITIME | DIRTY_RENDER_HYDROLOGY);
    bump(&terrain_revision);
    bump(&coast_revision);
    bump(&ownership_revision);
    bump(&province_revision);
    bump(&route_revision);
    bump(&label_revision);
    bump(&plague_revision);
    bump(&population_revision);
    bump(&ui_revision);
    bump(&hydrology_revision);
}

void dirty_mark_territory(void) {
    mark(DIRTY_RENDER_POLITICAL | DIRTY_RENDER_BORDERS | DIRTY_RENDER_LABELS);
    bump(&ownership_revision);
    bump(&province_revision);
    bump(&label_revision);
}

void dirty_mark_province(void) {
    mark(DIRTY_RENDER_BORDERS | DIRTY_RENDER_LABELS);
    bump(&province_revision);
    bump(&label_revision);
}

void dirty_mark_population(void) {
    mark(DIRTY_RENDER_LABELS);
    bump(&population_revision);
    bump(&label_revision);
}

void dirty_mark_plague(void) {
    mark(DIRTY_RENDER_PLAGUE);
    bump(&plague_revision);
}

void dirty_mark_maritime(void) {
    mark(DIRTY_RENDER_MARITIME | DIRTY_RENDER_LABELS);
    bump(&route_revision);
    bump(&label_revision);
}

void dirty_mark_hydrology(void) {
    mark(DIRTY_RENDER_HYDROLOGY);
    bump(&hydrology_revision);
}

void dirty_mark_labels(void) {
    mark(DIRTY_RENDER_LABELS);
    bump(&label_revision);
}

void dirty_mark_all_render(void) {
    dirty_mark_world();
}

int dirty_render_terrain(void) { return has(DIRTY_RENDER_TERRAIN); }
int dirty_render_political(void) { return has(DIRTY_RENDER_POLITICAL); }
int dirty_render_borders(void) { return has(DIRTY_RENDER_BORDERS); }
int dirty_render_coast(void) { return has(DIRTY_RENDER_COAST); }
int dirty_render_labels(void) { return has(DIRTY_RENDER_LABELS); }
int dirty_render_plague(void) { return has(DIRTY_RENDER_PLAGUE); }
int dirty_render_maritime(void) { return has(DIRTY_RENDER_MARITIME); }
int dirty_render_hydrology(void) { return has(DIRTY_RENDER_HYDROLOGY); }
int dirty_any_render(void) { return render_dirty_flags != 0; }

int dirty_revision_terrain(void) { return terrain_revision; }
int dirty_revision_coast(void) { return coast_revision; }
int dirty_revision_ownership(void) { return ownership_revision; }
int dirty_revision_province(void) { return province_revision; }
int dirty_revision_route(void) { return route_revision; }
int dirty_revision_label(void) { return label_revision; }
int dirty_revision_plague(void) { return plague_revision; }
int dirty_revision_population(void) { return population_revision; }
int dirty_revision_ui(void) { return ui_revision; }
int dirty_revision_hydrology(void) { return hydrology_revision; }

void dirty_clear_render_terrain(void) { clear(DIRTY_RENDER_TERRAIN); }
void dirty_clear_render_political(void) { clear(DIRTY_RENDER_POLITICAL); }
void dirty_clear_render_borders(void) { clear(DIRTY_RENDER_BORDERS); }
void dirty_clear_render_coast(void) { clear(DIRTY_RENDER_COAST); }
void dirty_clear_render_labels(void) { clear(DIRTY_RENDER_LABELS); }
void dirty_clear_render_plague(void) { clear(DIRTY_RENDER_PLAGUE); }
void dirty_clear_render_maritime(void) { clear(DIRTY_RENDER_MARITIME); }
void dirty_clear_render_hydrology(void) { clear(DIRTY_RENDER_HYDROLOGY); }
