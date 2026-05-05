#include "dirty_flags.h"

enum {
    DIRTY_RENDER_TERRAIN = 1 << 0,
    DIRTY_RENDER_POLITICAL = 1 << 1,
    DIRTY_RENDER_BORDERS = 1 << 2,
    DIRTY_RENDER_COAST = 1 << 3,
    DIRTY_RENDER_LABELS = 1 << 4,
    DIRTY_RENDER_PLAGUE = 1 << 5,
    DIRTY_RENDER_MARITIME = 1 << 6
};

static unsigned int render_dirty_flags =
    DIRTY_RENDER_TERRAIN | DIRTY_RENDER_POLITICAL | DIRTY_RENDER_BORDERS |
    DIRTY_RENDER_COAST | DIRTY_RENDER_LABELS | DIRTY_RENDER_PLAGUE |
    DIRTY_RENDER_MARITIME;

static void mark(unsigned int flags) {
    render_dirty_flags |= flags;
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
         DIRTY_RENDER_MARITIME);
}

void dirty_mark_territory(void) {
    mark(DIRTY_RENDER_POLITICAL | DIRTY_RENDER_BORDERS | DIRTY_RENDER_LABELS);
}

void dirty_mark_province(void) {
    mark(DIRTY_RENDER_BORDERS | DIRTY_RENDER_LABELS);
}

void dirty_mark_population(void) {
    mark(DIRTY_RENDER_LABELS);
}

void dirty_mark_plague(void) {
    mark(DIRTY_RENDER_PLAGUE | DIRTY_RENDER_LABELS | DIRTY_RENDER_MARITIME);
}

void dirty_mark_maritime(void) {
    mark(DIRTY_RENDER_MARITIME | DIRTY_RENDER_LABELS);
}

void dirty_mark_labels(void) {
    mark(DIRTY_RENDER_LABELS);
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
int dirty_any_render(void) { return render_dirty_flags != 0; }

void dirty_clear_render_terrain(void) { clear(DIRTY_RENDER_TERRAIN); }
void dirty_clear_render_political(void) { clear(DIRTY_RENDER_POLITICAL); }
void dirty_clear_render_borders(void) { clear(DIRTY_RENDER_BORDERS); }
void dirty_clear_render_coast(void) { clear(DIRTY_RENDER_COAST); }
void dirty_clear_render_labels(void) { clear(DIRTY_RENDER_LABELS); }
void dirty_clear_render_plague(void) { clear(DIRTY_RENDER_PLAGUE); }
void dirty_clear_render_maritime(void) { clear(DIRTY_RENDER_MARITIME); }
