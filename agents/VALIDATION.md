# Validation Instructions

`AGENTS.md` is the repository source of truth. These rules govern builds,
probes, performance checks, GUI evidence, and final acceptance.

## 1. Evidence Integrity

- Never report PASS for a command or run that was not executed and checked in
  the current task context.
- Evidence from a prior source state is historical. Any product-code change
  invalidates affected build, probe, performance, GUI, and final-gate evidence.
- Preserve failed attempts in separate directories. Record commands, exit
  codes, logs, screenshots, fixture parameters, and failure reasons.
- Empty logs, unrelated reruns, copied screenshots, or missing generated-world
  state are not evidence.
- A focused probe proves only its contract. It never replaces required natural
  generated-world GUI or long-run evidence.
- Validation harness changes must be distinguished from product changes and
  revalidated against final product source.

## 2. Gate Selection

- Tiny isolated changes start with the smallest meaningful smoke test.
- Test breadth scales with blast radius, shared contracts, persistence changes,
  rendering impact, and user-visible workflows.
- Product changes affecting gameplay, simulation, world generation, rendering,
  UI, localization, diplomacy, war, plague, population, routes, or map display
  require executable game-flow regression before completion.
- Do not claim broad gameplay safety unless the complete required final flow was
  actually run and checked.
- Rule 39 is never an exploratory, debugging, or intermediate probe. It is the
  final acceptance gate only.

## 3. Build And Static Gates

For applicable final-source validation:

1. Build canonical `world_sim.exe` first with the repository's canonical make
   path.
2. Run `cmd /c build.bat` from the same final source.
3. Run `git diff --check`.
4. Run the repository text checks when visible/localized text is affected.
5. Scan dirty/touched text files for mojibake markers.
6. Audit all relevant `.c/.h` files for the 500-line maximum.
7. Confirm no `.c` includes another `.c`.
8. Confirm `Makefile` and `build.bat` source inventories match exactly.
9. Audit staged files; unexpected staged content is a failure.
10. Confirm the root executable inventory is exactly one `world_sim.exe`.

When text is affected, run `make check-text` or
`python tools/check_mojibake.py` and scan for confirmed mojibake markers,
including suspicious sequences beginning with code points `U+00C3`, `U+00C2`,
or the replacement character `U+FFFD`. Do not blindly re-encode files
containing valid UTF-8 Chinese.

If `world_sim.exe` is locked:

- Do not kill a process, delete the executable, or force-unlock it without
  explicit user approval.
- Report the lock.
- Build a clearly temporary target only when the approved build workflow
  supports it.
- Delete that temporary executable when verification ends.
- Leave exactly one root executable: `world_sim.exe`.

## 4. Focused Probes And Boundary Matrices

- Add or update deterministic probes for new model, save, presentation, input,
  cache, and lifecycle contracts.
- Test endpoints, default mappings, invalid inputs, transitions, persistence,
  reload behavior, stable identity, deduplication, and ordering when relevant.
- Use task-specific boundary matrices for map sizes, seeds, parameter extremes,
  languages, widths, states, LODs, and failure paths.
- A failed gate must lead to diagnosis, a product or harness correction as
  appropriate, and rerun from current source.
- Do not broaden capacities, relax acceptance thresholds, seed-shop, or modify
  product behavior merely to manufacture passing evidence.

## 5. Non-Disruptive GUI Validation

When the user may be using another foreground or fullscreen application:

- Do not force `world_sim.exe` to the foreground.
- Prefer a maximized game window on another monitor.
- Before every GUI launch, enumerate the current monitors and visible top-level
  windows. Record the primary monitor bounds and any non-owned foreground or
  full-screen window there, including its HWND, PID, process image when
  readable, window bounds, and monitor bounds. Use geometry and ownership for
  the safety decision; do not depend only on a guessed list of game names.
- Treat any non-owned visible top-level window that covers the primary monitor
  within a documented small border tolerance as a protected full-screen
  application. If one is present, place the owned validation window fully on a
  suitable secondary monitor with `SWP_NOACTIVATE`, then verify its final
  monitor and bounds before sending any interaction or capture message.
- Never move, resize, minimize, cover, activate, inspect through invasive
  process access, or otherwise manipulate the protected full-screen window.
  If no suitable secondary monitor is available, do not launch GUI validation
  and report the display-isolation limitation; never fall back to the primary
  monitor merely to complete the run.
- Continue checking owned-window placement during the run. If display topology
  changes or an unrelated protected full-screen window appears on the selected
  validation monitor, non-activatingly move only the owned window to another
  suitable secondary monitor, or close only the owned process normally and
  preserve the incomplete attempt when no isolated monitor remains.
- Use HWND-scoped, non-activating methods such as
  `SetWindowPos(..., SWP_NOACTIVATE)`, `PrintWindow`, and safe direct
  `PostMessage` calls.
- Do not use `SendInput`, `SetCursorPos`, real global mouse/keyboard input,
  Alt-Tab, `SetForegroundWindow`, activation, or page switching without the
  user's explicit approval for that disruption.
- Do not require the user to keep the physical cursor stationary or keep one
  unrelated foreground window unchanged as a precondition for HWND-scoped
  validation. Cursor movement and foreground changes among non-owned windows
  are diagnostic only; they must not reset a quiet timer or fail the run.
- Hard non-interference gates are limited to the owned game window becoming
  foreground or active without authorization, a message/capture/placement
  targeting a non-owned HWND, a placement operation omitting its non-activation
  flag, or any call to a forbidden global-input or activation API. Direct user
  interaction with the owned game window may invalidate the affected evidence.
- New or modified GUI helpers must route Win32 interaction through an audited,
  allowlisted safety layer that verifies HWND ownership and records every
  message, capture, and placement target. Direct helper-specific `user32`
  calls that bypass this ownership audit are not acceptable.
