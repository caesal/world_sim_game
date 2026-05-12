# Ver0.2.4 Side Doc

Ver0.2.4 focuses on diplomacy display clarity, vassal tribute/support accuracy,
structured bilingual event logs, and validation through a deterministic technology
stage 10 probe.

## Implementation Notes

- Diplomacy relation cards now use relation-specific visual templates instead of
  forcing one six-metric layout onto every state.
- Vassal tribute display now matches simulation tribute exactly: food, livestock,
  wood, stone, minerals, and water are transferred at 40%; money, population,
  capacity, and habitability are not shown as tribute.
- Vassal army display now exposes total soldiers, callable soldiers, home guard,
  current support use, and support casualties through read-only helpers.
- Event logging now formats copied event entries directly, so snapshot text and
  clickable country payload stay paired.
- Historical event chips validate `civ_uid` before map highlight, preventing
  old logs from highlighting a later country that reused the same slot.

## Validation

- `make`: passed.
- Line count check: every `.c` and `.h` file is at or below 500 lines.
- `world_sim.exe --probe-expansion`: completed through year 500.
- `world_sim.exe --probe-tech10`: reached technology stage 10 naturally at
  year 1178 month 2.
- Stage 10 probe result:
  - expansion x1.20
  - resources x1.20
  - technology progress x1.10
  - deep sea stability 99%
  - deep sea plague spread 0%
  - defense x1.20
  - battle chance +15
  - vassal annexation unlocked
- `world_sim.exe` GUI smoke launch: started successfully and was stopped after
  a short smoke check.

## Performance Observation

The technology stage 10 probe is a blocking simulation probe, not the UI
scheduler path. It reached stage 10 with an average of about 29.29 ms/month on
the deterministic small-map run. UI stutter mitigations remain active through
render snapshot throttling and stale UI cache fallback, so a busy simulation
worker should no longer blank the right panel with a large "Updating data"
fallback.
