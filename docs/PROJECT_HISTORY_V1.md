# Obscura64 Project History V1

History resides at `<project>/.obscura64/history.state` and carries the same nonzero 16-byte Project ID as current.state. A different Project ID or invalid existing history causes Force Reinitialize to fail without replacing current state.

The canonical representation is exactly 336 bytes. All integers are explicitly encoded little-endian; no compiler struct layout is persisted.

| Offset | Size | Field | V1 value/rule |
|---:|---:|---|---|
| 0 | 8 | Magic | `OB64HI01` |
| 8 | 2 | Format version | 1 |
| 10 | 2 | Header size | 64 |
| 12 | 4 | Total size | 336 |
| 16 | 4 | Profile Library version | 1 |
| 20 | 2 | Entry size | 80 |
| 22 | 2 | Capacity | 3 |
| 24 | 2 | Entry count | 0 through 3 |
| 26 | 2 | Flags | 0 |
| 28 | 4 | Reserved | All zero |
| 32 | 16 | Project ID | Raw bytes, not all zero |
| 48 | 16 | Reserved | All zero |
| 64 | 80 | Entry 0 | Newest historical generation |
| 144 | 80 | Entry 1 | Second newest |
| 224 | 80 | Entry 2 | Third newest |
| 304 | 32 | SHA-256 | Raw digest of bytes 0 through 303 |

Each entry uses the following offsets within its 80-byte record:

| Offset | Size | Field |
|---:|---:|---|
| 0 | 8 | Generation, unsigned and at least 1 |
| 8 | 64 | Full raw Profile, valid and a member of frozen Library V1 |
| 72 | 8 | Reserved, all zero |

Used entries are ordered by strictly decreasing generation. Repeated Profiles across different generations are permitted. Every unused entry must be entirely zero. The format stores complete Profiles and no Profile IDs.

SHA-256 is calculated with Windows BCrypt over the first 304 bytes. It detects corruption and supports structural integrity checking; it is not attacker-resistant authentication. A writer who knows the format can recompute the digest.

Private validation, serialization, and deserialization enforce exact length, fixed header values, entry count, reserved bytes, unused entries, generation ordering, Profile validity, library membership, and digest. Failed serialization/deserialization leaves the caller's output unchanged.

## V1 retention and commit behavior

Every successful Force retains the most recent three historical generations, or fewer when fewer are available. Entry 0 is the current generation/Profile from before Force, followed by existing history entries in their original newest-first order. When the capacity is exceeded, the oldest generation is discarded. With no overlap, count advances 0 to 1, 1 to 2, 2 to 3, and then stays 3. All unused entries are zero. No sorting or global Profile-uniqueness requirement is applied.

Existing history is validated before any commit, including matching Project ID and strictly descending generations. Each history generation must be below current.generation, except that entry 0 may have the same generation and identical 64-byte Profile as current. That overlap can result from an earlier failed current replacement and is retained only once in the new history. Same generation with a different Profile, a future history generation, or malformed ordering is rejected by managed/structural validation. Stage 6 may repair from a legitimate matching backup; otherwise Force returns UNRECOVERABLE without mutating current/history. The generic History V1 parser does not accept duplicate generations within history.

Force acquires a non-waiting exclusive LockFileEx range on the persistent `<project>/.obscura64/operation.lock`. Active conflicting kernel locks return `OBSCURA64_BUSY`; unlocked file existence is harmless. Normal open uses a shared range lock. Handles are unlocked/closed on exit; the coordination file is not removed.

History is committed and verified before new current state is prepared and replaced. If history cannot be committed, current state stays unchanged. If the current replacement cannot be committed, old current remains, and history may contain a harmless duplicate of it. The next Force normalizes that current/history overlap before committing again; it does not insert the generation twice or evict an extra generation because of the duplicate.

Stage 5.1 centralizes byte persistence in a private Windows module. Replacement creates a unique same-directory `target.tmp.<16 hex digits>` using BCrypt random bytes and `CREATE_NEW`, writes all bytes (looping on partial writes), calls `FlushFileBuffers`, closes, reopens and verifies exact length, exact expected bytes, and the format verifier. Only then does `MoveFileExW(MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)` replace the target, followed by final reopen and verification. Existing authoritative files are never intentionally truncated or rewritten in place. Only a temp created by the current call is cleaned up; unrelated/stale temp-like files are preserved. If final verification fails after replacement, best-effort restoration of the old canonical target is retained; old current Profile remains in already committed history.

This is hardened persistence with tested process-crash windows, not a guarantee against arbitrary storage or controller failures. An interrupted operation may leave a harmless coordination file and unrelated temp artifacts; Windows releases its kernel locking authority and future operations ignore stale temp contents.

Stage 6 permits validated recent-history fallback when exact current recovery is unavailable; see [RECOVERY_V1.md](RECOVERY_V1.md). History promotion may roll back Profile/generation.

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
