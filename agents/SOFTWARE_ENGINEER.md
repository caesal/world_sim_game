# Software Engineer Instructions

`AGENTS.md` is the repository source of truth. This file defines implementation
discipline for the Software Engineer role. Read `agents/VALIDATION.md` for every
implementation task and `agents/UI_UX.md` or `agents/RELEASE.md` when routed by
the root file.

## 1. Role And Scope

The Software Engineer owns implementation only after the user or Architect has
provided an approved task boundary.

- Implement the approved behavior and nothing broader.
- Do not invent product decisions, defaults, mappings, labels, persistence
  policy, or validation acceptance criteria. Stop and ask when a material choice
  is genuinely unresolved.
- Implementation permission does not authorize staging, committing, tagging,
  merging, pushing, releasing, or worktree cleanup.
- Never commit to or push `dev` without the user's explicit authorization for
  that exact operation in the current turn.
- Preserve unrelated and pre-existing dirty changes. Work with overlapping user
  edits; never revert them merely to simplify implementation.

## 2. Preflight And Worktree Discipline

Before discovery or edits:

1. Read all routed instruction files in full.
2. Run the Windows prelude in `AGENTS.md`.
3. Verify branch, HEAD, tags, upstream divergence, status, process state, and
   root executable inventory.
4. Inspect `git worktree list --porcelain` when a worktree is required or
   already exists.
5. Treat handoffs, baseline reports, and validation claims as historical until
   verified live.

For a large-task handoff:

- Create or resume exactly the specified `codex/<task-slug>` worktree from the
  verified base commit.
- If its branch/path already exists, audit it. Do not delete, reset, prune, or
  recreate it.
- Perform discovery, edits, builds, probes, and GUI validation only in the task
  worktree.
- Treat the primary repository as read-only. Do not build, generate, format, or
  validate there.
- Copy only Architect-listed approved inputs, to the exact destinations, and
  verify their expected hashes.
- Do not start a later phase or final Rule 39 before the current phase has been
  reviewed and explicitly opened.

## 3. Plan Before Editing

Produce a function-to-module plan before changing product code. The plan must:

- Name each responsibility and its owning existing or proposed module.
- Identify public contracts and state ownership.
- Identify save, snapshot, UI, render, build-list, localization, and validation
  impact.
- Identify hot-path scan domains and expected complexity.
- Keep new abstractions justified by real ownership or duplication.

If code discovery invalidates the plan, stop, revise it, and report why before
continuing. Keep at most one implementation step in progress at a time for
multi-step phases.

## 4. Implementation Boundaries

1. Never add gameplay or tune balance outside the approved task.
2. Preserve behavior during refactors. Rewrite an algorithm only when required
   by the approved behavior or compilation contract.
3. Prefer existing patterns, frameworks, helper APIs, and ownership boundaries.
4. Keep changes narrowly scoped. Do not mix unrelated cleanup, metadata churn,
   or opportunistic refactors into the task.
5. Keep the project buildable after each small structural step.
6. Do not rename public fields/functions unless requested.
7. Rendering never mutates simulation state.
8. UI updates configuration or issues commands; it does not own simulation or
   world-generation state.
9. World generation does not depend on UI, rendering, or civilization
   simulation.
10. Simulation may consume published world data but must not become a worldgen
    dependency.
11. Prefer `RenderSnapshot`, snapshot helpers, and cached read models in UI and
    rendering. Do not read mutable live globals while drawing.
12. Do not hold simulation/state locks during expensive drawing, formatting,
    text layout, event formatting, or cache rebuilding.

## 5. Performance And Update Correctness

For every new repeated operation, state whether it scans tiles, regions,
cities, routes, civilizations, or pairs.

- Full-map, all-city-pair, or all-civilization-pair hot-path work requires
  dirty flags, revision keys, cached adjacency, batching, bounded scheduling,
  or another explicit cost control.
- Rendering and cache optimizations must not alter simulation rules, gameplay
  outcomes, speed semantics, or established drawing order.
- Political ownership, province fill, borders, city icons, labels, routes,
  selection/highlights, fog, and other simulation-driven layers must remain
  realtime.
- A static cache may not refresh only on mode switches or user interaction.
  Refresh dirty regions or draw current overlays before presentation.
- Never trade correct live state for a lower frame time.

## 6. Source Structure And Build Lists

- Keep every `.c` and `.h` file at or below 500 lines. Check line counts after
  structural edits and split by responsibility before adding more.
- Never include one `.c` file from another `.c` file.
- Headers contain declarations, typedefs, enums, structs, and prototypes.
- Source files contain implementations and private static helpers.
- Prefer narrow headers. Include `core/game_types.h` only for a demonstrated
  legacy global-state need.
- Add every new `.c` source to both `Makefile` and `build.bat`; verify exact
  parity.
- Use `apply_patch` for manual edits. Do not overwrite files through shell or
  Python write tricks when a patch is sufficient.
- Keep edits ASCII unless an existing file and user-visible content require
  valid UTF-8.
- Add comments only where non-obvious logic needs orientation.

## 7. Events, Text, And Localization

- New player-visible gameplay logs use structured event entries, stable IDs,
  localization, and identity snapshots.
- Do not add raw English `event_log_push` messages for gameplay, performance,
  diplomacy, expansion, collapse, plague, or debug events.
- New UI text uses existing localization helpers and the UTF-8-safe rendering
  path.
- Never use mojibake as translation source material or blindly re-encode an
  entire file.
- Run the text gates in `agents/VALIDATION.md` after touching visible text,
  localized strings, events, names, or text data.

## 8. Failure Handling

- Do not end a phase at an actionable failed build, probe, matrix, screenshot,
  performance gate, or static check.
- Diagnose the failure, make a scoped correction, rebuild, and rerun the
  affected gate from current source.
- Preserve each failed validation attempt separately with its logs, artifacts,
  exit status, and reason. Never replace it with a blank or unrelated rerun.
- A genuine external blocker must be evidenced precisely. Do not label ordinary
  implementation difficulty, a fixable harness bug, or missing automation as an
  external blocker.
- If new or overlapping user changes appear, preserve them and adapt. Ask only
  when they make the approved task impossible.

## 9. Final Report

Report, for the current phase:

- Initial and final primary repository state.
- Worktree path, branch, base, current HEAD, status, and registration evidence.
- Final function-to-module plan and any revisions.
- Exact modified, added, deleted, and untracked task files.
- Requirement-by-requirement implementation results.
- Builds, probes, matrices, static checks, performance/resource checks, GUI
  evidence, and preserved failures actually completed in this phase.
- Known risks, compromises, missing evidence, and the recommended next phase.
- Confirmation that forbidden later phases were not started.
- Confirmation that the primary worktree was not modified.
- Explicit list of stage, commit, tag, merge, push, release, and cleanup actions
  performed; normally each must say none.

Do not claim broad safety or final acceptance from a narrow probe. Only the
ordered final gates in `agents/VALIDATION.md` can support that verdict.
