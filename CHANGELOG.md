# Changelog

## 2.0.0 — Stable release

- Promoted five independently reviewed V2 Preview milestones into the
  stable Windows C11 protected-local-blob lifecycle library.
- Shipped NONE, CASUAL, and CURRENT_USER protection semantics, strict versioned
  envelope parsing, and precise validation boundaries.
- Shipped allocating memory APIs; reliable protected-file read/write with
  one validated backup, conservative fallback, explicit upgrade, and
  inspect/verify/upgrade maintenance CLI.
- Finalized real Windows CMake/CTest, exported installed library target
  `Obscura64::obscura64`, Windows CI, and the canonical V2 guide.
- Preserved released V1 public APIs, wire formats, frozen Profiles, and
  Provider ABI 1. Distributed under MIT.
- [Stable release notes](docs/RELEASE_NOTES_V2.0.0.md).

Historical Preview entries below record development milestones, not current
unfinished product features.


## 2.0.0-preview.5 — Hardening and V2 closure

- Added bounded Windows path alias handling: canonical existing parents and
  targets share sidecars, while target reparse points and multi-hard-link
  targets are rejected before mutation.
- Added real Windows CMake/CTest support, a static installed package with
  `Obscura64::obscura64`, external C/C++ package consumers, and Windows CI.
- Added deterministic parser mutation coverage and a consolidated V2 guide.
- Preserved V1 artifacts, V2 wire layouts, and existing public API.

## 2.0.0-preview.4 — Upgrade and tooling

- Added explicit, single-lock protected-file upgrade to selected V2 semantics,
  including builtin V1 Managed Payload, conservative backup source selection,
  and an untouched already-current no-op.
- Added `obscura64` maintenance CLI for exact-file inspect/verify and safe
  in-place upgrade with Unicode path support. Kept V1 `obscura64_recover`.
- Added focused upgrade/CLI tests. No wire format, Provider ABI, or frozen
  Profile Library changes; Preview 5 hardening remains deferred.

## 2.0.0-preview.3 — Reliable files

- Independent-review correction: separated nondestructive read fallback from
  destructive write eligibility. Ordinary writes now preserve inaccessible
  CurrentUser and ambiguous legacy bytes, including any existing backup.
- Added allocation-first protected-file write/read for NONE, CASUAL, and
  CURRENT_USER, with optional primary/backup and V2/V1 format classification.
- Added one adjacent validated backup, verified same-directory replacement,
  read fallback, and shared/exclusive nonblocking file coordination.
- Reused V1 persistence without changing frozen formats; extracted neutral
  UTF-8 path conversion and added a variable-length replacement entry point.
- No automatic read repair or V1 file upgrade; CLI tooling remains Preview 4.

## 2.0.0-preview.2 — Protection and legacy

- Added self-contained Casual obfuscation with corruption validation and
  Windows current-user DPAPI protection under the unchanged V2 envelope.
- Added automatic V2 protection dispatch and supported V1 builtin Managed
  Payload recognition, success classification, and narrow V1-to-V2 migration.
- Preserved the frozen Profile Library and released V1 APIs/formats.
- Independent-review correction: CurrentUser now validates a versioned inner
  record and digest after DPAPI succeeds; automatic V1 recognition filters
  candidates by the fixed Managed Payload prefix before full decoding.
  SHA-256 moved to a shared private helper without changing V1 or Casual bytes.

## 2.0.0-preview.1 — V2 foundation

- Added a canonical self-describing V2 byte envelope and ordinary
  `protect`/`unprotect` APIs with strict parsing and structural diagnostics.
- Corrected the Preview 1 operation model before tagging: allocation-first
  protection with explicit semantic selection; caller-buffer forms discover
  actual result size after the backend operation. No generic arithmetic-only
  size prediction API or implicit protection default.
- The Preview 1 representation is explicitly unprotected; Casual and DPAPI
  protection, V1 migration, and V2 file tooling are future preview work.
- Preserved V1 public entry points, wire formats, and frozen Profile artifacts.

## 1.0.0

Obscura64 1.0.0 source release.

- Windows C11 module with C/C++ public API, explicit binary lengths, caller-buffer
  and allocating operations, and configurable codec Providers.
- Managed Payload V1 for validated decoding, corruption/wrong-Profile detection,
  plus the separate Raw Codec API.
- Frozen 4096-Profile Library V1 with BCrypt runtime random selection.
- Stable project identity, explicit Force Reinitialize, latest-three history,
  verified persistence, and shared/exclusive process coordination.
- Per-user LocalAppData redundancy and validated automatic recovery; failed
  recovery does not silently create another identity.
- Standalone builtin Managed Payload disaster recovery CLI with exact unique
  Profile matching and safe create-new output.
- Public API examples and source-only release documentation.

The historical v1.0.0 release provides reversible obfuscation, not encryption
or attacker authentication.
