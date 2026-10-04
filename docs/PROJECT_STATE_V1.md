# Obscura64 Project State V1

## Purpose and status

Project State identifies a project instance and carries the Profile needed to continue using that project. It is project-level state, not device identity. The Project ID moves with the project and is not derived from hardware or Windows identity.

Stage 4.1 defines and implements only the in-memory logical state and canonical 160-byte representation. It does not write a state file. Durable and atomic persistence is deferred to a later stage.

Stage 4.2 stores the authoritative current state at `<project>/.obscura64/current.state`. It travels with the project directory. Normal open validates and uses an existing state without changing its bytes. An existing `.obscura64` directory without `current.state` is treated as missing state, not as a fresh project. Corrupt state is never silently replaced.

First-write protection in Stage 4.2 is basic: the writer uses `CREATE_NEW`, writes exactly 160 bytes, flushes, and rereads the result for validation. A concurrent first initializer that loses the create race loads the winner's state. Stronger atomic persistence and full process locking belong to Stage 5.

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

Force uses a minimal CREATE_NEW `operation.lock` and basic temp-write, flush, reread, replace, final-reread persistence. This does not implement Stage 5's complete locking or crash hardening. History is not currently an automatic recovery source.

No project state file, project path, temporary file, or file replacement is implemented in Stage 4.1. Durable and atomic persistence is deferred to later stages.

### Stage 5.1 verified byte persistence

State V1 (160 bytes) and History V1 (336 bytes) formats remain unchanged. The private persistence module owns Wide Win32 byte I/O; serialization and semantic checks remain in the State/History modules via private verifier callbacks.

Replacement sequence: unique same-directory temp with BCrypt suffix and CREATE_NEW -> exact write with partial-write loop -> FlushFileBuffers -> close -> reopen and verify exact size/bytes and format -> MoveFileExW with MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH -> final reopen and verify. Existing authoritative files are never intentionally truncated or rewritten in place. Only temps created by this invocation are removed; unrelated and stale temp-like files are preserved. Post-replacement failure is reported, with the prior best-effort restoration protection retained.

First initialization still creates the authoritative file with CREATE_NEW, then writes, flushes, closes and verifies it. A losing initializer reads the winning state using the existing bounded retry behavior; it never overwrites the winner. Force still commits verified history before preparing/replacing current and retains the previous Profile on current replacement failure.

This improves write/replacement reliability but does not prove absolute power-loss durability under every filesystem, device or controller failure. The existing Force operation.lock is unchanged. Full multi-process locking belongs to Stage 5.3; crash/fault simulations belong to Stage 5.4. No automatic recovery is added; that remains Stage 6 work.
