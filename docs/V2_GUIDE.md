# Obscura64 V2 — user guide

**Version 2.0.0 (stable)** · Windows · C11 · MIT

Obscura64 manages **local application blobs**, not application schemas. The stable V2 API exposes versioned byte protection and a small reliable protected-file lifecycle. V1 remains supported through its existing public APIs and a limited builtin Managed Payload compatibility path.

## Protection semantics

| Mode | Stored data | Validation | Limit |
| --- | --- | --- | --- |
| `OBSCURA64_PROTECTION_NONE` | Plain bytes inside the V2 envelope | Canonical envelope and exact length only | No confidentiality, integrity, or authentication |
| `OBSCURA64_PROTECTION_CASUAL` | Reversible Profile-based obfuscation | Strict format and public SHA-256 corruption check | Not encryption or attacker authentication |
| `OBSCURA64_PROTECTION_CURRENT_USER` | DPAPI current-user protected inner record | DPAPI unprotect plus strict inner-format and digest checks | Not portable; not authoritative against an authorized local process |

These modes express different requirements, **not** a monotonic security ranking. Each protect/write/upgrade call requires explicit mode selection. The library does not choose a default or manage passwords and keys.

## Memory API

```c
void *wrapped = NULL, *restored = NULL;
size_t wrapped_size = 0, restored_size = 0;

obscura64_status status = obscura64_protect_alloc(
    OBSCURA64_PROTECTION_CURRENT_USER, input, input_size,
    &wrapped, &wrapped_size);
if (status == OBSCURA64_OK)
    status = obscura64_unprotect_alloc(wrapped, wrapped_size,
        &restored, &restored_size);

/* Check status and restored_size before consuming restored. */
obscura64_free(restored);
obscura64_free(wrapped);
```

All byte APIs take explicit lengths and support embedded NUL bytes. `NULL` input is only valid with zero length. Allocating failure clears the pointer and size; free every allocated result with `obscura64_free`. Caller-buffer functions do not change caller output on failure. The `BUFFER_TOO_SMALL` required size is calculated by the actual protection operation; a later retry may produce a different backend length, so allocation-first calls are recommended. `unprotect_alloc_ex` optionally returns whether the result is V2 or supported builtin V1 legacy.

V2 has one 24-byte self-describing outer envelope (`OB64ENV2`, version 2, header size, protection kind, zero flags, exact body length). Parsing is strict. The Casual format and CurrentUser inner record are versioned. V2 magic is authoritative: malformed or future V2 input is not guessed to be V1.

## Reliable protected files

```c
obscura64_write_file(absolute_utf8_path,
    OBSCURA64_PROTECTION_CASUAL, input, input_size);
obscura64_read_file_alloc(absolute_utf8_path, &output, &output_size);
```

The API takes a **full NUL-terminated UTF-8 Windows path** to one target. Parent directories must already exist. The library controls two deterministic adjacent sidecars: `<target>.ob64.bak` (one validated prior/recovery representation) and `<target>.ob64.lock` (persistent coordination file). Randomly named same-directory temporary files are used only for verified replacement.

Under one nonblocking exclusive lock, writing first creates and validates protected bytes in memory, then ensures a validated backup exists before changing primary:

- If primary is valid, backup receives the **exact old protected primary bytes**.
- If primary is missing/corrupt and backup is valid, that backup is preserved.
- If neither copy is valid, backup first receives the **new protected bytes** before primary is created or replaced.

Replacement writes/flushed temporary bytes, reopens them for byte-for-byte comparison and full Obscura64 validation, uses Windows `MoveFileExW` replacement, then reopens the destination for verification. After a detected post-replacement failure it attempts best-effort restore from validated backup. Crash outcomes may leave old or new valid bytes; arbitrary controller/hardware power-loss durability is **not** promised.

Reading takes a shared lock and returns validated primary preferentially. Certain invalid/missing primary conditions permit validated backup fallback; normal read never repairs disk. `read_file_alloc_ex` reports successful PRIMARY/BACKUP source and V2/V1 format.

**Destructive writes are more conservative than read fallback.** Unsupported future V2 formats, DPAPI `PROTECTION_FAILURE`, ambiguous legacy matches, hard I/O/allocation/size failures, and unknown status values do **not** grant permission to replace a potentially valid file. They are not automatically treated as corrupted data.

Readers may coexist; writes/upgrades require the exclusive lock. Contention returns `OBSCURA64_BUSY` immediately. Kernel ownership is released when the process exits; the persistent lock file itself does not indicate ownership.

## Paths, aliases, and concurrency

