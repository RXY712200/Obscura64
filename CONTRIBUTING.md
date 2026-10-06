# Contributing to Obscura64

Useful contributions include reproducible bug reports, focused bug or portability
fixes, build-system improvements, documentation corrections, tests, and carefully
scoped API proposals. Check [the development log](https://github.com/RXY712200/Obscura64/issues/1)
and existing Issues before starting. Discuss major API or architectural changes
in an Issue before implementation.

## Compatibility boundaries

- V1 is a Windows-only C11 implementation. Compile implementation files as C;
  the public header also supports C++ callers.
- Keep byte inputs and outputs length-delimited and binary-safe. Do not infer
  buffer length with `strlen` or weaken documented error/output atomicity.
- Keep `include/obscura64.h` free of Windows headers/types and private structures.
- Preserve Current State V1 (160 bytes), History V1 (336 bytes), Managed Payload
  V1, and Provider ABI version 1 unless a versioned change is explicitly agreed.
- Profile Library V1 contains 4096 frozen Profiles. Its bytes, ID order, and
  canonical SHA-256 must remain unchanged:
  `8842cc4aa32bcb300937835f72aaacfd088568d1bfecd01811603c4f16e7280a`.
  Incompatible library changes need an explicit new version, not replacement of V1.
- V1 and V2 Casual are reversible obfuscation, not encryption or authentication;
  V2 CurrentUser delegates protection to Windows DPAPI. Keep
  security claims within the documented scope.

## Validation

Behavioral changes require focused regression tests. Run the relevant
tests and, where practical, the full suite; report the actual toolchain,
architecture, commands, results, and any untested areas. The verified v1.0.0
environment is MinGW GCC/G++ 10.2.0, i686-w64-mingw32, C11, with
`-Wall -Wextra -Wpedantic`; keep those builds warning-clean.

CMake received static review only in that environment. Preview 2 also passed
manual GCC/G++ builds and formal tests on x64 Windows. Real clean Windows
configure/build/CTest execution, MSVC, and Clang remain validation work.
Do not present them as tested until evidence is available. CMake is a
development tool, not a runtime dependency; direct source integration remains supported.

## Pull requests

Explain the problem, behavior/change scope, tests performed, and compatibility
impact. State effects on public APIs, formats, Provider ABI, and frozen artifacts.
Update relevant documentation. Keep changes focused and use the PR checklist.

Do not commit `.exe`, `.o`, `.obj`, `.dll`, other build artifacts, caches,
temporary output, runtime `.obscura64` state, local developer paths, or secrets.
Remove private data from examples and logs. For security-sensitive findings,
follow [SECURITY.md](SECURITY.md).
