# Ver0.2.6.b Side Doc

Ver0.2.6.b is a patch release over Ver0.2.6.a focused on UI/render stability,
map readability, and release metadata consistency. It does not add new gameplay
systems.

## Main Changes

1. Added a custom HSV civilization color picker with exact manual apply, cancel
   safety, auto-avoid preview, and modal pause behavior.
2. Kept manual color choices exact unless the player explicitly uses the
   auto-avoid action.
3. Standardized player-facing water display around shallow sea and deep sea
   snapshot fields.
4. Polished world-generation progress so the central overlay owns the full
   progress bars and the right panel only shows concise status.
5. Tuned river rendering to reduce visual thickness and stacked outline weight.
6. Improved natural-region shape validation diagnostics and strip/sliver repair.
7. Added priority-based label collision handling: country names, capital names,
   city names, then province/region names.
8. Kept city/capital icons visible even when their text labels are suppressed.

## Validation

- `make check-text` is required before the Ver0.2.6.b commit.
- `git diff --check` is required before the Ver0.2.6.b commit.
- `make` is required before the Ver0.2.6.b commit.
- Probe runs are required where the executable is available and not locked.
