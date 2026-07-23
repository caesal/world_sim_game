# Integration And Release Instructions

`AGENTS.md` is the repository source of truth. These rules apply to staging,
committing, tagging, merging, pushing, versioning, releasing, and worktree
cleanup.

## 1. Authorization Is Required Per Operation

- Default to read-only release preparation.
- No stage, commit, tag, merge, push, release, worktree removal, or cleanup is
  authorized merely because implementation or validation passed.
- No agent may commit to or push `dev` unless the user explicitly authorizes
  that exact operation in the agent's current turn.
- Prior-turn authorization, an Architect prompt, PASS, acceptance, a requested
  version name, or permission to use a worktree is not integration permission.
- Authorization for one operation does not imply another. For example, commit
  permission does not imply push, tag, merge, release, or cleanup permission.
- If authorization wording or target is ambiguous, stop before the operation
  and ask the user.

## 2. Live Preflight

Before any authorized integration or release operation:

1. Read all routed repository instructions.
2. Run the Windows prelude.
3. Verify primary and task-worktree branch, HEAD, tags, divergence, status,
   process, root executable, and `git worktree list --porcelain`.
4. Inspect every modified, added, deleted, untracked, and staged file.
5. Confirm the exact validated source state and evidence correspond to the
   intended integration state.
6. Confirm the user-authorized target branch, remote, version, tag, and
   operation list.

Do not reset, clean, restore, or discard files to manufacture a clean release.

## 3. Version And Documentation Checklist

For an authorized version release, update and verify as applicable:

- Active source version marker.
- Root `README.md`.
- `docs/README.md`.
- `docs/unofficial/version_log.md`.
- Version-specific side document.
- Requested official docs and change summaries.
- Save-version metadata and compatibility wording when affected.

If an item does not apply, report why. All release metadata/docs changes must be
included in the validated release source before commit/tag/push.

## 4. Staging And Commit Audit

Only after explicit authorization:

- Stage intentionally named files. Avoid broad staging that can sweep in
  unrelated dirty or generated artifacts.
- Inspect `git status --short` and `git diff --cached --stat` before commit.
- Ensure all intended source, data, build-list, documentation, version, and
  compatibility files are staged together.
- Ensure unrelated files are not staged.
- Use the exact approved commit target and message or a clearly scoped message
  consistent with the user's request.
- Verify the resulting commit hash and contents.

## 5. Tag, Merge, Push, And Release Verification

Perform only the individually authorized operations.

- Verify a tag points at the final intended release commit after all metadata
  and documentation fixes.
- If a later release-metadata commit changes the intended release tip, move and
  push the tag only with explicit authorization, or report that it remains on
  the earlier commit.
- After an authorized merge, verify the target branch and resulting tree.
- After an authorized push, verify the command succeeded, the remote reference
  points to the intended commit, branch divergence is correct, and the latest
  log matches the release.
- Do not claim the worktree is clean if intentionally excluded artifacts remain.
- Do not remove the task worktree until integration is verified and the user
  separately authorizes cleanup.

When the user asks whether something was pushed or released, never answer from
memory. Inspect branch, commit, tag, and remote state live, then report exact
status.

## 6. Final Report

Report:

- User authorization received for each operation.
- Initial and final branch/HEAD/tag/remote/worktree state.
- Exact staged and committed file scope.
- Commit hash, tag target, merge result, and push result for operations actually
  performed.
- Version/documentation checklist results.
- Validation evidence bound to the released source.
- Remaining dirty/untracked files and surviving worktrees.
- Every integration, publication, and cleanup operation explicitly not
  performed.
