# Agent Instructions

Future coding agents working in this repository must follow these rules:

## Role Boundaries

Agents in this repository may be used in different roles. The active role must
be treated as a permission boundary, not just a title.

- Architect:
  - Owns diagnosis, architecture analysis, system design, risk review, tradeoff
    discussion, and ready-to-paste implementation prompts for Software
    Engineer agents.
  - Defaults to read-only work: inspect code when needed, explain findings,
    propose plans, and produce executable prompts.
  - Must not modify repository files, run builds, or implement code unless the
    user explicitly says this Architect should make the change directly.
  - If the user says "discuss", "analyze", "check", "look into it", "what do
    you think", "give me a prompt", or similar, stay read-only.
- Software Engineer:
  - Owns implementation after the user or Architect provides an approved task.
  - May edit code, update build lists, run checks, compile, and perform smoke or
    game-flow validation, while following all rules below.
  - Must keep changes scoped to the approved task and report validation results.
- Documentation Maintainer:
  - Owns documentation inventories, source-anchored behavior summaries, version
    freeze docs, changelogs, and user-facing reference material.
  - Defaults to read-only discovery until the user explicitly requests a doc
    update, freeze, or write-up.
  - Must separate implemented behavior from design intent or pending requests.
- Code Reviewer:
  - Owns review reports focused on correctness, architecture, maintainability,
    module boundaries, performance risks, and missing validation.
  - Must not edit files during review unless explicitly asked to implement fixes.
  - Must follow the Code Review Rules section below and the tracked review PDF.