Relative file paths are rejected by the library; the CLI resolves relative command-line input. Alternate data streams, DOS device names, ambiguous trailing dots/spaces, device namespaces, and reserved sidecar/temp names are rejected. Existing parent and target paths are resolved through Windows handles before deriving sidecars. Existing multi-hard-link target files and target reparse points are rejected. Parent junctions/symlinks resolve when Windows permits traversal. Coverage of 8.3 and symbolic-link creation depends on OS settings and was skipped in the release-candidate local environment; see the validation record.

Independent memory operations are reentrant. Immutable V1 context reads can run concurrently while the context remains alive; custom Provider callbacks and their caller-owned `user_data` must be thread-safe when used concurrently. Never destroy a context while it is in use. The file lock coordinates cooperating library operations, **not hostile or unrelated processes** that ignore the lock. No external filesystem sandbox is provided.

## Upgrade and CLI

`obscura64_upgrade_file(path, target_mode)` is an explicit, single-lock in-place maintenance operation. A valid primary is preferred; a validated backup can supply data only when the primary is **safely replaceable-invalid or missing**. On primary upgrade, the exact original protected primary becomes backup; on backup-source upgrade the backup stays unchanged. A current V2 primary already using the selected mode is an untouched no-op. The library never silently reduces or increases protection.

```text
obscura64 inspect <file>
obscura64 verify <file>
obscura64 upgrade <file> --to none|casual|current-user
```

`inspect` reports structural format/semantic of **exactly** the named file; for V2 it can identify CURRENT_USER without decrypting it. `verify` validates **exactly** the named file, with no backup fallback and no plaintext output. NONE verifies structure only; CASUAL and V1 verify public corruption digests, not authenticity; CURRENT_USER requires the current Windows DPAPI context plus a valid inner record. `upgrade` executes the locked file operation. Exit codes: 0 success, 1 data/operation failure, 2 usage error. The CLI accepts relative or absolute Unicode Windows command-line paths; the library file API still requires full UTF-8 paths.

## Supported V1 compatibility

Builtin V1 Managed Payload may be recognized on unprotect/read and explicitly migrated/upgraded into V2. Arbitrary custom V1 Provider data **cannot** be scanned automatically and must use its original V1 context. V1 State (160 bytes), History (336 bytes), Managed Payload, Project IDs/generations, the Provider ABI 1, and the frozen 4096-Profile Library remain unchanged. The original `obscura64_recover` disaster-recovery CLI remains separate.

## Build, install, and validation

On Windows with CMake 3.16+:

```powershell
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
cmake --install build --config Release --prefix "C:/obscura64-install"
```

External C/C++ consumers use `find_package(Obscura64 CONFIG REQUIRED)` and link `Obscura64::obscura64`. `CMAKE_PREFIX_PATH` must include the install prefix. `BUILD_TESTING=OFF` omits the formal suite; `OBSCURA64_BUILD_TOOLS` controls CLI builds; `OBSCURA64_INSTALL_TOOLS` opts into tool installation; `OBSCURA64_BUILD_EXAMPLES` enables examples. Direct source integration is supported; compile library `.c` files as C, link `bcrypt crypt32 shell32 ole32 uuid`, and expose only `obscura64.h` to consumers.

Release-candidate evidence: MSVC x64/Win32 and clang-cl x64 CI, MinGW GCC i686/x64, all 35 CTest cases in each reported configuration, external C/C++11 package consumers, selected AddressSanitizer, and 100,000 deterministic parser mutations per mutation run. Two MSVC static-analysis C6386 bounded-write reports remain reviewed as false positives; test/tool CRT deprecation warnings remain. Tests that require 8.3/symlink creation were skipped where unavailable; isolated temp-directory cleanup reruns passed. These limits are not omitted from the [validation record](RELEASE_CHECKLIST_V2.md).

For historical implementation details see [Preview 1](V2_PREVIEW1.md), [Preview 2](V2_PREVIEW2.md), [Preview 3](V2_PREVIEW3.md), and [Preview 4](V2_PREVIEW4.md). Those are archived development milestones, not competing current-user manuals.

## Security statement

Neither local obfuscation nor DPAPI makes data an authorization source or server trust boundary. CASUAL is not encryption. The CurrentUser inner SHA-256 verifies decoded record corruption after DPAPI; it does not authenticate an authorized local writer. Obscura64 does not offer DRM, anti-cheat guarantees, cross-platform secret management, streaming, or arbitrary power-loss recovery.

See [SECURITY.md](../SECURITY.md) for reporting.
