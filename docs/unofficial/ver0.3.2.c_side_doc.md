# Ver0.3.2.c Side Doc

Ver0.3.2.c is a focused mixed checkpoint over Ver0.3.2.b. It accepts the
war/diplomacy settlement cleanup requested after observing inverted war results,
and it accepts the Phase 4 Country/Diplomacy clay presentation pass.

This checkpoint intentionally does not update `docs/official`; the release
record lives in this side doc and `docs/unofficial/version_log.md`.

## Main Changes

1. Bumped `WORLD_SIM_VERSION` to `0.3.2.c`.
2. Reworked peace-pressure settlement so attacker and defender roles determine
   the branch:
   - defender-only willingness: attacker wins through the surrender/cession path.
   - attacker-only willingness: no winner, offensive halt, 25-year truce.
   - both willing: no winner, negotiated truce, 25-year truce.
   - severed front: no winner, front-severed interruption, 25-year truce.
   - hard military defeat or decisive cession: existing winner/loser path, with
     55-year truce after successful cession.
3. Added last-war result kinds for military win/loss, surrender, negotiated
   truce, offensive halt, and severed-front interruption.
4. Vassalized a defeated country when actual transferred regions are zero, or
   when post-settlement disorder/cohesion meet either severe-instability rule:
   disorder at least 80 and cohesion at most 3, or disorder at least 92 and
   cohesion at most 4.
5. Deducted one cohesion when any civilization loses its capital.
6. Changed collapse and enclave successor cohesion to parent cohesion plus a
   random 1-3 bonus, capped at 10, instead of inheriting the exact parent value.
7. Applied Phase 4 clay presentation to Country list cards, selected summary,
   action buttons, overview metric chips, diplomacy tabs, diplomacy cards,
   semantic relation accents, truce chip spacing, and vassal hierarchy rows.
8. Updated active-war diplomacy cards to show country names above troop counts,
   attacker/defender roles under troop counts, casualties and wins on the first
   metric row, and front/disorder on the second metric row.
9. Sorted War & Truce relations with active wars first and truces by remaining
   years descending.
10. Ignored local UIUX screenshot evidence under `logs/uiux_phase4_evidence/`.

## Files In Scope

- `.gitignore`
- `logs/expansion_probe.txt`
- `src/core/version.h`
- `src/render/panel_country.c`
- `src/render/panel_country_actions.c`
- `src/render/panel_country_cards.c`
- `src/render/panel_country_detail.c`
- `src/render/panel_country_diplomacy.c`
- `src/render/panel_country_diplomacy_cards.c`
- `src/sim/collapse.c`
- `src/sim/diplomacy.c`
- `src/sim/diplomacy.h`
- `src/sim/enclave_resolution.c`
- `src/sim/war.c`
- `src/sim/war_resolution.c`
- `src/sim/war_resolution.h`
- `src/ui/ui_clay_theme.c`
- `src/ui/ui_clay_theme.h`
- `src/ui/ui_clay_widgets.c`
- `src/ui/ui_clay_widgets.h`
- `README.md`
- `docs/README.md`
- `docs/unofficial/version_log.md`
- `docs/unofficial/ver0.3.2.c_side_doc.md`

## Behavioral Notes

- This is not presentation-only: it includes narrow war/diplomacy settlement
  behavior changes explicitly requested by the user.
- The UIUX portion remains presentation-scoped and does not intentionally change
  world generation, terrain, route rules, plague, population, economy,
  technology, saves, or balance constants.
- Active-war cards read disorder from the existing snapshot civilization data;
  no new snapshot field was added for this display.
- Vassalization after failed cession or severe instability uses the existing
  vassal relation path.
- Local validation screenshots remain on disk but are ignored rather than
  committed as release source files.

## Validation

- `WORLD_SIM_VERSION` is `0.3.2.c`.
- Canonical `make -B world_sim.exe` was attempted first and reached the link
  step, but the running `world_sim.exe` was locked by the operating system.
- A temporary-target build with `TARGET=tmp_worldsim_ver032c_verify.exe`
  succeeded, string checks found `World Sim Game Ver 0.3.2.c`, and the
  temporary executable was deleted.
- `cmd /c build.bat` was attempted and reached the link step, but was blocked by
  the same locked canonical executable.
- `make check-text` passed.
- `git diff --check` passed with only CRLF conversion warnings.
- Static checks found no `.c` file includes another `.c`.
- All scanned source/header files are at or below 500 lines.
- Root executable inventory contains exactly `world_sim.exe`.
- Focused war validation covered defender-only surrender, attacker-only
  offensive halt, both-willing negotiated truce, hard military defeat, and
  severed-front interruption.
- Focused UIUX validation covered the Country overview metric grid, active and
  truce diplomacy cards, truce sorting, vassal hierarchy rows, route-potential
  route-only legend, generated political map, world-generation progress overlay,
  speed buttons, and Debug / Performance readability.

## Residual Risks

- Full strict AGENTS Large-map regression was not rerun for this mixed
  gameplay/UIUX checkpoint.
- Canonical `world_sim.exe` could not be relinked while the running executable
  was locked, so the release was verified through the approved temporary-target
  build path.
- Full strict AGENTS validation remains the main known validation gap.
