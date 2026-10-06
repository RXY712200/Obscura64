# Obscura64

[![Release: v1.0.0](https://img.shields.io/badge/release-v1.0.0-blue)](https://github.com/RXY712200/Obscura64/releases/tag/v1.0.0)
[![License: MIT](https://img.shields.io/badge/license-MIT-green)](LICENSE)
![Platform: Windows](https://img.shields.io/badge/platform-Windows-blue)
![Language: C11](https://img.shields.io/badge/language-C11-blue)

**Obscura64 2.0.0-preview.2** — Windows C11 development source. The immutable
[v1.0.0 release](https://github.com/RXY712200/Obscura64/releases/tag/v1.0.0)
remains available for released V1 behavior.

Obscura64 is a Windows C11 library for software-internal local data. Its Casual mode raises the effort needed for casual inspection and manual editing: fields such as `coins=1000`, `level=20`, and `unlock=0` become encoded bytes that are less immediately readable.

Choose a protection semantic explicitly: `NONE` leaves bytes plain, `CASUAL`
provides reversible obfuscation with corruption checks, and `CURRENT_USER`
delegates genuine user-bound protection to Windows DPAPI and validates an
inner Obscura64 record after decryption. Casual is not encryption; Obscura64
does not invent cryptography.

## V2 Quick Start

The allocation-first API needs no project directory, Profile, or V1 Provider:

```c
#include "obscura64.h"
#include <string.h>

int main(void)
{
    const unsigned char input[] = {0, 1, 255, 0};
    void *protected_bytes = NULL, *decoded = NULL;
    size_t protected_len = 0, decoded_len = 0;
    obscura64_status status = obscura64_protect_alloc(
        OBSCURA64_PROTECTION_CURRENT_USER, input, sizeof(input),
        &protected_bytes, &protected_len);
    if (status == OBSCURA64_OK)
        status = obscura64_unprotect_alloc(protected_bytes, protected_len,
            &decoded, &decoded_len);
    int matches = status == OBSCURA64_OK && decoded_len == sizeof(input) &&
        memcmp(decoded, input, sizeof(input)) == 0;
    obscura64_free(decoded);
    obscura64_free(protected_bytes);
    return matches ? 0 : 1;
}
```

Choose `OBSCURA64_PROTECTION_CASUAL` for portable obscurity without a secret,
or explicit `NONE` only when no protection is intended. Unprotect identifies
the V2 kind automatically and can also read supported V1 builtin Managed
Payload data. [Preview 2 details](docs/V2_PREVIEW2.md) cover security limits,
legacy recognition, and migration. Caller-buffer forms are available, but a
size query and retry are separate backend operations; allocation-first use is
recommended.

## V1 Features and Compatibility

- Windows-only C11 library with a C and C++ callable public header and opaque context.
- Custom Profile-based Base64-style codec and a frozen library of 4096 unique Profiles.
- Windows BCrypt CSPRNG Profile selection and strict canonical padded decoding.
- Binary-safe, explicit-length buffer APIs and allocating convenience APIs.
- Built-in Provider, runtime custom Providers, and a compile-time default Provider override.
- Project-level `obscura64_open` with UTF-8 Windows paths, stable Project ID, and generation tracking.
- Explicit Force Reinitialize with preservation of the latest three historical Profiles.
- Verified persistence, process coordination, LocalAppData redundancy and automatic state recovery.
- Managed Payload validation and a separate exhaustive builtin Profile recovery CLI.

## Documentation

- [V2 Preview 2 protection and legacy compatibility](docs/V2_PREVIEW2.md)
- [V2 Preview 1 format, API, security boundary, and V1 compatibility](docs/V2_PREVIEW1.md)
- [Documentation index](docs/README.md)
- [V1 specification](docs/OBSCURA64_V1_SPEC.md)
- [Managed Payload V1](docs/MANAGED_PAYLOAD_V1.md)
- [Recovery policy and limits](docs/RECOVERY_V1.md)
- [Changelog](CHANGELOG.md) and [v1.0.0 release notes](docs/RELEASE_NOTES_V1.0.0.md)
- [Published v1.0.0 Release](https://github.com/RXY712200/Obscura64/releases/tag/v1.0.0)
- [Issue #1 — Development Log, Known Issues & Roadmap](https://github.com/RXY712200/Obscura64/issues/1)

## V1 Quick Start (Managed Payload)

Include `obscura64.h` and pass an **existing project directory** as a UTF-8 path. This example accepts that directory as its command-line argument:

```c
#include "obscura64.h"
#include <stdio.h>

int main(int argc, char **argv)
{
    const char data[] = "coins=1000\nlevel=20\nunlock=0\n";
    obscura64_context *context = NULL;
    char *encoded = NULL;
    void *decoded = NULL;
    size_t encoded_len = 0, decoded_len = 0;
    obscura64_status status;

    if (argc != 2) {
        fprintf(stderr, "usage: quick_start <existing-project-directory>\n");
        return 1;
    }
    status = obscura64_open(argv[1], &context);
    if (status != OBSCURA64_OK) goto cleanup;
    status = obscura64_managed_encode_alloc(context, data, sizeof(data) - 1,
                                   &encoded, &encoded_len);
    if (status != OBSCURA64_OK) goto cleanup;
    status = obscura64_managed_decode_alloc(context, encoded, encoded_len,
                                   &decoded, &decoded_len);
    if (status == OBSCURA64_OK) fwrite(decoded, 1, decoded_len, stdout);

cleanup:
    obscura64_free(decoded);
    obscura64_free(encoded);
    obscura64_context_destroy(context);
    if (status != OBSCURA64_OK)
        fprintf(stderr, "%s\n", obscura64_status_string(status));
    return status == OBSCURA64_OK ? 0 : 1;
}
```

Encoded and decoded buffers use explicit lengths and are not NUL-terminated. Release allocating API results with `obscura64_free`.

Managed Payload wraps bytes in the fixed V1 envelope before the context Provider encodes them. It validates magic, length and SHA-256 on decode, detecting corruption and wrong-Profile use. Empty payloads still produce a nonempty encoded envelope. Raw Codec only transforms bytes and cannot validate wrong-Profile use. See [Managed Payload V1](docs/MANAGED_PAYLOAD_V1.md), [managed example](examples/managed_quickstart.c), and [raw example](examples/raw_codec.c).

## Project State

The first successful open creates `<project>/.obscura64/current.state`. Later opens validate and reuse it without changing the Profile. Force Reinitialize also maintains `<project>/.obscura64/history.state`.

Back up and migrate `.obscura64` together with the project data. An existing state directory with missing or corrupt current triggers validated automatic recovery. If no usable source exists, open returns `OBSCURA64_UNRECOVERABLE`; it never creates a replacement identity.

Force Reinitialize keeps Project ID, increments generation, and chooses a different Profile. Existing contexts retain their old Profile; callers should retire old contexts when finished. Previously encoded data is not automatically migrated to the new Profile.

Stage 5 persistence hardening is complete: verified same-directory replacement, managed current/history consistency checks, Windows shared/exclusive kernel locking, and deterministic failure/process-crash tests. Normal open uses a shared lock and only requires valid current; Force uses an exclusive lock and requires consistent history when generations require it. Existing-project lock contention returns BUSY without waiting. First-initialization race coordination alone permits a bounded grace of at most 240ms; a kernel-only marker distinguishes it from ordinary mutation. The persistent, empty `operation.lock` file is not ownership; process exit releases its kernel locks. Stale temp files are ignored and preserved. Stage 6 adds recovery and per-user redundancy as described below.

These tests do not certify all power-loss, filesystem or controller failure scenarios. File flush and write-through replacement are used; no universal directory-durability guarantee is claimed. See the [state persistence notes](docs/PROJECT_STATE_V1.md).

Stage 6 is complete: a per-user LocalAppData mirror stores the existing current/history formats. Valid project current always wins. Automatic recovery prefers exact backup current, then recent history; history fallback may roll back the Profile and cannot promise decoding data from a lost newer Profile. Backup maintenance is best-effort and never rolls back a successful primary operation. See [recovery behavior and limits](docs/RECOVERY_V1.md).

## Security Model

V1 and V2 Casual provide reversible obfuscation, not cryptographic confidentiality. V1 Managed Payload, V1 state/history, and V2 Casual SHA-256 digests detect corruption; they are not secret authentication. Someone who understands those formats can modify data and recompute their digests. V2 CurrentUser instead delegates Windows user-bound protection to DPAPI.

Authorization systems and anti-cheat systems must not trust client-side data merely because it is protected locally. Neither the Casual algorithm nor the frozen Profile Library is secret. CurrentUser is tied to the Windows user environment and is not portable or a server trust boundary.

## Frozen Profile Library

- Profile count: **4096**.
- Canonical payload: **262144 bytes**.
- Profile Library V1 SHA-256:

```text
8842cc4aa32bcb300937835f72aaacfd088568d1bfecd01811603c4f16e7280a
```

This is a frozen library identity/integrity reference, not a secret or a measure of security strength. The runtime table is embedded in the library; ordinary callers do not load the audit artifacts from `generated/`. The candidate artifact retains its historical `RB64_PROFILE_LIBRARY_CANDIDATE_V1` magic; see [Profile Library V1](docs/PROFILE_LIBRARY_V1.md).

## Build

Requirements: Windows, a C11-capable C compiler, and Windows BCrypt/Crypt32/Shell/OLE libraries. Link with `bcrypt`, `crypt32`, `shell32`, `ole32`, and `uuid` (`-lbcrypt -lcrypt32 -lshell32 -lole32 -luuid` with MinGW).

CMake is a development/build/test tool, not a runtime dependency. To integrate directly into a Windows C/C++ project, add all implementation `.c` files and supporting private headers from `src/`, add `include/` to the compiler include path, and link `bcrypt`, `crypt32`, `shell32`, `ole32`, and `uuid`. Compile implementation files as C; callers include only `obscura64.h`.

The repository includes a CMake configuration:

```text
cmake -S . -B build
cmake --build build --config Debug
cd build
ctest -C Debug --output-on-failure
```

CMake/CTest has not yet been executed in the current development environment because CMake is unavailable there. The code has been manually built and tested with MinGW GCC 10.2.0 using C11 and `-Wall -Wextra -Wpedantic`, with zero compiler warnings and errors in that regression. This is not a zero-warning guarantee for every compiler.

Advanced integrations may select a compile-time default Provider with the `OBSCURA64_DEFAULT_PROVIDER_SYMBOL` CMake setting and provide the named `obscura64_provider` instance in their linked sources.

## Tests

Test categories cover smoke/link checks, Profile validation and generation, the frozen library, runtime selection, strict codec behavior, public APIs, Providers, state formats, project initialization and reopening, Force Reinitialize/history retention, multi-process locking, and deterministic persistence failure/process-crash handling. Managed Payload, exhaustive Profile recovery, and Unicode CLI safety are also covered. Tests use standard C and Windows APIs without a third-party test framework.

## Disaster Recovery Tool

Build the source target `obscura64_recover` and run (manual MinGW CLI linking also needs `-municode` for its Unicode entry point):

```text
obscura64_recover <encoded-input-file> <new-decoded-output-file>
```

The CLI uses Unicode Windows paths, reads the exact input bytes without trimming, and scans all 4096 frozen builtin Profiles. Only a unique valid Managed Payload envelope is accepted. It prints the recovered ID and full Profile and writes binary payload bytes to a new file; existing outputs are never overwritten. It does not open, initialize, repair or modify any project state. Raw data and arbitrary custom Provider transports are unsupported. Recovery of payload/Profile does not reconstruct Project ID, generation or history. See [recovery limits](docs/RECOVERY_V1.md).

## Current Status and Roadmap

V1 development stages and release preparation are complete. Frozen Profile Library V1 content and ID order remain unchanged.

- Stage 0: Specification — complete.
- Stage 1: Codec Core — complete.
- Stage 2: Profile System — complete.
- Stage 3: Public API / Provider — complete.
- Stage 4: Project State / History — complete.
- Stage 5: Persistence Hardening — complete.
- Stage 6: Redundancy / Recovery — complete.
- Stage 7: Managed Payload / Disaster Recovery / V1 Preparation — complete.

V2 protected-file helper APIs and streaming are not implemented. CurrentUser uses Windows DPAPI; Obscura64 does not implement a custom encryption algorithm. Optional examples can be built with `OBSCURA64_BUILD_EXAMPLES=ON`; this is not required for source integration.

See [current format](docs/PROJECT_STATE_V1.md), [history format](docs/PROJECT_HISTORY_V1.md), and the [release checklist](docs/RELEASE_CHECKLIST_V1.md). The v1.0.0 release was verified with MinGW GCC/G++ 10.2.0 on i686 Windows. Preview 2 passed manual GCC builds and formal tests on i686 (10.2.0) and x64 (16.2.0) Windows. MSVC, Clang, and CMake/CTest execution are not claimed.

## License

[MIT License](LICENSE), Copyright (c) 2026 RXY712200. Third-party projects listed in [references](docs/THIRD_PARTY_REFERENCES.md) are design references, not vendored implementations.