- Controls that use `SetCapture`, including divider or slider drags, require a
  bounded owned-HWND transaction with a deterministic final move/release and
  verified value, pressed/dragging state, and capture release. External pointer
  movement must not require desktop-wide inactivity. If it contaminates the
  postcondition, repeat only that UI transaction without regenerating the
  world, changing the seed, or reclassifying a product failure.
- Use deterministic presentation artifacts or source-bound renderer contracts
  for hover or pointer-dependent states that cannot be isolated from the
  physical cursor. Do not move the real cursor to manufacture those states.
- If HWND-scoped validation cannot finish, stop and report the limitation. Do
  not silently fall back to disruptive automation.
- Do not kill a pre-existing game process. If the executable is already
  running, report it and choose a non-destructive validation path.

GUI and performance validation must use a maximized window where the complete
Debug / Performance panel can be inspected. Resize or scroll the panel and
capture every relevant row. Cropped or hidden performance rows are not
acceptable.

## 6. Visual And Realtime Correctness

Inspect concrete affected states, not only click paths. Evidence must prove:

- No clipping, overlap, mojibake, stale hover/pressed/selected state, missing
  controls, square artifacts, or incoherent scaling in required languages and
  viewports.
- Generated-map political colors are current immediately after generation.
- Province fill, borders, city icons, city labels, routes, selection/highlight
  extents, fog, legends, and map modes remain correctly layered and realtime.
- Mode switches or user interaction are not required to refresh live state.
- A selected country's current territory agrees with visible political fill,
  borders, and cities.
- Static-cache optimizations do not freeze dynamic simulation layers.
- Required 60-frame sequences show no whole-screen, top-bar, toolbar, map, or
  panel flicker anomalies.

For UI work, follow the detailed state matrix in `agents/UI_UX.md` and retain
screenshots sufficient for human visual review. A textual checklist alone is
not enough.

## 7. Performance And Resource Validation

Performance, stutter, scheduler, render, and map-cache work must:

- Preserve simulation rules, outcomes, speed semantics, and map draw order.
- Identify any tile, region, city, route, civilization, or pair scans.
- Prove bounded cache/rebuild behavior with revision counters or equivalent
  diagnostics.
- Use matched before/after fixtures from trustworthy executables when claiming
  causal improvements.
- Report mean, median, P95, and peak where the task contract requires them.
- Check GDI objects, retained cache memory, private bytes, working set, and
  growth across repeated interaction cycles when applicable.
- Keep political/province/city/route/label/highlight layers current throughout
  long runs.

If the run hangs, crashes, becomes unusably slow, misses a required state, or
exceeds an approved gate, validation fails and implementation resumes unless
the user explicitly stops the task.

## 8. Rule 39 Full Game-Flow Regression

After any applicable gameplay, simulation, world-generation, rendering, UI,
localization, diplomacy, war, plague, population, route, or map-display change,
the final acceptance run must:

- Use an Extreme map at 1152x800.
- Randomize physical map parameters.
- Randomize advanced terrain preferences.
- Confirm at least 26 initially placed civilizations.
- Confirm more than 600 natural regions.
- Run at max/5x speed until at least five distinct civilizations reach
  technology stage 5.
- Verify deep-sea routes are hidden/unrevealed before unlock and
  visible/revealed after unlock.
- Inspect UI/UX, map display, labels, routes, borders, diplomacy, wars,
  vassals, collapse, plague, logs, and performance.
- Verify realtime province/city correctness throughout the long run.
- Capture generated and late-run maps showing current political fill, borders,
  city icons, city labels, routes, and selected/highlighted country extents.
- Capture 60-frame flicker evidence.

A 60-second bounded run is not completion evidence.

The final Rule 39 report must include:

- Final year and month.
- Natural region count.
- Initial and final confirmed civilization counts.
- Exact speed setting.
- Exact IDs, UIDs when available, names, and stages of the first five qualifying
  stage-5 civilizations.
- Deep-route total/shallow/deep counts and hidden-before/revealed-after proof.
- Province/city/label/route correctness evidence.
- Diplomacy, war, vassal, collapse, plague, logs, presentation-order, dropped
  months, and performance rows required by the task.
- Every required natural event or an explicitly identified evidence limitation
  that prevents acceptance.

If the run cannot reach five stage-5 civilizations, cannot prove route reveal,
shows stale one-province maps after long expansion, lacks cities, requires mode
switches to refresh, flickers, hangs, crashes, or lacks required rows/artifacts,
Rule 39 fails. Diagnose and fix actionable causes instead of reporting the task
as accepted.

## 9. Mandatory Final Order (Rule 47)

Complete this sequence in order:

1. Finish the approved implementation and freeze intended source scope.
2. Pass incremental compilation and relevant focused deterministic probes.
3. Pass task-specific boundary matrices and subsystem regression probes.
4. Pass canonical `make -B world_sim.exe`.
5. Pass `cmd /c build.bat` from the same final source.
6. Pass static, text/mojibake, line-count, `.c`-include, build-list parity,
   staged-file, and root-executable-inventory gates.
7. Pass matched performance, resource, memory, and cache checks when applicable.
8. Pass final targeted generated-world GUI validation for every required state,
   language, viewport, interaction, and flicker condition.
9. Review the final diff and confirm no known actionable defect, missing
   requirement, or unexplained regression remains.
10. Only then run one fresh full final-source Rule 39 as the last acceptance
    gate.

If Rule 39 fails, the task returns to implementation status. Preserve the
failed attempt, diagnose and fix it, rerun affected focused gates plus required
final builds, static checks, performance/resource checks, targeted GUI, and
final diff review, then run a completely fresh Rule 39. Never describe a failed
or exploratory attempt as the final run.
