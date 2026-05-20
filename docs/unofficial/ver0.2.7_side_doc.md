# Ver0.2.7 Side Doc

Ver0.2.7 is a stabilization release over Ver0.2.6.b. It focuses on UI
responsiveness, event-history reliability, world-generation progress accuracy,
map legend clarity, and truce presentation.

## Main Changes

1. Added per-civilization gameplay event history keyed by stable civilization
   identity, then exposed it through `RenderSnapshot` for country recent events.
2. Made the right-side panel redraw through an offscreen side-panel buffer so
   partial paints do not leave half-drawn cards or blank fragments.
3. Reduced simulation-driven side-panel invalidation and made hover redraws
   depend on hover-target changes rather than every mouse pixel.
4. Reordered world-generation progress stages to match the actual generation
   flow: terrain, regions, ports, civilizations, shallow routes, deep routes,
   and finalize.
5. Updated map legend rules for political, geography, climate, region, and
   route-potential layers while keeping water terminology limited to shallow
   sea and deep sea.
6. Changed truce durations to 55 years for normal war outcomes, 25 years for
   severed-front interruptions or fallback outcomes, and kept collapse splits
   at 45 years.
7. Added `truce_initial_years` so diplomacy cards can render truce progress
   against the correct starting duration.
8. Cleaned remaining mojibake in diplomacy card labels touched by this release.

## Validation

- `make check-text` is required before the Ver0.2.7 commit.
- `git diff --check` is required before the Ver0.2.7 commit.
- `make` is required before the Ver0.2.7 commit.
- Executable smoke/probe checks are required where the executable is available.
