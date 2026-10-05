# Changelog

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

Obscura64 is reversible obfuscation, not encryption or attacker authentication.
