# V1 release checklist

## Development evidence

- [x] Managed format, Provider behavior, wrong Profile, tamper and atomicity tests.
- [x] Full 4096 unique/no-match scan and test-only ambiguity coverage.
- [x] Unicode CLI integration with exact payload and existing-output protection.
- [x] Full MinGW formal regression, public C/C++ consumers, and examples.
- [x] Timing-sensitive regression repeated; compiler warnings/errors zero.
- [x] Frozen artifacts unchanged; State/History formats and Provider ABI preserved.
- [x] Source/license, public header, repository hygiene and invariant audits.
- [x] Release notes prepared. CMake static audit only; no MSVC/Clang/x64 claim.

## Recorded candidate validation (2026-10-05)

MinGW GCC/G++ 10.2.0, i686-w64-mingw32. All C implementation/test builds used
C11 with `-Wall -Wextra -Wpedantic`; compiler warnings/errors: zero. All 21 formal
executables returned zero. Existing reinitialize (283 history + 200 Force),
persistence (80), failure/crash (237), redundancy (109) and recovery (69) checks
passed. New checks: managed_payload 156, disaster 44, managed_default 6,
recover_tool 17; each returned zero.

Locking, managed_state, project, recovery and disaster suites each passed 100/100
repeat runs (500/500 total). A representative full 4096 scan of a 4096-byte payload
measured 37.015 ms; this is informational, not a public performance guarantee.

External C11/C++11 consumers used only the public include directory and exercised
open, Managed allocation/round-trip and cleanup. A separate production C archive
passed C/C++ context/Managed consumers without test switches. Both example
programs compiled and ran. Project-opening tests/examples used an isolated
instrumented backup root; the production archive contains no test scan entry.

Static CMake audit verified all 17 library sources, 21 test registrations,
Windows libraries, Unicode MinGW CLI entry, private include boundaries and
optional examples. CMake/CTest was unavailable and was not run.

Frozen binary: 262144 bytes,
`8842cc4aa32bcb300937835f72aaacfd088568d1bfecd01811603c4f16e7280a`.
Candidate: 286765 bytes,
`024444dd8bc3ab533c10131bdfb4aca4cdd50a5a95d1369a50d6cd650c84ae90`.
Public Provider ABI stays 1; State 160 bytes and History 336 bytes are unchanged.
Repository scans found no credentials, developer absolute paths, runtime state
or build binaries among project files. MIT and reference-only attribution remain.
The 24 V1 invariants were checked against regression evidence and source/API/docs;
no known release-blocking defect was found in the verified environment.

## Publication verification

- [x] Stage 7 diff and development evidence reviewed; final release authorized.
- [x] Release source commit `f255d84cf08a1c02d0ef2d5b9eb79c9719a36e25`
      created and pushed to main; remote main matched the reviewed commit at publication.
- [x] Annotated `v1.0.0` tag created and verified to point exactly to that commit.
- [x] [GitHub Release v1.0.0](https://github.com/RXY712200/Obscura64/releases/tag/v1.0.0)
      published from the existing tag, using the complete release notes, without
      development binary assets.
- [x] GitHub-generated source ZIP and tar.gz available; source ZIP contains the
      public header, sources, tests,
      examples, documentation, CLI source and frozen artifacts.
- [x] Frozen library and candidate sizes/SHA-256 verified from the downloaded
      tagged source ZIP; both match the values recorded above.
- [x] Final pre-commit audit: no credentials, runtime state, caches or local build binaries.

No release binary was required or attached. This checklist was updated in a
post-release repository-maintenance commit to record publication results. That
maintenance commit is **not part of the immutable v1.0.0 tagged source tree**.
