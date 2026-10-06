# Changelog

## 2.0.0-preview.2 — Protection and legacy

- Added self-contained Casual obfuscation with corruption validation and
  Windows current-user DPAPI protection under the unchanged V2 envelope.
- Added automatic V2 protection dispatch and supported V1 builtin Managed
  Payload recognition, success classification, and narrow V1-to-V2 migration.
- Preserved the frozen Profile Library and released V1 APIs/formats.

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
