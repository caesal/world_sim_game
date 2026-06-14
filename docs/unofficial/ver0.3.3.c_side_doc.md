# Ver0.3.3.c Side Doc

Ver0.3.3.c is a focused player-country action and side-panel repaint checkpoint
over Ver0.3.3.b. It keeps the Extreme-map city capacity and save-version work,
then adds direct selected-country commands and removes a flicker-prone direct
side-panel paint path.

This checkpoint intentionally does not update `docs/official`; the release
record lives in this side doc and `docs/unofficial/version_log.md`.

## Main Changes

1. Bumped `WORLD_SIM_VERSION` to `0.3.3.c`.
2. Added selected-country action buttons for Declare War, Peace, Vassalize, and
   Civil Unrest in one consistent row.
3. Added Declare War and Vassalize target modes with pause/restore behavior,
   dynamic red or purple arrows, Esc/right-click cancel, and reusable top
   stacked notifications.
4. Added game-facing player action wrappers for war, peace, and vassalization.
5. Added a no-winner direct-war peace helper for the player Peace command.
6. Improved Declare War failure reporting with specific player-facing reasons.
7. Routed side-panel tab switching and World-tab child-control redraws through
   normal invalidation paths to reduce flicker.
8. Updated the in-game pause-menu version summary in English and Chinese.
9. Updated the root README, documentation index, version log, and this side doc.
10. Added an ignore rule for generated validation evidence under
    `logs/validation/`.

## Files In Scope

- `.gitignore`
- `Makefile`
- `build.bat`
- `src/core/version.h`
- `src/game/game_player_actions.c`
- `src/game/game_player_actions.h`
- `src/render/country_target_arrow.c`
- `src/render/country_target_arrow.h`
- `src/render/panel_country_actions.c`
- `src/render/panel_country_actions.h`
- `src/render/panel_country_detail.c`
- `src/render/panel_country_diplomacy_cards.c`
- `src/render/render.c`
- `src/render/render.h`
- `src/render/top_notifications.c`
- `src/render/top_notifications.h`
- `src/sim/diplomacy.c`
- `src/sim/war.h`
- `src/sim/war_player_peace.c`
- `src/sim/war_query.c`
- `src/ui/pause_menu.c`
- `src/ui/ui.c`
- `src/ui/ui_country_target.c`
- `src/ui/ui_country_target.h`
- `src/ui/ui_debug_input.c`
- `src/ui/ui_forms.c`
- `src/ui/ui_invalidation.c`
- `src/ui/ui_invalidation.h`
- `src/ui/ui_map_input.c`
- `src/ui/ui_map_input.h`
- `src/ui/ui_notifications.c`
- `src/ui/ui_notifications.h`
- `README.md`
- `docs/README.md`
- `docs/unofficial/version_log.md`
- `docs/unofficial/ver0.3.3.c_side_doc.md`

## Behavioral Notes

- Rendering remains read-only from simulation state.
- Target arrows and notifications are dynamic UI/render overlays, not static
  map-cache content.
- Peace is a player-command helper for ending active direct wars without
  assigning victory or defeat.
- Declare War validation now separates common failure categories instead of
  collapsing them into a generic rule-blocked message.
- The side-panel repaint cleanup removes direct `GetDC`-style side-panel
  painting from tab switching.
- `MAP_SAVE_VERSION` remains `13`; no save layout change is part of this
  checkpoint.
- This does not change world generation, AI diplomacy, war settlement scoring,
  economy, population simulation, plague formulas, route rules, or balance
  constants.

## Validation

Release validation for this commit should record:

- Canonical `make -B world_sim.exe`.
- `cmd /c build.bat`.
- `make check-text`, because the pause-menu release text changed.
- `git diff --check`.
- A `.c` include scan confirming no source file includes another `.c` file.
- Touched `.c/.h` line counts under the 500-line limit.
- A touched-file mojibake scan for edited source and release text.
- Root executable inventory confirming the repository root only contains
  `world_sim.exe`.

Focused GUI evidence from the implementation pass is recorded locally under
`logs/validation/sidebar_action_feedback_20260613_154734/`. Those generated
artifacts are intentionally ignored and are not part of the release commit.

Strict AGENTS Rule39 game-flow regression was not completed for this pushed
checkpoint. This commit must not be treated as full Rule39 release-readiness
evidence until a complete Rule39 run records the final year/month, natural
region count, civilization count, speed setting, five technology-stage-5
civilizations, and deep-sea route hidden-before/revealed-after evidence.

## Residual Risks

- Rare Declare War failure branches such as war-slot-full or unusual overlord
  redirection states received code-level separation, but not every rare branch
  has dedicated GUI proof in the focused validation pass.
- Sidebar flicker was checked through targeted tab-switch ROI captures, not a
  full long-run Rule39 performance pass.
- Player-facing action flow should receive another GUI pass after future
  diplomacy, vassal, or war-resolution changes.
