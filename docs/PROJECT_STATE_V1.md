# Obscura64 Project State V1

## Purpose and status

Project State identifies a project instance and carries the Profile needed to continue using that project. It is project-level state, not device identity. The Project ID moves with the project and is not derived from hardware or Windows identity.

The canonical 160-byte representation introduced in Stage 4.1 remains unchanged. Project persistence, coordination and recovery are implemented; the completed persistence layer is described below.

Stage 4.2 stores the authoritative current state at `<project>/.obscura64/current.state`. It travels with the project directory. Normal open validates and uses an existing state without changing its bytes. An existing `.obscura64` directory without usable `current.state` is not a fresh project. Stage 6 now attempts validated recovery and returns UNRECOVERABLE when no source succeeds; it never silently generates a replacement identity.

First-write protection uses `CREATE_NEW`, writes exactly 160 bytes, flushes, and rereads the result for validation. A concurrent first initializer loads the winner's state. Stage 5 adds kernel coordination and verified replacement; details and tested limits appear below.

## Logical state

The private logical state contains a 16-byte Project ID, a 64-bit unsigned generation, and the complete 64-byte Profile. Project ID must not be all zero. Generation starts at 1; this format stage does not generate IDs or increment generations. Profile must be valid and belong to Frozen Profile Library V1. The serialized state does not store a Profile ID.

## Canonical representation

All integer fields use little-endian encoding. The representation is exactly 160 bytes; implementations must use explicit byte offsets and must not dump a compiler-layout struct.

| Offset | Size | Field | V1 value or rule |
|---:|---:|---|---|
| 0 | 8 | Magic | ASCII bytes `OB64ST01` |
| 8 | 2 | Format version | 1 |
| 10 | 2 | Header size | 64 |
| 12 | 4 | Total size | 160 |
| 16 | 4 | Profile Library version | 1 |
| 20 | 2 | Profile size | 64 |
| 22 | 2 | Flags | 0; other values rejected |
| 24 | 8 | Generation | Unsigned, at least 1 |
| 32 | 16 | Project ID | Raw nonzero bytes; no textual GUID encoding |
| 48 | 16 | Reserved | Writer emits zero; parser requires zero |
| 64 | 64 | Profile | Complete raw Profile bytes |
| 128 | 32 | SHA-256 | Digest of bytes 0 through 127 |

The parser accepts exactly 160 bytes and checks all fixed fields, reserved bytes, Project ID, generation, digest, Profile validity, and Profile Library V1 membership before updating its output state.

## Integrity semantics

SHA-256 is computed with Windows BCrypt over the first 128 bytes. It detects accidental corruption and supports structural integrity checking. It is not secret authentication or protection from an attacker who can modify the blob and recompute its digest.

## Persistence boundary

### Explicit Force Reinitialize

`obscura64_force_reinitialize` and its custom Provider variant require existing valid current state. They keep the Project ID unchanged, increment generation by exactly one, and select a different frozen Profile. `UINT64_MAX` is rejected with `OBSCURA64_SIZE_OVERFLOW` before changing either state file. Force is never automatic and never repairs missing or corrupt current state.

The previous current generation/Profile is committed to `history.state` before current.state is replaced. V1 retains at most the latest three historical generations in newest-first order and discards the oldest when full. A matching current/history overlap left by a failed current replacement is normalized on the next Force. The unchanged History V1 format and validation rules are documented in `PROJECT_HISTORY_V1.md`.

Existing contexts retain their old Profile. After Force, callers should destroy old contexts when finished and use the newly returned context as the current project context. There is no context registry or automatic destruction.

Force uses the persistent kernel-lock coordination and verified replacement described below. Stage 6 now permits validated history fallback as documented in RECOVERY_V1.md.

Format serialization is separate from disk I/O. The completed persistence layer is described below.

### Stage 5.1 verified byte persistence

State V1 (160 bytes) and History V1 (336 bytes) formats remain unchanged. The private persistence module owns Wide Win32 byte I/O; serialization and semantic checks remain in the State/History modules via private verifier callbacks.

Replacement sequence: unique same-directory temp with BCrypt suffix and CREATE_NEW -> exact write with partial-write loop -> FlushFileBuffers -> close -> reopen and verify exact size/bytes and format -> MoveFileExW with MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH -> final reopen and verify. Existing authoritative files are never intentionally truncated or rewritten in place. Only temps created by this invocation are removed; unrelated and stale temp-like files are preserved. Post-replacement failure is reported, with the prior best-effort restoration protection retained.

First initialization still creates the authoritative file with CREATE_NEW, then writes, flushes, closes and verifies it. A losing initializer reads the winning state using the existing bounded retry behavior; it never overwrites the winner. Force still commits verified history before preparing/replacing current and retains the previous Profile on current replacement failure.

This improves write/replacement reliability but does not prove absolute power-loss durability under every filesystem, device or controller failure. Stage 5.1 originally retained the Force existence lock; it is superseded by the completed kernel coordination described below. Stage 5.1 alone added no automatic recovery; Stage 6 recovery is described below.

### Stage 5.2 managed-state consistency

`current.state` remains authoritative for `obscura64_open` and `obscura64_open_with_provider`. A valid current file is sufficient for normal open and decoding current application data, even if history is missing, corrupt, or semantically inconsistent. Normal open does not repair, rewrite or synthesize history.

Force uses a private cross-file validator after loading current/history under the existing operation lock and before writing either authoritative file. Existing format validators still own binary parsing, hashes and Profile validation; the byte persistence layer is unchanged. Current State V1 remains 160 bytes and History V1 remains 336 bytes, with unchanged magic, versions and layout.

