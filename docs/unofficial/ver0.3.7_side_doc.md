# Ver0.3.7 Side Doc

Ver0.3.7 is the cumulative Decision, Stability, completed-war-history, and
ocean-coherence release over Ver0.3.6.c. It passed final acceptance and does
not update the separately maintained `docs/official` documentation series.

## Coherent Decision Snapshots

- Every alive civilization receives one UID-matched coherent Decision snapshot
  for each completed month, including civilizations with late IDs.
- Published and building generations remain separate. The published generation
  stays readable until the complete next generation is atomically published.
- Selected-country rendering consumes immutable snapshot data only. Pausing,
  country selection, Decision subtab changes, language changes, and redraws do
  not calculate Decision state.
- Reused civilization slots require the current UID and cannot expose a prior
  civilization's Decision snapshot.

## Stability Presentation

- Country Detail > Decision > Stability uses one restrained Stability Intent
  meter and a fixed 2x3 factor composition.
- The six published factors are Base Pressure, War Status, Territory
  Fragmentation, Capital Connectivity, Vassal Governance, and High Disorder.
- Base Pressure displays only its numeric contribution. The Stability page does
  not duplicate the complete Disorder meter.
- Monthly Total, Active Limits, Gate Result, and Peace pressure remain visible
  and read the same coherent monthly snapshot.

## Completed-War History

- Each civilization owns exactly three bounded completed principal-war
  summaries, newest first; a fourth completion evicts only the oldest record.
- Country Detail > Diplomacy > War keeps the national army summary first, then
  the completed-war cards, followed by the active-war presentation.
- Each record keeps the selected/local principal on the left and the opponent
  on the right, and freezes their identities, localized exact result, principal
  casualties, actual transferred territory, actual indemnity and beneficiary
  direction, ending year/month, and duration.
- Ally or supporter participation alone does not create a record. Stable war
  serials prevent duplicate terminal publication, and slot reuse cannot inherit
  another civilization's history.

## Ocean Coherence

- Ocean inside and outside the playable map uses one authoritative texture
  coordinate system, native scale, tiling phase, and sharpness.
- Camera, display-mode, month, language, and selection changes reuse prepared
  ocean and motif layers instead of rerasterizing the immutable texture.
- Before generation, the viewport shows the same ocean texture without ships,
  monsters, other generated-world motifs, or a stale prior map.
- Generated-world ocean motifs remain post-generation decoration, while inland
  lakes retain their separate treatment.

## Save And Compatibility

- `WORLD_SIM_VERSION` is `0.3.7`.
- `MAP_SAVE_VERSION` is `21` because bounded war history and active-war serial
  metadata are persisted.
- MAP20 and older files are rejected cleanly. No migration or backward save
  compatibility is promised.

## Preserved Gameplay Scope

This release does not retune gameplay formulas, AI decisions, Decision
arithmetic, war outcomes, casualty or settlement formulas, world generation,
climate, hydrology, routes, plague, diplomacy classification, RNG order,
scheduler ordering, or simulation-speed semantics.

## Validation Status

The cumulative worktree implementation and focused visual evidence were
reviewed before manifest-driven copy-back. The ordered Rule 47 prerequisites
then passed from the final source, followed by one fresh Rule 39 acceptance run
as the last product gate on 2026-08-09. That run completed at Year 693 Month 4
with 1,295 natural regions, 64 initial civilizations, 57 final civilizations,
and no known actionable visual or product defect. The accepted executable was
3,018,003 bytes with SHA-256
`605A9D7FCEE2AB353203CC4860FE2E8AD6E2B1FF3A9334090EF9A8A66F2B9918`.
The release uses the lightweight `ver0.3.7` tag; MAP20 and older saves remain
intentionally unsupported.
