# Ver0.2.10.a Side Doc

Ver0.2.10.a is a Windows app-icon refresh checkpoint over Ver0.2.10. It
replaces the packaged executable icon with the new parchment-map icon and keeps
the existing Windows resource pipeline intact.

This release intentionally does not update or include `docs/official`.

## Main Changes

1. Bumped the visible prototype marker to `0.2.10.a`.
2. Replaced `assets/app_icon.png` with a transparent parchment-map icon source.
3. Replaced `assets/app_icon.ico` with a multi-resolution Windows icon generated
   from the new source image.
4. Kept `src/world_sim.rc`, `src/resource.h`, and the Makefile resource build
   path unchanged.

## No Gameplay Changes

This release does not change world generation, region generation, expansion,
population, plague, diplomacy, war, vassal, maritime, sea-lane, route-potential,
render cache behavior, snapshot behavior, or balance rules. The change is
restricted to executable icon packaging and version metadata.

## Known Follow-Up

Large-map stutter is still present. Ver0.2.10.a does not attempt to optimize
rendering or simulation paths.

## Validation

- `make -B world_sim.exe` is required before the Ver0.2.10.a commit.
- `make check-text` is required before the Ver0.2.10.a commit.
- `git diff --check` is required before the Ver0.2.10.a commit.
- All `.c` and `.h` files must remain at or below 500 lines.
- The repository root should contain exactly one executable: `world_sim.exe`.
- Associated-icon extraction should show the new parchment-map icon from the
  rebuilt executable.
- A smoke launch should confirm `world_sim.exe` starts with a responsive main
  window.
