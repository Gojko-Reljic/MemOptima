# MemOptima Agent Rules

## Scope and authority

- These rules apply to every automated or human-assisted change in this repository.
- The project owner has final approval authority for architecture, scope, commits, merges, and
  publication.
- The Architecture responsibility proposes and records technical decisions; it does not replace
  owner approval.

## Language

- Keep repository content in English, including source code, comments, documentation, tests, user
  messages, diagnostics, and commit messages.

## Architecture and dependencies

- Preserve existing behavior unless an owner-approved decision explicitly changes it.
- A module may depend only on the public interfaces of lower-level modules. It must not depend on
  their internal implementation details.
- Define and approve module interfaces before parallel implementation begins.
- Core application code remains C++23 unless the owner approves an exception.

## Change control

- Any change to the evidence artifact format, CLI output contract, public API, or module boundary
  requires an entry in `DECISIONS.md` and explicit owner approval before implementation.
- Do not begin parallel implementation until the architecture and development plan are approved.
- Use isolated branches or Git worktrees for independent parallel changes.
- Do not merge, push, or publish changes without explicit owner approval.

## Quality

- Every feature requires proportionate unit and integration tests.
- Preserve formatting, sanitizer, and Release memory-safety checks.
- Treat diagnostics conservatively: do not claim an OOM prediction, confirmed memory leak, or safe
  automatic action without evidence that supports that claim.

## Review

- Every GitHub pull request requires an independent review and owner approval before merging.
- CodeRabbit review becomes mandatory when the owner connects and explicitly enables it for this
  repository.
