#ifndef WORLD_SIM_DIRTY_FLAGS_H
#define WORLD_SIM_DIRTY_FLAGS_H

void dirty_reset_all(void);
void dirty_mark_world(void);
void dirty_mark_territory(void);
void dirty_mark_province(void);
void dirty_mark_population(void);
void dirty_mark_plague(void);
void dirty_mark_maritime(void);
void dirty_mark_labels(void);
void dirty_mark_all_render(void);

int dirty_render_terrain(void);
int dirty_render_political(void);
int dirty_render_borders(void);
int dirty_render_coast(void);
int dirty_render_labels(void);
int dirty_render_plague(void);
int dirty_render_maritime(void);
int dirty_any_render(void);

void dirty_clear_render_terrain(void);
void dirty_clear_render_political(void);
void dirty_clear_render_borders(void);
void dirty_clear_render_coast(void);
void dirty_clear_render_labels(void);
void dirty_clear_render_plague(void);
void dirty_clear_render_maritime(void);

#endif
