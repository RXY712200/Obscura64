# Obscura64

**Pre-1.0 / Active Development**

Obscura64 is a lightweight reversible obfuscation library for software-internal local data. It raises the effort needed for casual inspection and manual editing: fields such as `coins=1000`, `level=20`, and `unlock=0` become encoded bytes that are less immediately readable.

Obscura64 is **NOT encryption**. It is not designed to resist professional reverse engineering, knowledgeable attackers, or deliberate cryptanalysis.

## Current Features

- Windows-only C11 library with a C and C++ callable public header and opaque context.
- Custom Profile-based Base64-style codec and a frozen library of 4096 unique Profiles.
- Windows BCrypt CSPRNG Profile selection and strict canonical padded decoding.
- Binary-safe, explicit-length buffer APIs and allocating convenience APIs.
- Built-in Provider, runtime custom Providers, and a compile-time default Provider override.
- Project-level `obscura64_open` with UTF-8 Windows paths, stable Project ID, and generation tracking.
- Explicit Force Reinitialize with preservation of the latest three historical Profiles.

## Quick Start

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
    status = obscura64_encode_alloc(context, data, sizeof(data) - 1,
                                   &encoded, &encoded_len);
    if (status != OBSCURA64_OK) goto cleanup;
    status = obscura64_decode_alloc(context, encoded, encoded_len,
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

## Project State

The first successful open creates `<project>/.obscura64/current.state`. Later opens validate and reuse it without changing the Profile. Force Reinitialize also maintains `<project>/.obscura64/history.state`.

Back up and migrate `.obscura64` together with the project data. An existing state directory with missing current state is an error; corrupt state is never silently replaced. History is not yet an automatic recovery source.

Force Reinitialize keeps Project ID, increments generation, and chooses a different Profile. Existing contexts retain their old Profile; callers should retire old contexts when finished. Previously encoded data is not automatically migrated to the new Profile.

Stage 5 persistence hardening is complete: verified same-directory replacement, managed current/history consistency checks, Windows shared/exclusive kernel locking, and deterministic failure/process-crash tests. Normal open uses a shared lock and only requires valid current; Force uses an exclusive lock and requires consistent history when generations require it. Existing-project lock contention returns BUSY without waiting. First-initialization race coordination alone permits a bounded grace of at most 240ms; a kernel-only marker distinguishes it from ordinary mutation. The persistent, empty `operation.lock` file is not ownership; process exit releases its kernel locks. Stale temp files are ignored and preserved. No automatic recovery or redundant copies exist yet.

These tests do not certify all power-loss, filesystem or controller failure scenarios. File flush and write-through replacement are used; no universal directory-durability guarantee is claimed. See the [state persistence notes](docs/PROJECT_STATE_V1.md).

## Security Model

Obscura64 provides reversible obfuscation, not cryptographic confidentiality. Current-state and history SHA-256 digests primarily detect corruption. Someone who understands the format can modify data and recompute the digest.

Authorization systems, anti-cheat systems, and confidential data must not rely on Obscura64 alone. Neither the algorithm nor the frozen Profile Library is secret. Obscura64 raises the barrier to casual inspection and manual editing; it does not provide tamper-proof storage or authentication.

## Frozen Profile Library

- Profile count: **4096**.
- Canonical payload: **262144 bytes**.
- Profile Library V1 SHA-256:

```text
8842cc4aa32bcb300937835f72aaacfd088568d1bfecd01811603c4f16e7280a
```

This is a frozen library identity/integrity reference, not a secret or a measure of security strength. The runtime table is embedded in the library; ordinary callers do not load the audit artifacts from `generated/`. The candidate artifact retains its historical `RB64_PROFILE_LIBRARY_CANDIDATE_V1` magic; see [Profile Library V1](docs/PROFILE_LIBRARY_V1.md).

## Build

Requirements: Windows, a C11-capable C compiler, and the Windows BCrypt library. Link with `bcrypt` (`-lbcrypt` with MinGW).

CMake is a development/build/test tool, not a runtime dependency. To integrate directly into a Windows C/C++ project, add all implementation `.c` files and supporting private headers from `src/`, add `include/` to the compiler include path, and link BCrypt. Compile implementation files as C; callers include only `obscura64.h`.

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

Test categories cover smoke/link checks, Profile validation and generation, the frozen library, runtime selection, strict codec behavior, public APIs, Providers, state formats, project initialization and reopening, Force Reinitialize/history retention, multi-process locking, and deterministic persistence failure/process-crash handling. Tests use standard C and Windows APIs without a third-party test framework.

## Current Status

This is a Pre-1.0 development snapshot. **Public API may still change before 1.0.** Frozen Profile Library V1 content and ID order must not be casually changed. See [V1 specification](docs/OBSCURA64_V1_SPEC.md), [current state format](docs/PROJECT_STATE_V1.md), and [history format](docs/PROJECT_HISTORY_V1.md).

## Roadmap

Completed: Stage 0 Specification; Stage 1 Codec Core; Stage 2 Profile System; Stage 3 Public API / Provider; Stage 4 Project State / History; Stage 5 Persistence Hardening.

Upcoming:

- Stage 6: redundancy and automatic recovery, including planned project-state backup locations.
- Stage 7: disaster recovery and release preparation, including the planned Managed Payload format.

File-helper APIs and streaming are not implemented. They must not be assumed from the current raw codec API.

## License

[MIT License](LICENSE), Copyright (c) 2026 RXY712200. Third-party projects listed in [references](docs/THIRD_PARTY_REFERENCES.md) are design references, not vendored implementations.
