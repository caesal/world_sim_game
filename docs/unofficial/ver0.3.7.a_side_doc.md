# Ver0.3.7.a Side Documentation

## Overview

Ver0.3.7.a contains six bounded changes: episode-total plague-immunity
thresholds, map-size-aware Initial Civilizations controls, a fixed diminishing
aridity response with a strict drought oasis window, and plague
RenderSnapshot/cache consistency across unrelated publication revisions, plus
ordinary-pause snapshot/publication coherence and complete collapsed top-bar
map-mode labels. This document records implemented source behavior and the
verified final acceptance results. Git publication is audited separately.

## Plague Immunity

At episode end, every city ever infected in the episode receives immunity based
on the total duration of that episode:

- Less than 80 months: 30%.
- 80-139 months: 50%.
- 140-199 months: 80%.
- 200 months or more: 100%.

Immunity still lasts 480 months. The corresponding infection-candidate weights
remain 70%, 50%, 20%, and 0%. Immunity continues to affect origin/spread
candidate selection rather than severity or mortality.

## Initial Civilizations

The map-size caps are Small 50, Medium 80, Large 115, and Extreme 200. The
global dice uniformly selects from 1..current cap, excludes the current valid
value, and does not change map size. Manual 0 remains valid. Values above the
current cap clamp immediately, and the Hydrology & Regions and Legacy Modules
entries share one synchronized configuration value.

## Aridity, Drought, And Oases

The production response uses C integer division, which truncates toward zero:

```text
base_air = clamp(18 + world_moisture / 2 - drought / 24 + noise / 3, 4, 82)
bias_numerator = (int64_t)bias_desert * 4 * 100
drought_numerator = (int64_t)drought * 2 * (100 - bias_desert)
dryness_response = (bias_numerator + drought_numerator) / 10000
combined_arid_limit = clamp(31 + dryness_response + (world_moisture - 50) * 12 / 25, 0, 100)
semi_arid_band = clamp(12 - bias_desert * 10 / 100 + max(0, 50 - world_moisture) * 8 / 25, 2, 20)
desert_limit = clamp(combined_arid_limit - semi_arid_band, 0, 100)
semi_arid_limit = combined_arid_limit
oasis_limit = 42 - drought * 20 / 100
oasis_transition_limit = combined_arid_limit + drought * 2 / 100
```

An oasis must be on visible land with a river channel and moisture strictly
above `oasis_limit`. It must either have a macro-arid climate or, when drought
is positive, have temperature above 28. When drought is positive, moisture must
also be strictly below `oasis_transition_limit`. A drought-zero macro-arid tile
retains the historical behavior without that upper bound. Delta, lake, coast,
and other geography precedence remains authoritative; no oasis is forced after
classification.

## Plague Snapshot Cache Consistency

The plague RenderSnapshot revision key uses the civilization-source revisions
that can change plague presentation data. Unrelated Decision publication and
war-history revisions remain part of the full civilization presentation key
but no longer invalidate an otherwise current plague payload. Real plague,
lane, city, month, civilization-source, and world-generated changes continue to
invalidate the plague payload normally. This does not add a second monthly
refresh, a render-side cache rebuild, or a new scan.

## Ordinary Pause And Snapshot Publication

Ordinary pause closes new-month admission immediately and cancels only queued
work that has not started. If a month has already started, that month drains
through the existing snapshot cache, Decision cache, and front-publication
phases before the game reports a paused/idle boundary. A resume requested while
that drain is in progress is deferred until coherent publication completes.
World generation, load, reset, and close continue to use a distinct worker
quiescence and hard-reset boundary. The pause path adds no full-map, all-city,
all-civilization, or all-pair scan.

## Collapsed Top-Bar Labels

The collapsed map-mode lane now uses `MIN_SIDE_PANEL_W - 28`, or 472 pixels,
matching the minimum legal expanded side panel. The six-pixel gaps, font,
strings, Reset/language controls, and expanded geometry remain unchanged;
visual and hit rectangles stay aligned. `DT_END_ELLIPSIS` remains available for genuinely constrained
windows, including the existing synthetic 620-pixel fallback.

