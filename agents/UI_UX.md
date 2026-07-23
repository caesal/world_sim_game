# UI And UX Instructions

`AGENTS.md` is the repository source of truth. These rules apply to UI,
rendering, map presentation, localization, interaction, and the long-term clay
visual system.

## 1. Presentation Boundary

- UI/UX work is presentation-only unless the user explicitly approves gameplay
  or model changes.
- Existing visible information and behavior are sacred. Do not delete, hide,
  weaken, simplify away, or silently replace panel sections, debug rows,
  labels, buttons, tabs, forms, sliders, tooltips, legends, or map modes.
- Preserve all existing interactions and dense operational information.
- If content does not fit a chunkier visual style, adapt spacing, density,
  grouping, scrolling, and responsive layout without removing information.
- Compile-only includes and build-list changes are allowed only when needed for
  presentation modules and must not change gameplay.

## 2. Gameplay Non-Goals

Unless expressly approved, do not modify world generation, terrain, geography,
climate, wind, rivers, coasts, water depth, regions, provinces, natural regions,
spawns, expansion, ports, harbors, route potential, maritime routes, sea lanes,
diplomacy, war, vassals, collapse, plague, population, economy, resources,
technology progression, speed semantics, or balance constants.

## 3. Unified Clay System

- Use one coherent clay theme instead of per-panel ad hoc colors.
- Centralize color, radius, spacing, shadow, highlight, typography, state, and
  accent tokens.
- Reuse clay primitives/widgets rather than copying rounded rectangles,
  highlights, or shadow code into panels.
- Inspect existing `src/ui/ui_theme.*`, `src/ui/ui_widgets.*`,
  `src/ui/ui_clay_*`, and `src/render/render_common.*` before naming new modules.
- Keep cards at a restrained radius unless the established theme requires
  otherwise. Do not nest decorative cards inside cards.
- Use familiar icons for icon actions; use text or icon-plus-text only for clear
  commands. Provide tooltips for unfamiliar icons.
- Use segmented controls for modes, toggles/checkboxes for binary settings,
  sliders/steppers/inputs for numeric values, menus for option sets, and tabs
  for views.

## 4. Rendering And State Ownership

- Rendering never mutates simulation state.
- Draw paths read `RenderSnapshot`, snapshot helpers, or presentation caches,
  not mutable live simulation globals.
- Do not hold simulation/state locks during drawing, text layout, shadow
  generation, formatting, or cache rebuilds.
- Keep map rendering and UI shell rendering separate. Do not bake dynamic UI or
  simulation overlays into static physical-map caches.
- Panel cache keys must exclude unrelated camera state unless the panel actually
  displays it.
- Hover, pressed, selected, disabled, and focused transitions invalidate the
  smallest practical area.

## 5. Performance Guardrails

- Clay shadows, rounded surfaces, and highlights must not create visible
  stutter.
- Avoid per-frame `CreatePen`, `CreateBrush`, `CreateFont`, bitmap, or
  compatible-DC churn.
- Cache stable repeated surfaces and geometry with bounded ownership and clear
  revision keys.
- The Debug / Performance panel must remain readable and must not rebuild every
  frame for decorative animation.
- Preserve draw order and realtime correctness for terrain/ocean, political
  ownership, provinces, borders, cities, routes, labels, highlights, legends,
  panels, and controls.
- Do not substitute full-surface flashing or mode-switch rebuilds for targeted
  invalidation.

## 6. Layout And Readability

- Preserve English and Chinese readability through the existing localization
  and UTF-8-safe text path.
- Do not introduce mojibake or source translations from corrupted strings.
- Avoid overlap, clipping, occlusion, unstable dimensions, and font sizes tied
  directly to viewport width.
- Fixed-format boards, grids, toolbars, icons, counters, and tiles need stable
  dimensions or responsive constraints so dynamic content cannot shift layout.
- High-density panels may use lighter clay styling to preserve scanability.
- Do not cover important map content with decorative UI.
- Keep display text sized for its container; reserve large type for actual
  primary headings.
- Do not make the interface visually dominated by one hue family.

## 7. Incremental Migration

Migrate in small buildable and reviewable phases. A typical order is:

1. Theme tokens, primitives, cache plan, and minimal shell integration.
2. Top/bottom bars, buttons, tabs, and pause menu.
3. World-generation controls, sliders, inputs, toggles, and setup forms.
4. Country lists/details, cards, and diplomacy subviews.
5. Population, plague, map/info, and Debug / Performance polish.
6. Interaction states and performance cleanup.

Do not perform a whole-interface conversion in one phase. Each phase preserves
all existing content and stops for visual review.

## 8. Required UI Validation

Follow `agents/VALIDATION.md`. In addition, inspect and capture all states
affected by the task, including as applicable:

- Game launch, map display, and preserved side-panel content.
- World-generation progress while generation is active.
- Generated political colors immediately after generation.
- All affected map modes, including route potential and its correct legend.
- Selected tile/country highlighting near viewport edges and lower corners.
- Bottom-bar play/speed buttons at every speed, with visible icons/text.
- Country, Population, World, Plague, and Debug / Performance panels.
- Pause menu, top bar, bottom bar, side-panel scrolling, forms, and every
  modified control state.
- Diplomacy cards at empty, sparse, and dense states, checking vertical space,
  clipping, and information density.
- English and Chinese at required narrow/wide viewport sizes.
- Hover, pressed, selected, disabled, focused, drag, release-outside, paging,
  scrolling, empty, loading, active, completed, and fallback states relevant to
  the change.

Validation fails if screenshots show missing progress UI, stale political
colors, wrong legend content, clipped glyphs, incorrect highlight coverage,
stale provinces/cities, selected extents that disagree with the map, excessive
empty card space, missing controls, clipped labels, stale interaction states,
rapid flicker, or square artifacts behind rounded controls.

UI reports must include enough distinct screenshot evidence for human review;
a prose checklist is insufficient. Report exact changed files, whether gameplay
files were touched, content preservation, build/check results, screens and
states inspected, visual compromises, cache/performance risks, and whether the
approved styling phase was actually started. Confirm that no existing control
disappeared and that no gameplay file changed except an explicitly allowed
compile-only include or build-list necessity.
