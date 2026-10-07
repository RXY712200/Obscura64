# Obscura64 V2 guide

Development version: `2.0.0-preview.5`. This is a Windows C11 library for the
lifecycle of protected local application blobs. The stable published release
remains v1.0.0 until V2 independent review and release.

## Choose a protection semantic

| Choice | Meaning | Limits |
| --- | --- | --- |
| `NONE` | Versioned representation of plain bytes | No confidentiality, integrity, or authentication |
| `CASUAL` | Reversible obfuscation with corruption validation | Not encryption or attacker authentication; a knowledgeable writer can create valid data |
| `CURRENT_USER` | Windows DPAPI protection for the current user, plus an inner Obscura64 corruption check | Bound to Windows credentials/machine conditions; not portable or server authority; an authorized local process can create valid data |

Protection is always explicit. Use `obscura64_protect_alloc` and
`obscura64_unprotect_alloc` for byte buffers. Inputs carry explicit lengths;
`NULL` input is accepted only at length zero. Successful allocated outputs
belong to the caller and must be released with `obscura64_free`. Failure clears
allocation outputs. Caller-buffer forms return `BUFFER_TOO_SMALL` and the
required size without changing caller bytes. A size query performs the backend
operation and a later retry can differ in size; allocation-first calls avoid
this problem. `unprotect_alloc_ex` optionally reports V2 or supported V1
legacy format on success.

Use `obscura64_write_file` and `obscura64_read_file_alloc` for a protected
local file. `read_file_alloc_ex` also reports whether the primary or adjacent
backup supplied data and its format. A write keeps one validated adjacent
`*.ob64.bak`; reads fall back conservatively. A successful read does not repair
the primary. `obscura64_upgrade_file` explicitly converts supported V1 data
or changes the V2 protection choice under one exclusive lock; already-current
V2 primary data is a no-op. Future V2 versions and unknown protection kinds
are preserved rather than overwritten automatically.

## Paths and concurrency

Library file APIs require a full, NUL-terminated UTF-8 Windows path and an
existing parent directory. Relative paths are rejected; the maintenance CLI
resolves its relative input paths before calling the library. ADS, DOS device
names, trailing spaces/dots, device namespaces, and sidecar-owned names are
rejected. Ordinary `.` and `..` components normalize before use. Existing
parents are resolved by handle before sidecar names are derived. Existing
targets are resolved by handle to normalize ordinary/8.3 spelling; target
reparse points and multi-hard-link targets are rejected before mutation with
`INVALID_ARGUMENT`. Parent directory junctions/symlinks resolve to their
target when Windows permits traversal. Missing parent directories preserve
the usual file-not-found/IO behavior. This is coordination for ordinary
applications, not a defense against another process maliciously changing the
filesystem during an operation.

Independent V2 memory calls may run concurrently; protection selection uses
no mutable global application state. Windows supplies RNG and DPAPI. V1
contexts are immutable after construction and can be read concurrently while
they remain alive. A custom Provider must make its callbacks and `user_data`
thread-safe for concurrent calls; destroying a context while it is in use is
the caller's responsibility. File readers take a shared nonblocking lock;
writers and upgrades take an exclusive nonblocking lock on the same canonical
sidecar. A conflict returns `BUSY`. External writers ignoring this protocol
are not coordinated.

## Build and integration

Compile the C sources directly into a Windows C/C++ project, or use CMake:

```text
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
cmake --install build --config Release --prefix <prefix>
```

`BUILD_TESTING=OFF` excludes the formal suite. `OBSCURA64_BUILD_TOOLS` defaults
to ON, `OBSCURA64_INSTALL_TOOLS` defaults to OFF, and
`OBSCURA64_BUILD_EXAMPLES` defaults to OFF. The installed static package
provides `Obscura64::obscura64` through `find_package(Obscura64 CONFIG REQUIRED)`.
The package's full development identity is `2.0.0-preview.5`; CMake's numeric
project version is `2.0.0` for compatibility. CMake is not a runtime dependency.

## Maintenance and compatibility

`obscura64 inspect`, `verify`, and `upgrade <file> --to none|casual|current-user`
are explicit maintenance commands. The separate `obscura64_recover` tool is
for builtin V1 Managed Payload disaster recovery, not general V2 repair.
Supported V1 builtin Managed Payload can be read/migrated; custom V1 Provider
data still requires its V1 context and is not automatically discovered. V1
Project ID, generation, Provider ABI, and wire formats remain unchanged.

Verified replacement and flushing reduce ordinary process-failure windows but
do not promise survival of arbitrary hardware power loss. The CurrentUser
representation depends on Windows DPAPI credentials and machine conditions.
Neither V1 Managed nor Casual public digests authenticate a hostile writer.

See the [V2 format history](V2_PREVIEW1.md), [protection details](V2_PREVIEW2.md),
[file policy](V2_PREVIEW3.md), and [upgrade policy](V2_PREVIEW4.md) for deeper
implementation contracts.
