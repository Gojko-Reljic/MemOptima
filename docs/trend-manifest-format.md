# Trend Manifest Format

## Purpose

A trend manifest explicitly selects local evidence artifacts for
`memoptima trends analyze <MANIFEST>`. It is one versioned JSON document, not JSON Lines. It
selects evidence and does not assert a logical workload identity or inject analytical metadata.

## Manifest version 1

```json
{
  "manifest_version": 1,
  "artifacts": [
    { "path": "evidence/session-001.jsonl" },
    { "path": "evidence/session-002.jsonl" }
  ]
}
```

The root object has exactly two fields:

| Field | Type | Rules |
|---|---|---|
| `manifest_version` | unsigned integer | Must equal `1`. |
| `artifacts` | array | Contains from 1 through 128 artifact objects. |

Each artifact object has exactly one field:

| Field | Type | Rules |
|---|---|---|
| `path` | string | A non-empty relative path to one local evidence artifact. |

Unknown fields are validation errors. A path cannot be an array, object, number, boolean, or `null`.

## Path rules

Paths are interpreted relative to the manifest directory. The manifest directory path itself must
not contain a symbolic-link component. An artifact path cannot be absolute, contain a NUL byte, or
escape the manifest directory tree through parent traversal or lexical normalization. The same
normalized relative path may occur only once; duplicates are validation errors.

During analysis, MemOptima opens the manifest directory and traverses every artifact path relative
to that directory through file descriptors. It applies `O_NOFOLLOW` to every path component, so an
artifact path cannot contain a symbolic link, even when that link points back inside the manifest
directory. This descriptor-relative traversal also rejects a directory or file replaced with a link
after manifest validation. The final artifact must be a regular file accepted by MemOptima's bounded
evidence input reader.

## Processing rules

The manifest selects artifacts but does not define their analytical order. MemOptima validates all
selected artifacts, then sorts them by validated session start time. Equal start times use a stable
normalized-path order.

If one selected artifact is unavailable, unsafe, malformed, schema-invalid, or lifecycle-invalid, the
whole command fails closed. No partial report is emitted.

Selected sessions cannot overlap for the same full Docker container ID and cgroup path. An overlap is
ambiguous input and rejects the manifest.

## Compatibility and privacy

The manifest does not change the evidence artifact schema. It consumes schema-version-1 artifacts
through the existing safe validation path. Artifact paths can reveal local directory names, so the
manifest and its evidence directory require the same access controls.
