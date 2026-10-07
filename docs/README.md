# Obscura64 Documentation

Design, stable file formats, persistence, recovery, release information, and
compatibility notes for Obscura64. V2 Casual is reversible obfuscation;
V2 CurrentUser delegates protection to Windows DPAPI.

## Start here

- [V2 Preview 2 protection and legacy compatibility](V2_PREVIEW2.md)
- [V2 Preview 3 reliable files](V2_PREVIEW3.md)
- [V2 Preview 1](V2_PREVIEW1.md)
- [Project README](../README.md)
- [V1 specification](OBSCURA64_V1_SPEC.md)
- [Managed Payload V1](MANAGED_PAYLOAD_V1.md)
- [Recovery V1](RECOVERY_V1.md)

## Stable formats and identities

- [Profile Library V1](PROFILE_LIBRARY_V1.md)
- [Project State V1](PROJECT_STATE_V1.md)
- [Project History V1](PROJECT_HISTORY_V1.md)
- [Managed Payload V1](MANAGED_PAYLOAD_V1.md)

## Recovery and persistence

- [Recovery policy and limits](RECOVERY_V1.md)
- [Current state and verified persistence](PROJECT_STATE_V1.md)
- [History retention and consistency](PROJECT_HISTORY_V1.md)

## Release information

- [Changelog](../CHANGELOG.md)
- [v1.0.0 release notes](RELEASE_NOTES_V1.0.0.md)
- [V1 release checklist and validation evidence](RELEASE_CHECKLIST_V1.md)
- [Published v1.0.0 Release](https://github.com/RXY712200/Obscura64/releases/tag/v1.0.0)

## Project and attribution

- [Contributing](../CONTRIBUTING.md)
- [Support](../SUPPORT.md)
- [Security policy](../SECURITY.md)
- [Third-party references](THIRD_PARTY_REFERENCES.md)
- [MIT License](../LICENSE)

## Examples

- [Managed Payload quick start](../examples/managed_quickstart.c)
- [Raw Codec example](../examples/raw_codec.c)

## Project status

[Issue #1 — Development Log, Known Issues & Roadmap](https://github.com/RXY712200/Obscura64/issues/1)
tracks the current state, known validation gaps, and chronological maintenance
updates. Real CMake/CTest, MSVC, and Clang validation remain open work; Preview 2
was manually built and tested with GCC 10.2.0 i686 and GCC 16.2.0 x64 Windows. The open
validation items are not currently known v1.0.0 functional defects.
