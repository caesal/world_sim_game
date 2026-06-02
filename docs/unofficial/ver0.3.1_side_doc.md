# Ver0.3.1 Side Doc

Ver0.3.1 is a fragmentation-control checkpoint over Ver0.3.0. It preserves the
render marker, sea-lane stability, and responsiveness work while reducing
runaway country-count growth from enclave separation and collapse successor
creation.

This checkpoint intentionally does not update `docs/official`; the release
record lives in this side doc and `docs/unofficial/version_log.md`.

## Main Changes

1. Bumped `WORLD_SIM_VERSION` to `0.3.1`.
2. Replaced deterministic enclave separation with weighted outcomes:
   independent country, original-owner vassal, strongest land-neighbor
   integration, or unowned collapse.
3. Kept `MAX_CIVS` at 200 and added fallback handling when new country slots or
   land-neighbor candidates are unavailable.
4. Limited collapse successor creation by owned natural-region count:
   up to 35 owned regions creates at most one successor, and larger countries
   create at most two.
5. Reset total disorder and carry after successful collapse for the original
   country and successors while preserving the underlying plague, war,
   resource, migration, and stability pressure sources.
6. Added grace protection against immediate recollapse during the post-collapse
   recovery window.
7. Added fragmentation diagnostics for enclave outcomes, collapse outcomes,
   vassal breaks, alive/independent/vassal counts, one-province counts, and
   country slot usage.

## Behavioral Notes

- One-province enclaves before owner technology stage 6 are much more likely to
  become unowned land, reducing early-game fragmentation pressure.
- Later one-province enclaves favor integration into the strongest land
  neighbor, while still allowing independence and original-owner vassalization.
- Multi-province enclaves keep a higher chance of becoming political entities,
  but no longer route every no-candidate case into sovereign independence.
- Successful collapse relieves current total disorder without erasing the
  underlying causes that can rebuild disorder later.
- Existing route, marker, progress-overlay, UI, and max-speed responsiveness
  changes remain part of this checkpoint.

## Validation

- `WORLD_SIM_VERSION` is `0.3.1`.
- `MAP_SAVE_VERSION` remains 10.
- Build/static validation was reported passing for the fragmentation-control
  implementation.
- Focused probes reported the requested enclave weight tables:
  - One-region tech below 6: `17/16/17/50`.
  - One-region tech 6 or above: `20/20/45/15`.
  - Multi-region tech below 6: `30/25/25/20`.
  - Multi-region tech 6 or above: `25/25/40/10`.
- Reported 20-civ Large run: 708 natural regions, final Year 236 Month 4,
  alive 77, independent 72, vassal 5, one-province 10, slots 77/200,
  enclave outcomes `I 11 / V 10 / J 8 / U 14`, and collapse successors 50.
- Reported 26-civ Large strict run: 698 natural regions, 26/26 placed,
  reached technology stage 5 by Year 366, final captured Year 383 Month 1,
  alive 106, independent 98, vassal 8, one-province 19, slots 112/200,
  enclave outcomes `I 21 / V 18 / J 27 / U 30`, and collapse successors 105.
- The reported strict run inspected map display, labels, routes, borders,
  diplomacy, war, vassals, collapse, plague, logs, and Debug / Performance
  diagnostics.

## Residual Risks

- Slot-full and no-land-neighbor fallback branches were not naturally hit in
  the reported long runs, so they should stay covered by focused branch probes.
- Future tuning should continue to use fragmentation diagnostics rather than
  relying only on final country count.
