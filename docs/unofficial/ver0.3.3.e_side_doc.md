# Ver0.3.3.e Side Doc

Ver0.3.3.e is a focused diplomacy presentation cleanup over Ver0.3.3.d. It
preserves the Ver0.3.3.d diplomacy-state and player-action base, then fixes two
UI/render issues reported after release:

1. Truce diplomacy cards did not show the normal relation score bar or hover
   factor details.
2. Country highlight focus rings could read as a black circular artifact near
   country focus points and city icons.

This checkpoint intentionally does not update `docs/official`; the release
record lives in this side doc and `docs/unofficial/version_log.md`.

## Main Changes

1. Bumped `WORLD_SIM_VERSION` to `0.3.3.e`.
2. Updated truce cards in Country > Diplomacy > Tense so they draw the normal
   relation score summary block before the truce countdown.
3. Preserved truce-specific rows: truce countdown, re-war risk, truce status,
   and war history.
4. Ensured the truce relation score block registers the same hover tooltip hit
   area used by Peace, Tense, and Alliance relation cards.
5. Increased truce card height using the existing relation-score block height
   helper so the added relation block does not clip countdown or chip rows.
6. Softened the map highlight focus-ring outer color from a dark civ-shadow mix
   to a light halo mix.
7. Updated deterministic diplomacy visual probe evidence so the Tense tab
   contains a truce relation and reports truce score tooltip registration.

## Files In Scope

- `src/core/version.h`
- `src/render/panel_country_diplomacy_cards.c`
- `src/render/map_highlight.c`
- `src/game/game_diplomacy_visual_probe.c`
- `src/ui/pause_menu.c`
- `README.md`
- `docs/README.md`
- `docs/unofficial/version_log.md`
- `docs/unofficial/ver0.3.3.e_side_doc.md`

## Behavioral Notes

- This is a UI/render presentation checkpoint.
- No diplomacy state transition logic changed.
- No relation score math changed.
- No war, truce, alliance, vassal, save format, or map generation behavior
  changed.
- `MAP_SAVE_VERSION` remains unchanged.
- Truce remains part of the Tense diplomacy display family.
- The highlight change keeps selected, war, vassal/overlord, and alliance
  highlight behavior intact while reducing the black-ring appearance.

## Validation

Release validation for this commit should record:

- Canonical `make -B world_sim.exe`.
- `cmd /c build.bat`.
- `git diff --check`.
- `.c` include scan confirming no source file includes another `.c` file.
- `make check-text`, because the in-game version summary and release text
  changed.
- Touched `.c/.h` line counts under the 500-line limit.
- Touched-file mojibake marker scan.
- `world_sim.exe --probe-diplomacy`.
- Root executable inventory confirming the repository root only contains
  `world_sim.exe`.

Focused deterministic evidence from the implementation pass is recorded under:

- `build/validation/diplomacy_alliance_probe_20260613/summary.txt`
- `build/validation/diplomacy_alliance_probe_20260613/tab_tense.bmp`
- `build/validation/diplomacy_alliance_probe_20260613/alliance_highlight_sample.bmp`

The probe summary should include:

- `case=diplomacy_tab_render ok=1 truce_score_tooltip_hits=1`
- `case=alliance_highlight_render ok=1`
- `overall_ok=1`

Strict AGENTS Rule39 game-flow regression was not completed for this pushed
checkpoint. This commit must not be treated as full Rule39 release-readiness
evidence until a complete Rule39 run records the final year/month, natural
region count, civilization count, speed setting, five technology-stage-5
civilizations, and deep-sea route hidden-before/revealed-after evidence.

## Residual Risks

- Live populated GUI hover validation was not completed for this focused
  presentation fix; deterministic/offscreen evidence is available.
- User-side visual inspection is still useful for confirming that the softened
  focus halo no longer reads as a black ring on varied map backgrounds.
- `src/sim/plague.c` remains an unrelated pre-existing file-size violation.
