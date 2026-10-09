# Obscura64 v2.0.0 — release validation record

This document records **pre-publication** V2 review/validation evidence.
The public [v2.0.0 GitHub Release](https://github.com/RXY712200/Obscura64/releases/tag/v2.0.0)
and annotated stable tag are the authoritative proof of actual publication.
Publication is conditional on **fresh successful Windows CI for the stable
release commit**, not merely this checked-in document.

## Product and compatibility

- [x] All five planned Previews passed independent review: [Preview 5 PASS](https://github.com/RXY712200/Obscura64/issues/2#issuecomment-6075448935).
- [x] Preview 1–4 annotated tags verified at unchanged approved commits.
- [x] Frozen V1 library 262144 bytes, SHA-256 `8842cc4aa32bcb300937835f72aaacfd088568d1bfecd01811603c4f16e7280a`.
- [x] V1 State/History/Managed Payload, 4096 Profiles, Provider ABI 1 and deterministic vectors retained.
- [x] V2 envelope/Casual/CurrentUser inner-record fixtures unchanged.
- [x] V2 scope frozen; no further protection kinds or storage products introduced.

## Actual Windows toolchains and distribution evidence

- [x] MinGW GCC 10.2.0 i686, GCC 16.2.0 x64: CMake/CTest 35/35 and strict C11 direct 35/35 each, zero GCC compile warnings.
- [x] MSVC 19.51.36260.0: CMake/CTest 35/35 on x64 and Win32.
- [x] clang-cl 19.1.5 x64: hosted Windows CI 35/35.
- [x] Installed static-library `find_package(Obscura64 CONFIG REQUIRED)` C and C++11 consumers 2/2 on MSVC x64/Win32.
- [x] `BUILD_TESTING=OFF` excludes formal tests and active fault hooks.
- [x] Windows CI on Preview-5 head [run 37733067399](https://github.com/RXY712200/Obscura64/actions/runs/37733067399) passed at `1e621e37171e9ce195b83c300ccbf7115145116d`.
- [x] Release automation gates the stable tag and public GitHub Release on a new successful `Windows V2` run for the stable-source commit.

## Hardening and limitations

- [x] V1/V2 crash/fault tests, legacy recovery, upgrade and CLI regressions included.
- [x] Multi-hard-link target rejection tested; path/Unicode checks passed.
- [x] MSVC x64 selected AddressSanitizer path/mutation/threading/envelope tests 4/4.
- [x] 100,000 deterministic parser mutations per mutation run.
- [x] MSVC `/analyze` completed: one uninitialized-local warning fixed; two C6386 bounded-write findings assessed as false positives (not independently re-executed).
- [x] Canonical V2 guide, README, security and contributor docs consolidated.

The following are explicitly **not represented as completed coverage**:

- Local 8.3 and symbolic-link creation were unavailable and those tests were skipped.
- MSVC and clang-cl production library source was warning-clean in Preview-5 CI, but tests/tools still emitted 187/187/178 CRT deprecation and related warnings without suppression.
- Two temporary-directory cleanup checks failed under simultaneous test runs but passed when rerun independently; the serial suites and final CI passed.
- No libFuzzer or whole-library exhaustive sanitizer coverage is claimed.
- Sidecar locks require cooperating processes. CURRENT_USER uses Windows DPAPI, and no arbitrary hardware-power-loss guarantee is offered.

## Publication boundary

The release workflow on `main` verifies the existing Preview tags and
versions, waits for a successful `Windows V2` test run at the actual stable
release commit, then publishes annotated `v2.0.0-preview.5` at the approved
Preview-5 checkpoint and annotated stable `v2.0.0` at the stable source
commit. GitHub Releases is the authoritative publication record.

Historical V1 verification is recorded separately in
[RELEASE_CHECKLIST_V1.md](RELEASE_CHECKLIST_V1.md).
