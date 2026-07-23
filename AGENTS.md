# Agent Instructions

This repository-root file is the mandatory entry point for every agent. It is
the highest-priority repository instruction file. Role and shared instruction
files under `agents/` supplement this file and may make a rule stricter, but
they may never weaken, bypass, or contradict it.

## 1. Required Reading And Role Routing

Before inspection, planning, editing, building, validation, review, release, or
prompt generation, identify the active role and read the required files in
full. If a required file is absent or unreadable, stop and report that fact.

| Active role | Required repository instructions |
|---|---|
| Every role | `AGENTS.md` |
| Architect | `agents/ARCHITECT.md` |
| Software Engineer | `agents/SOFTWARE_ENGINEER.md`, `agents/VALIDATION.md` |
| Documentation Maintainer | `agents/DOCUMENTATION_MAINTAINER.md` |
| Code Reviewer | `agents/CODE_REVIEWER.md`, `agents/VALIDATION.md` |

Additional mandatory routing:

- UI, rendering, map presentation, localization, or interaction work must also
  read `agents/UI_UX.md`.
- Any request involving versioning, staging, committing, tagging, merging,
  pushing, releasing, or worktree cleanup must also read `agents/RELEASE.md`.
- Before preparing or reviewing a Software Engineer handoff, Architects must
  also read `agents/SOFTWARE_ENGINEER.md`, `agents/VALIDATION.md`, and every
  other shared file that the downstream prompt invokes.
- Software Engineers must read the current worktree's own `AGENTS.md` and all
  routed files before changing that worktree. A newer handoff policy supplied
  from the primary repository remains binding when the selected baseline
  predates it.
- If the requested role is unclear, remain read-only and ask the user rather
  than choosing broader permissions.

## 2. Permission Boundaries And Git Prohibitions

The active role is a permission boundary, not a title.

- Architect, Documentation Maintainer, and Code Reviewer default to read-only.
  They may edit only when the user explicitly authorizes that role to edit in
  the current turn.
- Discussion, analysis, diagnosis, inspection, review, planning, or prompt
  requests are read-only. Do not edit files, build, run probes, operate the GUI,
  commit, tag, push, merge, release, or clean up in those turns.
- Software Engineer implementation permission covers only the approved task
  scope. It does not imply Git publication or integration permission.
- By default, no agent may stage, commit, tag, merge, push, release, delete a
  worktree, or clean/reset/restore/discard repository state.
- **No agent may commit to or push `dev` unless the user explicitly authorizes
  that exact operation in the agent's current turn. This rule is absolute.**
- PASS results, design approval, implementation approval, worktree creation,
  validation completion, an Architect handoff, a prior-turn authorization, or
  a version name never imply permission to commit or push to `dev`.
- A `codex/<task-slug>` worktree branch also defaults to no staging, commit,
  tag, push, merge, release, or cleanup. Each operation requires explicit user
  authorization for that operation and target in the current turn.
- Never use destructive Git or filesystem operations to simplify recovery.
  Do not reset, clean, checkout, restore, overwrite, prune, or discard surviving
  work unless the user explicitly authorizes the exact destructive action.
- Do not kill, force-close, move, activate, or manipulate a user process merely
  to unblock work. Follow the locked-executable and non-disruptive GUI rules in
  `agents/VALIDATION.md`.

## 3. Windows Toolchain Prelude

Before Git inspection, code or file search, Python, build, test, probe, or GUI
validation on Windows, run:

```powershell
$env:PATH='C:\msys64\ucrt64\bin;C:\Users\c4esa\AppData\Local\Programs\Python\Python313;' + $env:PATH
Get-Command gcc
Get-Command python
```

Use the verified tools for the rest of that shell session. Do not claim the
prelude passed unless the commands were run and checked in the current task.

## 4. Universal Product And Architecture Boundaries

1. Never add or tune gameplay unless explicitly asked.
2. Refactors must preserve existing behavior unless the approved task says
   otherwise. Do not rewrite algorithms merely for style.
3. Do not rename public fields or functions unless explicitly requested.
4. Rendering must not mutate simulation state.
5. UI code may update configuration or issue commands; it must not directly
   rewrite world-generation or simulation internals.
6. World generation must not depend on UI, rendering, or civilization
   simulation. Simulation may depend on published world data.
7. UI and rendering should use `RenderSnapshot` or cached read models instead
   of live simulation globals. Do not hold simulation/state locks during
   expensive rendering, formatting, text layout, or cache rebuilds.
8. Player-visible gameplay events must use structured localized event entries
   with stable identity snapshots. Do not add raw English gameplay log pushes.
9. Keep every `.c` and `.h` file at 500 lines or less. Split by responsibility
   before adding work that would exceed the limit.
10. Never include a `.c` file from another `.c` file. Headers contain
    declarations; source files contain implementations and private helpers.
11. Prefer narrow headers. New files must not include `core/game_types.h`
    unless they genuinely require legacy global state.
12. Add every new source file to both `Makefile` and `build.bat` and keep the
    source inventories identical.
13. Before editing product code, write a function-to-module plan. If discovery
    invalidates it, stop and revise the plan before continuing.
14. Any new per-frame, per-month, per-civilization, per-route, per-city, or
    per-pair work must identify its scan domain. Full-map or all-pair hot-path
    work requires caching, dirty flags, batching, or scheduler budgeting.
15. Keep gameplay constants and feature rules in design/task documentation
    unless the user explicitly asks to encode them as repository policy.

## 5. State, Recovery, And Reporting Integrity

- Treat handoffs and prior reports as historical until verified live.
- After a resume, reboot, interruption, compaction, heartbeat, or status
  question, inspect the current branch, HEAD, tags, worktree status, relevant
  files/artifacts, process state, and executable inventory before making claims.
- Preserve dirty worktrees and failed validation artifacts. Create new evidence
  attempts rather than replacing failures with blank logs or unrelated reruns.
- Never say a build, probe, test, GUI run, commit, tag, merge, push, or release
  succeeded unless that exact operation was run and checked in the current task
  context.
- A reminder or automation wake-up is not completion evidence.
- Multi-part work requires an explicit checklist. Before the final response,
  compare the latest user request with the actual repository state.
- If an explicit requirement was missed, correct the state when authorized and
  possible; otherwise report the precise gap. Do not call partial work complete.
- Validation requirements, evidence order, and the final Rule 39 gate live in
  `agents/VALIDATION.md` and remain mandatory whenever routed.
