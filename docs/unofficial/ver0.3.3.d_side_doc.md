# Ver0.3.3.d Side Doc

Ver0.3.3.d is a focused diplomacy-state, relation-score, alliance-highlight,
and diplomacy UI explanation checkpoint over Ver0.3.3.c. It keeps the
Ver0.3.3.c player-country action base, then adds first-class alliance display,
directional relation scores, relation-factor explanation UI, and player-facing
alliance commands.

This checkpoint intentionally does not update `docs/official`; the release
record lives in this side doc and `docs/unofficial/version_log.md`.

## Main Changes

1. Bumped `WORLD_SIM_VERSION` to `0.3.3.d`.
2. Added Alliance as a first-class diplomacy state for display, AI war blocking,
   player Declare War feedback, debug filtering, event logs, and map animation.
3. Added post-war cooling behavior so ordinary truce expiry no longer
   immediately loops back into endless war under normal pressure.
4. Added directional relation scores from one country toward another, with
   cached yearly factor breakdowns copied into render snapshots.
5. Replaced the previous diplomacy subtab grouping with Alliance, Peace, Tense,
   War, and Vassal groups.
6. Added five-year soft transition preparation and ten-year soft-state grace
   behavior for diplomacy state changes.
7. Added blue map highlighting for selected-country allies while preserving
   selected, vassal/overlord, and war highlight priority.
8. Added player Alliance and Dissolve commands. Alliance uses a blue target
   arrow; Dissolve clears the selected country's alliances back to Peace.
9. Allowed player-forced Declare War against an ally by breaking only the
   relevant alliance pair before starting the war.
10. Added a compact relation-score card section and a hover diplomacy ledger
    tooltip explaining positive and negative yearly relation-change factors.
11. Kept diplomacy subview tabs sticky while relation cards scroll.
12. Improved tooltip hover/cache behavior so the ledger is drawn as a
    post-cache overlay instead of being erased by ordinary side-panel repaint.
13. Updated deterministic diplomacy probes and tooltip visual evidence.
14. Updated the in-game pause-menu version summary in English and Chinese.
15. Updated the root README, documentation index, version log, and this side doc.

## Files In Scope

- `Makefile`
- `build.bat`
- `src/core/event_log.c`
- `src/core/game_types.h`
- `src/core/render_snapshot.c`
- `src/core/render_snapshot.h`
- `src/core/render_snapshot_cache.c`
- `src/core/render_snapshot_sections.c`
- `src/core/render_snapshot_sections.h`
- `src/core/version.h`
- `src/game/game.h`
- `src/game/game_diplomacy_probe.c`
- `src/game/game_diplomacy_relation_probe.c`
- `src/game/game_diplomacy_relation_probe.h`
- `src/game/game_diplomacy_tooltip_probe.c`
- `src/game/game_diplomacy_tooltip_probe.h`
- `src/game/game_diplomacy_visual_probe.c`
- `src/game/game_diplomacy_visual_probe.h`
- `src/game/game_player_actions.c`
- `src/game/game_player_actions.h`
- `src/main.c`
- `src/render/country_target_arrow.c`
- `src/render/diplomacy_map_anim.c`
- `src/render/map_highlight.c`
- `src/render/map_highlight.h`
- `src/render/map_highlight_batch.c`
- `src/render/map_highlight_contours.c`
- `src/render/map_highlight_internal.h`
- `src/render/map_presentation_policy.h`
- `src/render/panel_country.c`
- `src/render/panel_country_actions.c`
- `src/render/panel_country_actions.h`
- `src/render/panel_country_decision.c`
- `src/render/panel_country_detail.c`
- `src/render/panel_country_detail.h`
- `src/render/panel_country_diplomacy.c`
- `src/render/panel_country_diplomacy.h`
- `src/render/panel_country_diplomacy_cards.c`
- `src/render/panel_country_diplomacy_score.c`
- `src/render/panel_country_diplomacy_score.h`
- `src/render/panel_country_diplomacy_tooltip.c`
- `src/render/panel_country_diplomacy_tooltip.h`
- `src/render/panel_debug.c`
- `src/render/panel_diplomacy_cache_key.c`
- `src/render/panel_diplomacy_cache_key.h`
- `src/render/panel_view_model_cache.c`
- `src/sim/decision_snapshot.c`
- `src/sim/decision_snapshot.h`
- `src/sim/diplomacy.c`
- `src/sim/diplomacy_policy.c`
- `src/sim/diplomacy_policy.h`
- `src/sim/diplomacy_relation_score.c`
- `src/sim/diplomacy_relation_score.h`
- `src/sim/diplomacy_stability.c`
- `src/sim/diplomacy_stability.h`
- `src/sim/war.c`
- `src/sim/war_desire.c`
- `src/sim/war_desire.h`
- `src/ui/pause_menu.c`
- `src/ui/ui.c`
- `src/ui/ui_clay_theme.c`
- `src/ui/ui_clay_theme.h`
- `src/ui/ui_country_target.c`
- `src/ui/ui_country_target.h`
- `src/ui/ui_state.c`
- `src/ui/ui_types.h`
- `README.md`
- `docs/README.md`
- `docs/unofficial/version_log.md`
- `docs/unofficial/ver0.3.3.d_side_doc.md`

