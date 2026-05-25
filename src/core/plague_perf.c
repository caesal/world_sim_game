#include "plague_perf.h"

static int plague_system_on = 1;
static int plague_map_visuals_on = 1;
static int plague_sim_skipped;
static int plague_visual_skipped;
static int plague_invalidation_suppressed;

int plague_perf_system_enabled(void) { return plague_system_on; }
int plague_perf_map_visuals_enabled(void) { return plague_map_visuals_on; }
int plague_perf_visuals_allowed(void) { return plague_system_on && plague_map_visuals_on; }

void plague_perf_set_system_enabled(int enabled) { plague_system_on = enabled ? 1 : 0; }
void plague_perf_set_map_visuals_enabled(int enabled) { plague_map_visuals_on = enabled ? 1 : 0; }
void plague_perf_toggle_system(void) { plague_perf_set_system_enabled(!plague_system_on); }
void plague_perf_toggle_map_visuals(void) { plague_perf_set_map_visuals_enabled(!plague_map_visuals_on); }

void plague_perf_begin_frame(void) {
    plague_visual_skipped = 0;
    plague_invalidation_suppressed = 0;
}

void plague_perf_note_sim_skipped(int skipped) { plague_sim_skipped = skipped ? 1 : 0; }
void plague_perf_note_visual_skipped(int skipped) { plague_visual_skipped = skipped ? 1 : 0; }
void plague_perf_note_invalidation_suppressed(int suppressed) {
    if (suppressed) plague_invalidation_suppressed = 1;
}

int plague_perf_sim_skipped(void) { return plague_sim_skipped; }
int plague_perf_visual_skipped(void) { return plague_visual_skipped; }
int plague_perf_invalidation_suppressed(void) { return plague_invalidation_suppressed; }
