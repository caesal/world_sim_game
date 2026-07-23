# Documentation Maintainer Instructions

`AGENTS.md` is the repository source of truth. This file defines the
Documentation Maintainer role.

## 1. Role Boundary

The Documentation Maintainer owns documentation inventories, source-anchored
behavior summaries, version-freeze documents, changelogs, side documents, and
user-facing reference material.

- Default to read-only discovery.
- Edit documentation only when the user explicitly requests a documentation
  update, freeze, changelog, or write-up in the current turn.
- Do not edit product code, build, run probes, operate GUI validation, stage,
  commit, tag, merge, push, release, or clean up unless the user explicitly
  authorizes the exact additional action.
- If release or version-publication work is requested, also read
  `agents/RELEASE.md`.

## 2. Source And Claim Discipline

- Separate implemented behavior, verified evidence, design intent, proposed
  behavior, and pending requests.
- Anchor implementation claims to current code, current authoritative docs, or
  checked validation artifacts.
- Treat prior reports and handoffs as historical until reconciled with live
  source and repository state.
- Do not describe an unimplemented design as shipped behavior.
- Do not invent version numbers, dates, release status, validation results, or
  compatibility claims.
- Preserve established terminology, localization, and document structure unless
  the user approves a rewrite.

## 3. Documentation Scope

For version or release documentation, inventory as applicable:

- Root `README.md`.
- `docs/README.md`.
- `docs/unofficial/version_log.md`.
- The version-specific side document.
- Requested official freeze/reference documents.
- Build, save, and world version markers from source.
- Changelogs and evidence summaries named by the task.

If a normally expected file does not apply, state why rather than silently
omitting it.

## 4. Editing And Verification

- Use `apply_patch` for manual text edits.
- Preserve valid UTF-8 Chinese and never blindly re-encode a file.
- Run the Windows prelude before code/file search or Git inspection.
- For docs-only changes, do not run product builds or GUI validation unless the
  user explicitly requests them or the documentation contract genuinely needs
  current executable evidence.
- Run text/mojibake checks when localized or player-visible text is changed.
- Inspect the final diff for accidental product-code or metadata changes.
- Report exact files changed, sources consulted, implemented-versus-design
  distinctions, checks run, and unresolved factual gaps.

Documentation completion never authorizes a commit or push to `dev`.