## Behavioral Notes

- `MAP_SAVE_VERSION` remains `13`; no save layout change is part of this
  checkpoint.
- Diplomacy relation-score diagnostics are cached and copied into render
  snapshots; tooltip drawing should not recalculate simulation factors.
- Alliance, Peace, Tense, War, and Vassal display grouping is a UI/read-model
  presentation over existing diplomacy data plus the new alliance state.
- Truce remains part of the tense display family and keeps its no-war lock.
- Player Alliance and Dissolve are direct player commands and do not add
  multi-country wars or call-to-arms behavior.
- Player-forced Declare War can break the relevant alliance pair before war,
  but AI war desire remains blocked against allies.
- The polished relation tooltip is a UI explanation layer over the current
  numeric factors; it does not change scoring values.

## Validation

Release validation for this commit should record:

- Canonical `make -B world_sim.exe`.
- `cmd /c build.bat`.
- `make check-text`, because player-visible version and tooltip text changed.
- `git diff --check`.
- A `.c` include scan confirming no source file includes another `.c` file.
- Touched `.c/.h` line counts under the 500-line limit.
- A touched-file mojibake scan for edited source and release text.
- `world_sim.exe --probe-diplomacy`.
- Root executable inventory confirming the repository root only contains
  `world_sim.exe`.

Focused deterministic evidence from the implementation passes is recorded
under `build/validation/diplomacy_alliance_probe_20260613/`, including
directional relation probes, alliance highlight stress, diplomacy tab renders,
tooltip hit tests, tooltip overlay persistence probes, and tooltip visual
artifacts.

Live populated Country > Diplomacy hover validation was attempted with
non-disruptive hwnd-scoped automation during implementation, but it did not
produce full positive live-hover visual proof. User-side live checks should be
used before treating the tooltip interaction as fully accepted.

Strict AGENTS Rule39 game-flow regression was not completed for this pushed
checkpoint. This commit must not be treated as full Rule39 release-readiness
evidence until a complete Rule39 run records the final year/month, natural
region count, civilization count, speed setting, five technology-stage-5
civilizations, and deep-sea route hidden-before/revealed-after evidence.

## Residual Risks

- The diplomacy system changed substantially in one checkpoint and should get
  longer live-game observation before future balance changes.
- Tooltip visual evidence is deterministic/offscreen; full live hover evidence
  remains a known validation gap.
- The relation-score system still exposes an unattributed adjustment row when
  cached factor rows do not sum exactly to the displayed yearly delta.
- `src/sim/plague.c` remains an unrelated pre-existing file-size violation.
