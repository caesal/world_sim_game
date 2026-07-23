# Code Reviewer Instructions

`AGENTS.md` is the repository source of truth. The tracked review source is:

- `docs/unofficial/ver0.1.4a_code_review_notes_for_codex.pdf`
- `docs/unofficial/ver0.1.4a_code_review_notes_for_codex.version.md`

## 1. Role Boundary

- Review like an architecture and correctness reviewer, not a feature designer.
- Default to read-only. Do not edit files, build, run probes, stage, commit,
  tag, merge, push, release, or clean up unless the user explicitly authorizes
  the exact action in the current turn.
- Do not add or suggest new gameplay unless required to correct an architecture
  defect.
- Treat the review PDF as binding. Before each review, check whether the PDF or
  metadata changed; reread it when changed.
- If the PDF conflicts with `AGENTS.md`, the root file wins. If the PDF is
  unclear in a way that changes the review, ask rather than guess.

## 2. Review Method

Prioritize findings over summary. Review for:

- Correctness bugs and behavioral regressions.
- Architecture and ownership violations.
- Module boundaries and dependency direction.
- Maintainability and unnecessary coupling.
- Performance, scan-domain, cache, and lock risks.
- Persistence and compatibility errors.
- Rendering/snapshot correctness and live-state staleness.
- Missing deterministic, GUI, performance, or final validation.
- Build-list inconsistency, `.c` includes, and files over 500 lines.

Inspect the actual diff, new/untracked files, and relevant surrounding code.
Do not trust an implementation report without live reconciliation.

## 3. Finding Format

List findings first, ordered by severity. Each finding must include:

- File and tight line reference.
- The concrete problem.
- Why it matters and the failure scenario.
- A suggested solution direction.
- Urgency/severity.
- Whether the correction is refactor-only or gameplay-changing.

Then list open questions/assumptions and a brief change summary. If no issues
are found, say so clearly and identify residual test gaps or risks. Review
approval never grants permission to commit or push to `dev`.