Eight 2560x1369 deterministic artifacts cover EN/ZH, panel widths 500/720, and
expanded/collapsed states. All 48 labels and 48 hit targets passed. English
`Geography` measures 71 pixels in a 73-pixel button text area, retaining one
pixel on each side. Original-resolution review confirmed complete labels and
no overlap in those artifacts, the six corrected English collapsed live views,
and the final Rule 39 captures. The full presentation registry is 632/632/632
expected/registered/actual, with zero missing, unexpected, duplicate, or
outside-write artifacts.

## Save And Compatibility

`WORLD_SIM_VERSION` is `0.3.7.a`. `MAP_SAVE_VERSION` remains 21. MAP20 and older
saves continue to be rejected without a migration promise.

## Validation And Publication Status

Final Rule 47 Steps 1-9 and the fresh final Rule 39 passed on 2026-09-04.
Original-resolution manual review then passed all 23 final review checks.
Step 3's 96-world/96-histogram/48-artifact binding was explicitly classified
`REUSED_UNAFFECTED` after a 357-file dependency/hash audit; it was not described
as a new run. Final targeted GUI and Rule 39 used the accepted final executable.

The accepted Rule 39 world used Extreme 1152x800 with randomized physical and
advanced terrain parameters. It began with 64 civilizations and 1,212 natural
regions and ended at Year 672 Month 5 with 64 civilizations alive. It ran at
max/5x before the final paused captures. There were 1,443 wars started and
15 named plague episodes; war, vassal, collapse, plague, realtime map, and
performance checks passed.

The first five qualifying stage-5 witnesses were:

| ID | UID | Name | Stage | First recorded year/month |
|---:|---:|---|---:|---|
| 28 | 29 | Gratia | 5 | 537/12 |
| 48 | 49 | Regidoren | 5 | 546/1 |
| 58 | 59 | Qiqi | 5 | 550/12 |
| 18 | 19 | Chakukora | 5 | 559/7 |
| 8 | 9 | Lucheng | 5 | 560/11 |

Before unlock, total/shallow/deep route counts were 0/0/0; after reveal they
were 43/41/2. The final 60 recorded frames followed 10 warmups and lasted
19.842 seconds, with one full-frame, state, and geometry hash each. GDI stayed
171, USER stayed 24, and Private Bytes stayed 2,325,651,456. Working Set was
recorded as diagnostic-only. All 333 artifacts and 279 images passed integrity
checks, and all 190 unique image groups were reviewed at original resolution.
The owned game process exited normally with code 0 and no game process remained.

Accepted executable identity:

- File: `world_sim.exe`.
- Size: 3,173,605 bytes.
- Build timestamp: `2026-09-04T18:09:09.9352618Z`.
- SHA-256: `9672F877F4614B24746205AF3D4CC86C1A3681107C061E19DD2652A3B1CFDE7B`.

The immutable acceptance evidence is retained locally under
`build/validation/ver037a_collapsed_topbar_correction_20260904/attempt_04/`:

- Rule 47: `04_rule47/final_ledger.json`, SHA-256
  `A73CA487E8CD72C54C7D11E7B2883F1496919EBB05AC27ACA3B9B15F6F0E6778`.
- Rule 39: `05_rule39/final_run_01/summary.json`, SHA-256
  `EC90B053B585D394B2175020456F71D13C3226DC633FF8E403561230837F9503`.
- Manual review: `06_manual_review/final_review_result.json`, SHA-256
  `D3531448ED91FC7BF509476665C599DB7A6CEAC24523FBACB4D6FE9CB4AB46DE`.

This release closeout changes only four documentation files relative to that
accepted product state. Source, production inputs, executable, and historical
acceptance evidence remain unchanged. The user separately authorized staging,
commit on dev, lightweight tag `ver0.3.7.a`, and an atomic push; their actual
outcome is recorded by the release audit and remote references. Acceptance PASS
alone is not a claim that a push has succeeded. No GitHub Release is part of
this closeout.
