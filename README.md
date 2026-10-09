# Obscura64

[![Release v2.0.0](https://img.shields.io/badge/release-v2.0.0-blue)](https://github.com/RXY712200/Obscura64/releases/tag/v2.0.0)
[![Windows CI](https://github.com/RXY712200/Obscura64/actions/workflows/windows-v2.yml/badge.svg)](https://github.com/RXY712200/Obscura64/actions/workflows/windows-v2.yml)
[![MIT](https://img.shields.io/badge/license-MIT-green)](LICENSE)
![Windows](https://img.shields.io/badge/platform-Windows-blue)
![C11](https://img.shields.io/badge/language-C11-blue)

**Obscura64 2.0.0** is a Windows C11 library for the lifecycle of **protected local application bytes and files**. Give it binary data, choose a protection semantic, and let it handle versioned envelopes, validation, reliable file replacement, one validated backup, fallback, and explicit migration. It is not a database, serializer, DRM system, or home-grown cryptography framework.

The historical [v1.0.0 release](https://github.com/RXY712200/Obscura64/releases/tag/v1.0.0) remains available. V1 APIs and frozen data formats remain compatible.

## Choose the protection you actually need

| Semantic | What you get | What you **do not** get |
| --- | --- | --- |
| `NONE` | A versioned envelope and the same file lifecycle, with plain application bytes | Confidentiality, integrity, or authentication |
| `CASUAL` | Reversible obfuscation and corruption checks, without managing a secret | Encryption or attacker-resistant authentication |
| `CURRENT_USER` | Windows DPAPI current-user protection plus validated Obscura64 inner format | Portability, DRM, or a server trust boundary |

Choose explicitly; the library never silently selects a mode. The implementation may use Profile data or DPAPI internally, but ordinary V2 callers do not need Profile IDs, Providers, project generations, or key-management parameters.

## Start with one protected file

The parent directory must already exist. The file API takes an **absolute UTF-8 Windows path**.

```c
#include "obscura64.h"
#include <stddef.h>

int main(void)
{
    const char *path = "C:\\existing-directory\\settings.ob64";
    const unsigned char settings[] = {0, 1, 255, 0};
    void *loaded = NULL;
    size_t loaded_len = 0;
    obscura64_status s = obscura64_write_file(
        path, OBSCURA64_PROTECTION_CURRENT_USER, settings, sizeof(settings));

    if (s == OBSCURA64_OK)
        s = obscura64_read_file_alloc(path, &loaded, &loaded_len);

    /* Check s and loaded_len before consuming loaded. */
    obscura64_free(loaded);
    return s == OBSCURA64_OK ? 0 : 1;
}
```

A write verifies a same-directory replacement and maintains one adjacent `<file>.ob64.bak`. Reads prefer the primary and can validate/fall back to the backup without rewriting the primary. Cooperation is coordinated by `<file>.ob64.lock`; contention returns `OBSCURA64_BUSY`. A failed DPAPI unprotect does **not** authorize destructive overwrite.

For in-memory protection, use `obscura64_protect_alloc` and `obscura64_unprotect_alloc`. All `*_alloc` results are length-delimited (not strings) and released with `obscura64_free`. `unprotect` automatically dispatches supported V2 kinds and recognizes builtin V1 Managed Payload.

The canonical [V2 user guide](docs/V2_GUIDE.md) covers exact failure precedence, backup evolution, concurrency, path restrictions, thread safety, ownership, and security limits.

## Maintenance CLI

Build the optional `obscura64_cli` target to get `obscura64.exe`:

```text
obscura64 inspect settings.ob64
obscura64 verify settings.ob64
obscura64 upgrade settings.ob64 --to current-user
```

- `inspect` identifies the named protected file structurally without printing plaintext.
- `verify` validates exactly that file (no backup fallback). Its output distinguishes the guarantees of NONE, CASUAL, CURRENT_USER, and V1 Managed.
- `upgrade` explicitly converts supported old data or changes a V2 protection semantic under one exclusive lock. It refuses to overwrite potentially valid but inaccessible CurrentUser data and preserves the appropriate original protected bytes as backup.

Changing `CURRENT_USER` to `CASUAL` or `NONE` is allowed only when explicitly requested; it is **not** a security improvement. The specialized V1 `obscura64_recover` CLI remains available for builtin Managed Payload disaster recovery.

## Build and integrate

Requires Windows, a C11-capable compiler, and CMake 3.16+ for the supported package build.

```powershell
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
cmake --install build --config Release --prefix "C:/obscura64-install"
```

An external C or C++ project can then use:

```cmake
find_package(Obscura64 CONFIG REQUIRED)
target_link_libraries(your_app PRIVATE Obscura64::obscura64)
```

Pass the installation prefix through `CMAKE_PREFIX_PATH`. Set `BUILD_TESTING=OFF` for a smaller production-only build; `OBSCURA64_BUILD_TOOLS` and `OBSCURA64_INSTALL_TOOLS` control the CLI tools, while `OBSCURA64_BUILD_EXAMPLES` is opt-in. Direct C-source integration is also supported: compile implementation files as C, include only `include/obscura64.h` in consumers, and link Windows `bcrypt`, `crypt32`, `shell32`, `ole32`, and `uuid`.

**Validation at the v2.0.0 release candidate:** MSVC x64/Win32, clang-cl x64, and MinGW GCC i686/x64 full 35-test suites; installed C/C++11 consumers; selected AddressSanitizer; deterministic parser mutations and crash tests. See the [V2 validation record](docs/RELEASE_CHECKLIST_V2.md) and [Windows CI](https://github.com/RXY712200/Obscura64/actions/workflows/windows-v2.yml). This is not a guarantee for every filesystem, configuration, or crash condition.

## Compatibility and boundaries

- V1 Profile/Provider/project APIs remain available; V1 State, History, Managed Payload, Provider ABI v1, and the 4096 frozen Profiles retain their existing formats/identities.
- Automatic legacy recognition covers **builtin V1 Managed Payload** only. Arbitrary V1 custom Providers still require their original V1 context.
- The versioned V2 envelope, Casual body, and CurrentUser inner record retain their reviewed wire layouts.
- File API paths must be full UTF-8 Windows paths with existing parents. Existing multi-hard-link targets and target reparse points are rejected; normal path aliases are resolved where Windows supports it. A hostile filesystem writer that ignores Obscura64's lock is not coordinated.
- `NONE` has no integrity check. CASUAL and V1 digests are public corruption checks, **not** cryptographic signatures. CURRENT_USER delegates real user-bound protection to Windows DPAPI but does not make client-side data authoritative.
- Obscura64 does not guarantee survival of arbitrary hardware/controller power loss and does not implement streaming, cross-platform key management, or a general encryption framework.

## Documentation

- [V2 guide](docs/V2_GUIDE.md) — canonical current user documentation
- [v2.0.0 release notes](docs/RELEASE_NOTES_V2.0.0.md)
- [Changelog](CHANGELOG.md) and [validation record](docs/RELEASE_CHECKLIST_V2.md)
- [Documentation index](docs/README.md)
- [V1 specification](docs/OBSCURA64_V1_SPEC.md), [V1 Managed Payload](docs/MANAGED_PAYLOAD_V1.md), [V1 recovery](docs/RECOVERY_V1.md)
- [Historical Preview 1–4 development notes](docs/README.md#historical-v2-preview-records)
- [Security policy](SECURITY.md), [Contributing](CONTRIBUTING.md), [MIT License](LICENSE)
- [Development log (Issue #1)](https://github.com/RXY712200/Obscura64/issues/1)

Copyright (c) 2026 RXY712200. Licensed under MIT.
