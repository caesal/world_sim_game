#ifndef RIVERS_H
#define RIVERS_H

#include "core/world_types.h"
#include "world/river_types.h"
#include "world/world_gen_context.h"

int world_gen_hydrology_build(WorldGenContext *context);
const RiverNetworkView *river_network_latest_view(void);
int river_network_view_matches_context(const WorldGenContext *context);
int river_network_count_legacy_paths(const WorldGenContext *context);
void river_generation_last_diagnostics(RiverGenerationDiagnostics *out);
int river_generation_committed_diagnostics(RiverGenerationDiagnostics *out);
void river_generation_note_commit(const RiverGenerationDiagnostics *diagnostics);
int river_network_copy_legacy_paths(RiverPath *paths, int capacity);
int river_network_legacy_path_count_required(void);
void river_network_release_transient(void);
void generate_rivers(int moisture, int bias_wetland);

#endif
