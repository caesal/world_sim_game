#include "game/game_presentation_water_river_probe.h"
#include "game/game_presentation_coast_artifact_probe.h"
#include "game/game_presentation_coast_multiscale_probe.h"
#include "game/game_presentation_coast_protection_probe.h"
#include "game/game_presentation_lake_semantics_probe.h"
#include "game/game_presentation_river_lod_probe.h"
#include "game/game_presentation_river_style_probe.h"
#include "game/game_presentation_river_terminal_probe.h"
#include "game/game_presentation_water_surface_probe.h"

int game_presentation_water_river_probe(
    FILE *summary, StaticPhysicalProbeCanvas *canvas,
    const RenderSnapshot *snapshot) {
    int water_ok = game_presentation_water_surface_probe(
        summary, canvas, snapshot);
    int lake_semantics_ok = game_presentation_lake_semantics_probe(summary);
    int river_ok = game_presentation_river_lod_probe(
        summary, canvas, snapshot);
    int river_style_ok = game_presentation_river_style_probe(
        summary, canvas, snapshot);
    int coast_artifact_ok = game_presentation_coast_artifact_probe(
        summary, canvas, snapshot);
    int coast_multiscale_ok = game_presentation_coast_multiscale_probe(
        summary, snapshot);
    int coast_protection_ok = game_presentation_coast_protection_probe(
        summary, snapshot);
    int river_terminal_ok = game_presentation_river_terminal_probe(summary);
    return water_ok && lake_semantics_ok && river_ok && river_style_ok &&
           coast_artifact_ok && coast_multiscale_ok && coast_protection_ok &&
           river_terminal_ok;
}
