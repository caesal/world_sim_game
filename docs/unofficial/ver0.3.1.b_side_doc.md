# Ver0.3.1.b Side Doc

Ver0.3.1.b is an interaction and war-cadence checkpoint over Ver0.3.1.a. It
preserves the plague-cooldown, fragmentation-control, marker, route, and
presentation responsiveness work while tightening country selection, Civil
Unrest, and active-war timing.

This checkpoint intentionally does not update `docs/official`; the release
record lives in this side doc and `docs/unofficial/version_log.md`.

## Main Changes

1. Bumped `WORLD_SIM_VERSION` to `0.3.1.b`.
2. Batched country highlight fill drawing through one transparent overlay blend
   per pass so selecting large countries avoids per-tile DIB/DC/AlphaBlend work.
3. Filled Civil Unrest snapshot enable/blocker fields from collapse logic.
4. Kept Civil Unrest as a non-modal action: clicking it preserves the current
   pause/run state.
5. Made manual Civil Unrest the only exception to collapse grace. Grace still
   blocks pressure and natural collapse, but the manual button can collapse a
   splittable country again while grace is active.
6. Changed active-war battle cadence to 2 years / 24 months.
7. Aligned diplomacy war cards and decision-panel battle countdowns to the new
   24-month cadence.

## Behavioral Notes

- Release Vassal behavior was audited and left unchanged; user validation
  confirmed the button path works.
- Civil Unrest still requires the country to be alive, splittable, and within
  civilization slot capacity.
- Collapse grace remains meaningful for automatic pressure and ordinary collapse
  checks.
- War casualty, odds, cession, peace pressure, truce, support casualty, and
  technology bonus math were not intentionally changed; only the battle cadence
  moved from 3 years to 2 years.

## Validation

- `WORLD_SIM_VERSION` is `0.3.1.b`.
- Build/static validation was reported passing by the implementation agent and
  rerun for the release.
- Focused validation used a Large randomized map with 5 civilizations for this
  user-approved interaction follow-up.
- Focused evidence covered large-country highlight responsiveness, Civil Unrest
  preserving run/pause state, repeated manual Civil Unrest during collapse
  grace, and 24-month battle cadence/countdown behavior.
- Full 26-civilization strict game-flow regression was not rerun for this
  focused checkpoint.

## Residual Risks

- Future gameplay, simulation, rendering, map-display, or performance changes
  should continue to use the strict AGENTS regression gate unless the user
  explicitly accepts focused-only validation for that task.
- Future war tuning should report battle interval, casualties, peace pressure,
  cession, and truce evidence together so cadence changes do not hide balance
  shifts.
