# Ver0.3.2.d Side Doc

Ver0.3.2.d is a focused render/UIUX checkpoint over Ver0.3.2.c. It accepts the
vassal relation pulse hotfix, Phase 5 clay presentation polish, lighter
centralized clay shadows, and diplomacy map-event animation cleanup.

This checkpoint intentionally does not update `docs/official`; the release
record lives in this side doc and `docs/unofficial/version_log.md`.

## Main Changes

1. Bumped `WORLD_SIM_VERSION` to `0.3.2.d`.
2. Made selected-overlord vassal relation highlights pulse direct vassal focus
   points immediately, matching selected-vassal behavior where both vassal and
   overlord focus points pulse.
3. Applied Phase 5 clay presentation polish to Population, Plague, Map/Info,
   Debug / Performance, and shared progress/metric widgets.
4. Kept Debug / Performance dense diagnostic rows readable while adopting the
   lighter clay presentation around tabs, filters, buttons, event cards, and
   controls.
5. Removed the accidental `render_common -> ui_clay_widgets` dependency by
   keeping clay metric rendering in the concrete Info panel path.
6. Added centralized `shadow_soft` and `shadow_soft_offset` clay style tokens,
   reduced default shadow offsets, and removed the hard-coded soft-shadow color
   from the primitive surface path.
7. Preserved Phase 4 Country/Diplomacy presentation, including truce card
   spacing, semantic relation accents, and vassal hierarchy rows.
8. Fixed diplomacy/event map animations so drawing is clipped to the
   intersection of the map viewport and actual map rectangle.
9. Stored snapshot map dimensions with queued diplomacy map animations and
   skipped stale or invalid endpoints before drawing.
10. Reduced long-distance diplomacy animation bowing and changed war-start map
    animation to a single red attacker-to-defender arrow.

## Files In Scope

- `src/core/version.h`
- `src/render/diplomacy_map_anim.c`
- `src/render/diplomacy_map_anim.h`
- `src/render/map_highlight.c`
- `src/render/panel_country_diplomacy.c`
- `src/render/panel_country_population.c`
- `src/render/panel_debug.c`
- `src/render/panel_debug_controls.c`
- `src/render/panel_info.c`
- `src/render/panel_map.c`
- `src/render/panel_plague_page.c`
- `src/render/panel_population_page.c`
- `src/render/render.c`
- `src/ui/ui_clay_primitives.c`
- `src/ui/ui_clay_theme.c`
- `src/ui/ui_clay_theme.h`
- `src/ui/ui_clay_widgets.c`
- `src/ui/ui_clay_widgets.h`
- `README.md`
- `docs/README.md`
- `docs/unofficial/version_log.md`
- `docs/unofficial/ver0.3.2.d_side_doc.md`

## Behavioral Notes

- This release does not change war settlement, diplomacy rules, vassal rules,
  world generation, route rules, plague simulation, population simulation,
  economy, technology, saves, or balance constants.
- The war-start map animation direction follows the existing structured event
  identity: `civ_id` is the attacker and `target_id` is the defender.
- Diplomacy map animations now use snapshot map dimensions instead of current
  global map dimensions for queued endpoint conversion.
- Phase 5 work is presentation-scoped and preserves existing labels, controls,
  tabs, debug rows, map display modes, and Chinese text rendering.

## Validation

- `WORLD_SIM_VERSION` is `0.3.2.d`.
- Canonical `make -B world_sim.exe` succeeded.
- `cmd /c build.bat` succeeded.
- `make check-text` passed.
- `git diff --check` passed with only CRLF conversion warnings.
- Static checks found no `.c` file includes another `.c`.
- All touched source/header files are at or below 500 lines.
- The repo-wide line-count scan still reports the pre-existing untouched
  `src/sim/plague.c` at 515 lines.
- Root executable inventory contains exactly `world_sim.exe`.
- Focused vassal relation highlight evidence covered selected-vassal and
  selected-overlord pulse behavior.
- Focused UIUX validation covered lighter shadows, Population, Plague no-active
  and active states, route-potential route-only legend, Debug / Performance,
  lower-edge selected highlight, pause menu, speed buttons, and Phase 4
  diplomacy preservation.
- Focused diplomacy animation validation covered long orange animation clipping,
  red war-start attacker-to-defender direction, political map sanity, and
  route-potential route-only legend sanity.

## Residual Risks

- Full strict AGENTS Large-map regression was not rerun for this focused
  render/UIUX checkpoint.
- Plague and diplomacy animation visual evidence used controlled fixtures/probes
  for presentation validation rather than organic long-run emergence.