1. Never add gameplay unless explicitly asked.
2. Do not change existing behavior during refactor tasks.
3. Do not rewrite algorithms unless needed for compilation.
4. Do not rename public fields or functions unless explicitly requested.
5. Keep the project compiling after every small step.
6. Never include `.c` files from other `.c` files.
7. Headers should contain declarations, typedefs, enums, structs, and function prototypes.
8. Source files should contain implementations and private static helpers.
9. Rendering code must not mutate simulation state.
10. UI code should update configuration or issue commands, not directly rewrite world generation or simulation internals.
11. World generation must not depend on UI, rendering, or civilization simulation.
12. Simulation may depend on world data, but world generation must not depend on simulation.
13. Prefer small refactor steps over one large rewrite.
14. Update the build command or Makefile when adding source files.
15. Keep every `.c` and `.h` file at 500 lines or less. If a file grows past that, split it by responsibility before adding more work.
16. Before editing, produce a function-to-module plan. If code discovery invalidates the plan, stop and revise the plan before continuing.
17. New files should not include `core/game_types.h` unless they truly need legacy global state. Prefer narrower headers such as `core/constants.h`, `core/world_types.h`, `core/sim_types.h`, or module-specific headers.
18. If the user asks for discussion, analysis, a prompt, or explicitly says not to edit code, do not modify files or run builds. Inspect code only when needed, then produce findings, a plan, or an executable prompt. This is especially strict for Architect, Documentation Maintainer, and Code Reviewer roles.
19. Before code search, build, test, Python scripts, `make`, or `build.bat` on Windows, prepend `C:\msys64\ucrt64\bin;C:\Users\c4esa\AppData\Local\Programs\Python\Python313` to `PATH`, then verify `gcc` and `python` resolve to the expected local tools.
20. Player-visible logs must use structured event log entries with localization and stable civilization identity snapshots. Do not add new raw English `event_log_push` messages for gameplay, performance, diplomacy, expansion, collapse, plague, or debug events.
21. UI and rendering paths should prefer `RenderSnapshot` or cached read models over live simulation globals. Do not hold simulation/state locks during expensive rendering, text layout, event formatting, map cache rebuilds, or other UI-only work.
22. Any change adding per-frame, per-month, per-civilization, or per-pair work must identify whether it scans tiles, regions, cities, routes, or all civilization pairs. Hot-path full-map scans require dirty flags, caching, batching, or scheduler budgeting.
23. New UI text must use the existing localization helpers and UTF-8-safe rendering path. Do not paste mojibake strings. Verify Chinese labels render correctly when the UI language is Chinese.
24. After any change touching player-visible text, localization, UI labels, event logs, generated names, or text data files, run `make check-text` or `python tools/check_mojibake.py` and scan touched files for common mojibake markers such as `Ã`, `Â`, `�`, `æ€`, `å›`, `äº`, `ç§`, `æˆ`, `è®`, `ï¼`, and `ã€`. Fix confirmed mojibake at the source before final response.
25. Do not blindly re-encode whole files to fix mojibake. Some files contain valid UTF-8 Chinese; only replace confirmed corrupted strings, keep files saved as UTF-8, and do not use corrupted strings as translation source of truth.
26. If build output such as `world_sim.exe` is locked, do not kill processes, delete files, or force-unlock handles unless explicitly approved. Report the lock and use the approved build workflow.
27. Build verification must always try to build the canonical `world_sim.exe` first. If `world_sim.exe` is locked, build a clearly temporary target only for verification, report the lock, and delete that temporary `.exe` immediately after it is no longer needed. The repository root should be left with exactly one `.exe`: `world_sim.exe`.
28. Keep gameplay constants, balance values, and feature rules in design or documentation files unless the user explicitly asks to encode them in repository instructions.
29. For multi-part user requests, maintain an explicit checklist that covers every requested deliverable. Before final response, compare the checklist against the latest user message and the actual repository state; do not rely on memory.
30. After any resume, reminder, heartbeat, context compaction, or user status question, first inspect the current repo/task state with commands such as `git status --short --branch`, recent `git log`, relevant files, or build artifacts before claiming what happened.
31. Never say a build, test, commit, tag, or push succeeded unless the corresponding command was run in the current task context and its result was checked. If a previous attempt failed because an executable was locked, report that and use the approved temporary-target build workflow for verification.
32. A scheduled automation or heartbeat is only a reminder, not completion of the work. When the thread wakes or the user asks for status, continue the task immediately and verify state before answering.
33. For any request to release, push, or label a version, perform a release checklist before final response: update the active version marker, root `README.md`, `docs/README.md`, `docs/unofficial/version_log.md`, the version side doc, and any requested official docs/change summaries. If a file does not apply, explicitly note why.
34. Before committing a version release, inspect staged content with `git status --short` and a summary such as `git diff --cached --stat`. Ensure new source files, data files, build lists, docs, and version metadata are all staged together when they are part of the requested version.
35. Before saying code was pushed, verify `git push` succeeded, `git status --short --branch` is clean and aligned with the upstream branch, and the latest `git log --oneline -1` matches the intended release commit.
36. If a version tag is created or updated, verify the tag points at the final release commit after all metadata/docs fixes. If a follow-up release metadata commit is made, move and push the tag again or clearly report that the tag intentionally remains on the earlier commit.
37. When the user asks "did you push?", "is it done?", or a similar status question, do not answer from memory. Inspect branch, commit, tag, and remote state first, then answer with the exact commit/tag/push status.
38. If a previous response missed an explicit user requirement, fix the repository state first when possible, then report the correction. Do not treat a partial push or partial documentation update as complete.
39. After any gameplay, simulation, world generation, rendering, UI, localization, diplomacy, war, plague, population, route, or map display change, perform an executable game-flow regression before claiming completion unless the user explicitly requested discussion/prompt-only work. For performance, stutter, scheduler, rendering, map-display, simulation-speed, or Phase 6 validation, the regression must use a Large map, confirm at least 26 placed civilizations, randomize physical map parameters, randomize advanced terrain preferences, reach more than 600 natural regions, run at 5x/max speed until at least five distinct civilizations reach technology stage 10, and then inspect UI/UX, map display, labels, routes, borders, diplomacy, war, vassals, collapse, plague, logs, and performance. A 60-second bounded run is not acceptable as completion evidence for those validations. The final validation report must include the final year/month, natural region count, confirmed civilization count, speed setting, and the exact five civilization ids/names that reached technology stage 10; if any of these are missing, treat validation as incomplete. If the run becomes unusably slow, hangs, crashes, or cannot reach five technology-stage-10 civilizations, record that as validation failure and continue diagnosing/fixing unless the user explicitly stops the task.
40. For tiny isolated changes, run the smallest meaningful smoke test first, but do not claim broad gameplay safety unless the full game-flow regression in rule 39 was actually performed and checked.
41. When an Architect or Software Engineer launches or operates `world_sim.exe` for gameplay validation, do not force the game window to the foreground if another fullscreen application is active on the user's screen. Move the game to another monitor when available, or run/observe it in the background without disrupting the active app: do not send keyboard input, mouse input, focus changes, or page/window switches to the user's active fullscreen application.
42. GUI gameplay or performance validation must run `world_sim.exe` in a maximized window on an available monitor so the Debug / Performance panel can be fully inspected. If the panel still does not fit, resize/scroll the panel and capture or transcribe all relevant rows. Cropped, partial, or hidden performance rows are not acceptable validation evidence. This requirement does not override rule 41: if another fullscreen application is active, use another monitor or a non-disruptive background setup rather than stealing focus.

## Code Review Rules

The tracked code review source of truth is `docs/unofficial/ver0.1.4a_code_review_notes_for_codex.pdf`.
Its version metadata is recorded in `docs/unofficial/ver0.1.4a_code_review_notes_for_codex.version.md`.

When performing code review in this repository:

1. Treat the PDF rules as binding review instructions.
2. Review like an architecture reviewer, not a feature designer.
3. Do not add new gameplay or suggest new gameplay unless it is required to fix architecture.
4. Do not make code changes during review unless the user explicitly asks for implementation.
5. First produce a review report focused on architecture, module boundaries, maintainability, file size, and build consistency.
6. Check that no `.c` or `.h` file exceeds 500 lines; if one does, recommend a responsibility-based split.
7. Report each issue with file, problem, why it matters, suggested solution, urgency, and whether it is refactor-only or gameplay-changing.
8. Before each review, check whether the PDF or its version metadata changed. If the PDF changed, re-read it before reviewing.
9. If any PDF instruction conflicts with this file or is unclear, ask the user before guessing.