For current generation N, a steady history has exactly min(N-1, 3) entries: N-1, N-2, N-3, stopping at generation 1. Generation 1 permits absent history or an existing valid empty history. The private managed validator diagnoses absent required history as STATE_MISSING and empty/incomplete/gapped windows as STATE_CORRUPT. Stage 6 Force attempts legitimate repair and returns UNRECOVERABLE if no mutation-safe state can be reconstructed. Project IDs must match byte-for-byte. Future generations and same-generation conflicting Profiles return STATE_CORRUPT.

A safe transitional overlap has exactly min(N, 3) entries: N, N-1, N-2, stopping at generation 1. Entry 0 must match both current generation and all 64 Profile bytes. The private validator reports steady/overlap; the existing history merge consumes that classification and includes an overlap only once. Incomplete overlap windows are rejected. Profiles may repeat across different, nonadjacent generations; there is no global Profile uniqueness rule.

The generic History V1 parser continues to accept strictly descending, structurally valid non-contiguous generations such as 7,5,3. The managed validator rejects that history for current generation 8. Invalid sets leave current/history bytes unchanged, with no owned persistence temp left behind. Stage 5.2 adds detection, not repair or recovery. Stage 5 locking and crash/fault tests are described below; completed Stage 6 adds the recovery policy in RECOVERY_V1.md.

### Stage 5 completed: locking and tested failure windows

Responsibilities remain separate: State/History modules own binary layouts and structural validation; managed-state owns cross-file consistency; persistence owns exact byte I/O and verified replacement; lock owns Windows coordination; project orchestrates operations. No public API or binary format changed.

The persistent `.obscura64/operation.lock` is opened with OPEN_ALWAYS and FILE_SHARE_READ | FILE_SHARE_WRITE. Delete sharing is denied while handles are active to keep one stable coordination object. LockFileEx locks byte offset 0, length 1 using a zeroed OVERLAPPED for normal readers and Force. First initialization exclusively locks bytes 0 and 1 together: byte 1 is a transient kernel-only initialization marker, not file contents or ownership metadata. A non-waiting marker probe distinguishes initialization races from ordinary contention. Normal open holds a shared lock through current read/validation and Context construction. Force holds an exclusive lock through both file commits and returned Context preparation. Both modes use LOCKFILE_FAIL_IMMEDIATELY; contention returns BUSY. Lock acquisition itself never waits. Only first-init coordination retains a bounded race grace (at most twenty-four 10ms delays, 240ms total) so concurrent initialization can load one CREATE_NEW winner. An already-existing state container without current is not silently initialized.

UnlockFileEx and CloseHandle run on every acquired-lock exit path. The file stays empty and persistent, without PID, timestamp or identity metadata; file existence does not imply ownership. Process termination releases kernel locks, though Windows may take time to release them under resource pressure. A release error is reported as IO_ERROR and no returned Context is published; a completed mutation is not rewritten merely because unlock failed. See [LockFileEx](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-lockfileex).

Ordinary builds contain no failure callback or global fault state. Only the dedicated failure test compiles OBSCURA64_TESTING and supplies one-shot hooks at temp creation/write/flush/reopen, pre-verification, replacement, final reopen/post-verification and post-replacement observation. Tests cover history and current failure separately, rollback verification, history-success/current-failure overlap, repeated retry retention, and abrupt termination of a child paused before replacement, between history/current commits, and after current replacement. These are process-crash and deterministic failure tests, not hardware power-failure certification.

Verified same-directory replacement, FlushFileBuffers, and MOVEFILE_WRITE_THROUGH remain required. No existing authoritative file is intentionally truncated or rewritten in place. Current remains authoritative for normal open; bad/missing history blocks mutation as specified above. Safe overlap is normalized only during an explicitly requested Force. Corrupt current is not regenerated; stale temp artifacts are neither authoritative nor recovery candidates and are preserved unless owned by the invocation.

Directory durability limitation: the local Windows probe could open and flush a directory with GENERIC_READ | GENERIC_WRITE and FILE_FLAG_BACKUP_SEMANTICS; read-only directory flush failed with ERROR_ACCESS_DENIED. This observation does not establish a portable directory-metadata durability contract across Windows filesystems. No mandatory directory-handle flush or privileged volume flush is added: it would introduce additional access/compatibility requirements without proving transactional or controller-level durability. File-level flush and write-through replacement are retained. See [FlushFileBuffers](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-flushfilebuffers) and [directory handles](https://learn.microsoft.com/en-us/windows/win32/fileio/obtaining-a-handle-to-a-directory).

Stage 5 is complete within these tested Windows persistence guarantees. Stage 6 now adds per-user LocalAppData redundancy and automatic recovery; see RECOVERY_V1.md. No ProgramData or Registry copies are implemented. Stage 7 Managed Payload and manual disaster recovery are complete; V1 release preparation is complete.

### Stage 6 redundancy and recovery

Valid project current remains highest authority. Missing/corrupt current in an existing container triggers exclusive-lock recovery from exact LocalAppData current, then validated project/backup history. A fresh container absence still initializes a new identity. Normal open can remain available with unrepaired history; Force requires a mutation-safe set or returns OBSCURA64_UNRECOVERABLE. Existing State/History binary formats are unchanged. See [RECOVERY_V1.md](RECOVERY_V1.md) for identity conflict rules, partial recovery, path-local mirrors, repair ordering, and rollback limitations. Stage 6 is complete; Stage 7 Managed Payload and manual disaster recovery are complete; V1 release preparation is complete.
