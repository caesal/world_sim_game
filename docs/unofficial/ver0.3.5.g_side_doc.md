# Ver0.3.5.g Side Doc

Ver0.3.5.g is a presentation and World Announcement backup checkpoint over
Ver0.3.5.f.

This checkpoint intentionally does not update `docs/official`; the release
record lives in this side doc and `docs/unofficial/version_log.md`.

## Main Changes

1. Bumped `WORLD_SIM_VERSION` to `0.3.5.g`.
2. Added structured World Announcements with stable event ids, stable
   country/alliance identity snapshots, localized formatting, and compact
   RenderSnapshot delivery.
3. Added approved Critical, Major, and Normal announcement classifications for
   world-scale collapse, union, alliance, vassal, plague, technology/deep-route,
   and Military Alliance war events.
4. Added queue ordering, deduplication, priority preemption/resume, pagination,
   hover pause, Locate, Dismiss, and bounded durations.
5. Rendered announcements as an 82px transient map overlay below the unchanged
   top bar, with exact 191/255 alpha and cached 20/18/16px fonts.
6. Added identity-colored rich text, cached PNG icons, fresh-underlay checks,
   atomic partial-band presentation, and non-overlapping Action Toast layout.
7. Unified bottom controls/status blocks on one 30px row and centered the
   status dot from its actual rectangle.
8. Applied Country/independent Alliance fill alpha 136/255 and Alliance-member
   alpha 176/255 in live and snapshot map paths.
9. Changed Tense-page truce ordering from shortest-first to longest-first while
   retaining population and civilization-id tie breaks.
10. Preserved the existing diplomacy-arrow, border/cache scale, live
    province/city, map legend, ocean, alliance, war-card, and draw-order behavior.

## Scope

- Core event log, runtime announcement store, announcement types, and render
  snapshot event sections.
- Simulation announcement observers for collapse, union, alliances, vassals,
  plague global transitions, named technology ages, deep routes, and wars.
- UI announcement queue, snapshot readers, layout, notifications, and input.
- Render announcement resources/surfaces, partial/transient UI presentation,
  icons, map display policy, bottom status geometry, and supporting panels.
- Focused presentation probes for announcement policy, event production,
  queue/UI behavior, layout, Tense ordering, and existing regressions.
- `Makefile`, `build.bat`, `README.md`, `docs/README.md`, this side doc, and
  `docs/unofficial/version_log.md`.

## Behavioral Boundaries

- `MAP_SAVE_VERSION` remains `18`.
- The proposed replacement plague model is not part of this checkpoint.
- Existing plague mechanics are unchanged except for structured observation of
  the existing global active/inactive transition for announcements.
- Ordinary diplomacy changes keep the existing map-arrow logic.
- Country-only wars remain outside the World Announcement policy.
- No speed constants, simulation scheduling, map draw order, world generation,
  route unlock, technology progression, population/economy balance, or save
  schema are intentionally changed.

## Validation Evidence

- Focused presentation summary: `overall_ok=1` with announcement policy,
  queue, layout, alpha, typography, rich text, partial presentation, Tense
  sorting, live province/city, diplomacy-arrow, legend, border, and ocean cases.
- Targeted GUI matrix covered English/Chinese, expanded/collapsed panels,
  multiple resolutions, map modes, toast coexistence, and announcement timeout.
- A 60-frame sequence found no clean-frame gap, ghost reappearance, or
  bottom-bar change caused by the announcement overlay.
- Fresh Rule39 used an Extreme 1152x800 world, 26 initial civilizations, 1,103
  natural regions, max/5x speed, and reached Year 692 Month 1.
- The first five recorded stage-5 civilizations were `0 Leoberg`, `1 Itanaru`,
  `2 Kwaranmbo`, `3 Eberburg`, and `4 Norborg`.
- Deep routes changed from `84 total / 84 shallow / 0 deep` to
  `108 total / 107 shallow / 1 deep`.
- Presentation recorded zero dropped months and zero order skips.

## Residual Risk

- Broad performance remains outside the requested hard thresholds. The final
  Rule39 sample recorded 97ms/month, 32ms render average, and 129ms render peak.
- Ver0.3.5.g is therefore a backup checkpoint with complete flow evidence, not
  a declaration of performance acceptance.
