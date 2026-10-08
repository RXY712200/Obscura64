# V2 release candidate checklist

This checklist records Preview 5 validation. Complete entries only after the
named command or CI run succeeds. Independent review, Preview 5 tag, and stable
v2.0.0 publication are separate later gates.

## Compatibility

- [x] Approved Preview 1–4 tags verified at immutable target commits.
- [x] Frozen V1 Profile Library is 262144 bytes and matches its canonical SHA.
- [x] V1 State, History, Managed Payload, Provider ABI, and known vectors pass.
- [x] V2 envelope, Casual, and CurrentUser inner-record fixtures pass unchanged.

## Toolchains and integration

- [x] GCC i686 strict C11 formal suite and direct C/C++ consumers pass.
- [x] GCC x64 strict C11 formal suite and direct C/C++ consumers pass.
- [x] MSVC x64 and Win32 CMake builds and complete CTest suites pass.
- [x] Clang x64 CMake build and complete CTest suite pass.
- [x] `BUILD_TESTING=OFF` builds only selected production targets.
- [x] Installed package resolves via `find_package` for external C and C++11 consumers.
- [x] GitHub Actions Windows workflow completes successfully.

## Hardening and release review

- [x] Path alias tests, deterministic mutation test, CLI and crash regressions pass.
- [x] Practical sanitizer and static-analysis results are reviewed.
- [x] V2 guide, README, SECURITY, CONTRIBUTING, and status strings are audited.
- [x] No frozen artifact, V1/V2 wire format, or published tag changes.
- [x] Diff, secret/build-artifact scan, and `git diff --check` pass.
- [ ] Independent V2 Preview 5 review passes before any Preview 5 tag.
- [ ] Stable release validation and publication are performed separately.

## Local evidence

- CMake 4.3.1-msvc1; MSVC 19.51.36260.0, Visual Studio 18 2026 generator,
  Release x64 and Win32: 35/35 CTest each. MinGW GCC 16.2.0 x64 and 10.2.0
  i686, Ninja Release: 35/35 CTest each, zero GCC warnings. The additional
  test runs six workers through independent V2 protection and a shared V1
  context concurrently. Manual GCC C11 `-Wall -Wextra -Wpedantic` suites:
  35/35 per architecture, zero warnings.
- External installed-package C and C++11 consumers: 2/2 on MSVC x64 and
  Win32. Direct-source C/C++11 consumers: both pass on GCC i686 and x64.
- MSVC x64 AddressSanitizer: 4/4 selected path/mutation/threading/envelope
  tests. The deterministic mutation suite exercises 100,000 parser mutations
  per run.
- MSVC `/analyze` covered the production library. Three diagnostics from the
  first pass included an uninitialized-local warning fixed in Preview 5 and
  two C6386 reports on mathematically bounded allocations/writes; the latter
  were reviewed as analyzer false positives. No sanitizer finding remains.
- Local 8.3 short-name and symlink creation were unavailable and explicitly
  skipped; hard-link rejection was exercised. No local Clang compiler was
  found. GitHub Actions supplied clang-cl 19.1.5 on Windows Server 2022.
- Under simultaneous test runs, one MSVC x64 file-locking test and one GCC
  i686 persistence test failed while removing their temporary directories.
  Both passed in isolated reruns; the complete serial CTest and strict GCC
  suites then passed. This cleanup sensitivity remains for independent review.
- [GitHub Actions run 37631242883](https://github.com/RXY712200/Obscura64/actions/runs/37631242883)
  passed on commit `9b6620ccd4380606d3c2466db0b2712741553b09`:
  MSVC x64 34/34 plus installed consumers 2/2, MSVC Win32 34/34, and
  clang-cl x64 34/34. Production Clang objects had no diagnostics. The Clang
  full suite emitted 178 CRT deprecation warnings in legacy tests/tools
  (`fopen`, `wcscpy` and similar); these are not suppressed and remain for
  independent review. `clang-cl /W4` is used as Clang's documented equivalent
  of `-Wall -Wextra`; `/Wall` would enable `-Weverything` on this driver.
- [Final code CI run 37732895800](https://github.com/RXY712200/Obscura64/actions/runs/37732895800)
  passed on commit `8ef16de4de4c4c4e5fe2bddc9c8f7c29849dec7b`:
  MSVC x64 35/35 plus installed C/C++11 consumers 2/2, MSVC Win32 35/35,
  and clang-cl x64 35/35. All three jobs built the production sources with
  zero compiler warnings or errors. Existing tests/tools emitted 187 MSVC
  x64, 187 MSVC Win32, and 178 clang-cl warnings; these remain visible.
