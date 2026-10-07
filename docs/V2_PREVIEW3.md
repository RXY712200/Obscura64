# V2 Preview 3 — Reliable Files

Development version: `2.0.0-preview.3`. This layer is independent of V1
project state, Project ID, generation, and LocalAppData mirrors.

## API and paths

`obscura64_write_file(path_utf8, protection, input, input_len)` stores one
protected file. `obscura64_read_file_alloc` returns application bytes; free a
successful allocation with `obscura64_free`. Optional
`obscura64_read_file_alloc_ex` reports the successful `PRIMARY` or `BACKUP`
source and `V2` or `V1_LEGACY_UPGRADE_RECOMMENDED` format. Failure clears all
outputs; an empty application payload returns NULL and zero length.

Paths are NUL-terminated UTF-8 **absolute Windows target file paths**, strictly
converted to UTF-16. Drive-absolute and UNC paths are accepted. Relative and
device paths, DOS device names, alternate data streams, trailing dot/space components, and owned
sidecar names are rejected. Accepted paths are normalized by `GetFullPathNameW`
before sidecar derivation. No extension is required and parent directories must
already exist. Windows path-length behavior depends on the caller/OS long-path
configuration. Symlinks, hard links, and 8.3 short names can create aliases; callers should use
one stable target spelling for coordination. External writers that ignore the
Obscura64 lock protocol are not coordinated.

For target `<target>`, the owned sidecars are `<target>.ob64.bak` and
`<target>.ob64.lock`. Temporary replacements are adjacent, named
`<path>.tmp.<16 random hex digits>`. Targets ending in either sidecar suffix
or the temporary form are rejected without case sensitivity. Do not use an
Obscura64 sidecar as another independent target.

## One-backup policy

Under one exclusive lock, the new application data is fully protected in
memory. If the old primary fully unprotects, its **exact protected bytes** are
written and verified as backup before replacing primary. After success,
primary is new and backup is the prior valid primary. If primary is absent or
has a replaceable data error while backup validates, that backup is kept
unchanged and only primary is replaced. If neither validates and both are
replaceable or absent, the new protected bytes are first written
and verified as backup, then written as primary. Thus a first successful write
already has one valid fallback. No corrupt primary bytes are promoted.

A primary with an unsupported future V2 version or protection kind is not
overwritten. Hard allocation, size, filesystem, or internal legacy-scan errors
also stop the write. `PROTECTION_FAILURE` is not proof of corruption: a
CurrentUser DPAPI blob may simply belong to another Windows protection
context. `LEGACY_AMBIGUOUS` likewise does not establish disposable bytes.
Either status on primary stops ordinary write before changing primary **or
backup**, even when backup validates. If primary is missing or replaceable but
backup returns either status, write preserves backup and fails. The same rule
preserves an unsupported backup needed for the decision.
Builtin V1 Managed Payload can be read; an explicit write over a valid legacy
primary copies its exact V1 protected bytes into backup and writes a V2 primary.
Custom V1 Provider payloads are not automatically supported. Read never
upgrades legacy data; explicit upgrade is Preview 4 work.

### Explicit file status policies

| Status | Read may try backup? | Ordinary write may replace this copy? |
| --- | --- | --- |
| `FILE_NOT_FOUND` | Yes | Yes; there are no bytes to discard |
| `ENVELOPE_CORRUPT`, `CASUAL_CORRUPT`, `PROTECTED_CORRUPT`, `UNRECOGNIZED_DATA` | Yes | Yes |
| `PROTECTION_FAILURE`, `LEGACY_AMBIGUOUS` | Yes | **No**; valid data may be inaccessible or ambiguous |
| `UNSUPPORTED_VERSION`, `UNSUPPORTED_PROTECTION` | No | No |
| `INVALID_ARGUMENT`, `OUT_OF_MEMORY`, `SIZE_OVERFLOW`, `IO_ERROR`, `LEGACY_SCAN_FAILURE` | No | No |
| All other or future statuses | No | No |

The read and write policies are separate private pure functions with
conservative defaults. Read fallback does not mutate either copy. Ordinary
write cannot infer permission to discard bytes from an inability to decrypt
them. Callers who intentionally discard inaccessible data must explicitly
remove or rename it outside this API; Preview 3 has no force-overwrite entry.

## Replacement and failure semantics

The V1 same-directory engine is reused through a variable-length private
entry point. It creates an owned random temp, writes exact bytes, calls
`FlushFileBuffers`, closes, reopens, and verifies exact bytes **and successful
unprotect**. It then calls `MoveFileExW` with `MOVEFILE_REPLACE_EXISTING |
MOVEFILE_WRITE_THROUGH`, and reopens/verifies the final name. Backup is
validated before primary mutation. Failure before the primary rename leaves
the old primary unchanged. A detected failure after rename attempts a verified
primary restore from validated backup under the same lock, never modifying the
backup. An API failure may leave either old or new valid primary. A failed
backup replacement does not advance primary.

This strategy does not promise universal power-loss durability, directory
fsync, or storage-controller guarantees. Abrupt termination can leave old or
new valid state at a completed rename point. Stale temps are ignored on read;
there is no arbitrary temp-file recovery scan or automatic read repair.

## Read and concurrency

Readers take a shared nonblocking kernel byte-range lock; writers take an
exclusive one. Multiple readers coexist. Contention returns `OBSCURA64_BUSY`
without waiting. The persistent lock file is not ownership metadata: process
handle cleanup releases its kernel lock. One lock covers primary, backup, and
their temps.

Primary is read first. Successful unprotect returns `PRIMARY` and does not
inspect backup. Missing or genuinely invalid primary data permits fallback:
malformed envelope, Casual/inner corruption, unrecognized data, protection
failure (including CurrentUser DPAPI context mismatch), or legacy ambiguity.
Unsupported version/protection, invalid arguments, allocation/size/I/O errors,
and internal legacy-scan errors do **not** fallback. An older library must not
silently roll back a future V2 primary. CurrentUser is bound to the Windows
protection context and is not portable across users or machines.

A valid backup returns success with `BACKUP`. Both absent returns
`OBSCURA64_FILE_NOT_FOUND`. Invalid primary plus absent/invalid backup returns
the primary data error; absent primary plus invalid backup returns the backup
error. A hard error required for the decision takes precedence. Read never
repairs disk.

## Win32 API choice and scope

The design retains V1's `MoveFileExW` replacement pattern. Microsoft documents
the chosen `MOVEFILE_REPLACE_EXISTING` and `MOVEFILE_WRITE_THROUGH` flags, but
documents `REPLACEFILE_WRITE_THROUGH` on `ReplaceFileW` as **unsupported**.
`ReplaceFileW`'s backup option also does not provide the required full backup
validation before primary mutation. No collections, schemas, transactions,
autosave, snapshots, cloud synchronization, CLI inspect/verify/upgrade, or
automatic legacy migration are included. The latter CLI/upgrade workflows are
deferred to Preview 4.

Filesystem identity aliases (hard links, symlinks/reparse points, and 8.3 short
names) can derive distinct sidecars from different path spellings. Callers
should keep one stable target spelling. Alias-aware coordination is deferred
to Preview 5 filesystem hardening; Preview 3 does not claim it.

Microsoft references: [MoveFileExW](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-movefileexw)
and [ReplaceFileW](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-replacefilew).
