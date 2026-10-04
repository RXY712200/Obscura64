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

Existing history is validated before any commit, including matching Project ID and strictly descending generations. Each history generation must be below current.generation, except that entry 0 may have the same generation and identical 64-byte Profile as current. That overlap can result from an earlier failed current replacement and is retained only once in the new history. Same generation with a different Profile, a future history generation, or malformed ordering is rejected as STATE_CORRUPT without rewriting either authoritative file. The generic History V1 parser does not accept duplicate generations within history.

Force acquires `<project>/.obscura64/operation.lock` with `CREATE_NEW`. An existing lock returns `OBSCURA64_BUSY`; there is no indefinite wait. Only the call that created the lock removes it, on both success and failure.

History is committed and verified before new current state is prepared and replaced. If history cannot be committed, current state stays unchanged. If the current replacement cannot be committed, old current remains, and history may contain a harmless duplicate of it. The next Force normalizes that current/history overlap before committing again; it does not insert the generation twice or evict an extra generation because of the duplicate.

The Force-only basic replacement writes `target.tmp` with `CREATE_NEW`, flushes and closes it, rereads and validates it, replaces the target with `MoveFileExW(MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)`, and rereads/validates the final target. A temporary file created by this call is removed on a pre-replacement failure. A pre-existing temp file is left untouched and causes an I/O error. If final verification fails after replacement, restoration of the old canonical target is attempted; the old Profile is still retained in the committed history for current-state replacement.

This is basic persistence, not a guarantee against arbitrary storage failures or process interruption. An interrupted operation can leave its lock/temp behind; stale-file handling, complete locking, crash testing, and persistence hardening belong to Stage 5.

History is currently saved only. Normal open does not use it to recover missing or corrupt current state; automatic recovery remains Stage 6 work.
