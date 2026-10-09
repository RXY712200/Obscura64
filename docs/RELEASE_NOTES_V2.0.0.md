# Obscura64 v2.0.0 — Stable Release

**Windows · C11 · MIT License**

Obscura64 2.0.0 turns the original reversible-obfuscation module into a small library for **the lifecycle of protected local application blobs**. It provides a versioned representation and reliable protected-file operations without asking ordinary callers to manage Profiles, Project IDs, generation counters, or DPAPI flags.

## Highlights

- **Three explicit protection semantics:** `NONE` (versioned plain bytes, no integrity), `CASUAL` (reversible obfuscation plus corruption detection, not encryption), and `CURRENT_USER` (Windows DPAPI current-user protection plus validated inner record).
- **Allocation-first memory API:** `obscura64_protect_alloc`, `obscura64_unprotect_alloc`, automatic envelope dispatch, precise errors, and optional V1/V2 format classification.
- **Reliable single-file lifecycle:** `obscura64_write_file`, `obscura64_read_file_alloc`, verified same-directory replacement, one validated `.ob64.bak`, nonblocking shared/exclusive `.ob64.lock`, and validated read fallback.
- **Explicit safe upgrade:** `obscura64_upgrade_file` upgrades supported V1 builtin Managed Payload or reprotects current V2 data under a chosen semantic; it refuses destructive overwrite of inaccessible DPAPI data or unknown future formats.
- **Maintenance CLI:** `obscura64 inspect`, `verify`, and `upgrade --to none|casual|current-user`, with truthful semantic-specific verification output.
- **Windows CMake package:** installable static library exported as `Obscura64::obscura64` for external C/C++11 consumers. Direct C source integration remains supported.

## Compatibility

The V1 public APIs, frozen 4096 Profiles, State V1, History V1, Managed Payload V1, and Provider ABI 1 are preserved. `obscura64_recover` remains available. Automatic migration recognizes builtin V1 Managed Payload; arbitrary V1 custom Providers still require their original context. No released V1 artifact was overwritten.

V2 protects local application bytes; it does not interpret application schemas, issue license/authentication decisions, or become a general encryption framework.

## Installation

Source archives are available from this GitHub Release. On Windows with CMake 3.16+:

```powershell
cmake -S . -B build -DBUILD_TESTING=OFF
cmake --build build --config Release
cmake --install build --config Release --prefix "C:/obscura64-install"
```

Downstream CMake:

```cmake
find_package(Obscura64 CONFIG REQUIRED)
target_link_libraries(your_app PRIVATE Obscura64::obscura64)
```

The installed package is a static library; no Windows installer or prebuilt cross-toolchain binary bundle is promised. See the [README](https://github.com/RXY712200/Obscura64/blob/v2.0.0/README.md) and [V2 guide](https://github.com/RXY712200/Obscura64/blob/v2.0.0/docs/V2_GUIDE.md).

## Validation and remaining limits

The release candidate passed independent review across all five planned V2 Previews. Confirmed Windows CI configurations include MSVC x64 and Win32 and clang-cl x64 (35/35 CTest each), with external installed C/C++11 consumers. MinGW GCC i686/x64, selected MSVC AddressSanitizer tests, 100,000 deterministic parser mutations per run, and crash/failure regressions were also reported. The stable release pipeline requires a fresh successful Windows CI run before creating this Release.

Environment-dependent 8.3 and symlink path tests were skipped where unsupported. Test/tool CRT deprecation warnings remain, as do two reviewed MSVC `/analyze` C6386 warnings considered bounded-write false positives. External or hostile filesystem writers that ignore the sidecar lock are not coordinated. No arbitrary hardware-power-loss guarantee is claimed.

**Security:** CASUAL and V1 are not cryptographic confidentiality or authentication. NONE has no integrity. CURRENT_USER relies on Windows DPAPI and is not portable or server-authoritative. Obscura64 is not a DRM, credential-vault, or general-purpose crypto product.

Full validation details: [V2 release checklist](https://github.com/RXY712200/Obscura64/blob/v2.0.0/docs/RELEASE_CHECKLIST_V2.md). License: [MIT](https://github.com/RXY712200/Obscura64/blob/v2.0.0/LICENSE).
