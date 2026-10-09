> **Historical Preview 1 development record.** For stable Obscura64 v2.0.0, see the [current V2 guide](V2_GUIDE.md) and [release notes](RELEASE_NOTES_V2.0.0.md). Future-work statements below describe the state at this historical milestone.

# Obscura64 2.0.0-preview.1

This records the tagged Preview 1 baseline. For current Preview 2 behavior,
see [V2 Preview 2](V2_PREVIEW2.md).

Preview 1 establishes a byte-oriented V2 envelope and ordinary `protect` /
`unprotect` API. The product remains a local application-data obfuscation and
protection component, not a serializer, database, backup product, or encryption
framework. V1.0.0 and its tag remain immutable historical releases.

## Canonical V2 envelope

All multi-byte integers are unsigned little-endian. Serialization writes each
field explicitly; no C struct layout or host packing is used. The total size is
exactly `24 + body_length`, with checked arithmetic and no trailing bytes.

| Offset | Bytes | Field | Preview 1 value |
| --- | ---: | --- | --- |
| 0 | 8 | Magic | ASCII `OB64ENV2` |
| 8 | 2 | Format version | 2 |
| 10 | 2 | Header size | 24 |
| 12 | 2 | Protection kind | 0: explicit NONE/plain body; 1 and 2 reserved for Preview 2 |
| 14 | 2 | Flags | 0; every unknown bit is rejected |
| 16 | 8 | Protected body length | Exact byte length |
| 24 | body length | Protected body | Arbitrary bytes |

There are no reserved fields or application-schema metadata. Version 2 is a
wire-format version, independent of the `2.0.0-preview.1` development version.
Kind 0 is an explicit **unprotected NONE representation**. It is deliberately
retained as a supported opt-in semantic, not a default: it enables exact
envelope interoperability and tests without implying protection. It has no
confidentiality, integrity, or authentication. Kind 1 is reserved for Casual;
kind 2 is reserved for Windows CurrentUser. Both return
`OBSCURA64_UNSUPPORTED_PROTECTION` until Preview 2 implements them. Their
backend body layouts are not specified in Preview 1. Unknown kinds return
`OBSCURA64_UNSUPPORTED_PROTECTION`. A wrong magic, header size,
flags, truncated input, length mismatch, or trailing data returns
`OBSCURA64_ENVELOPE_CORRUPT`. Other format versions return
`OBSCURA64_UNSUPPORTED_VERSION`. Unrepresentable length returns
`OBSCURA64_SIZE_OVERFLOW`. These are structural diagnostics, not attacker
attribution.

## Ordinary API

`obscura64_protect_alloc(protection, input, length, ...)` and
`obscura64_unprotect_alloc(envelope, length, ...)` are the primary operations.
Protection is a small semantic enum: `NONE` works now, while `CASUAL` and
`CURRENT_USER` return `OBSCURA64_UNSUPPORTED_PROTECTION` until Preview 2.
There is **no implicit default**. New data requires an explicit choice;
unprotect learns the kind from the envelope. Already-written envelopes retain
their recorded kind when future recommended choices change. No algorithm,
DPAPI flag, key, project path, Profile, generation, or Provider is exposed.

The caller-buffer `obscura64_protect` and `obscura64_unprotect` forms perform
the operation into library-owned temporary storage, then copy on success.
`BUFFER_TOO_SMALL` reports the **actual** required size after the operation.
A `NULL` output with zero capacity invokes that same operation to query size;
it is not a cheap arithmetic calculation and may fail for backend reasons.
There is no generic plaintext-length-only protected-size API and no generic
pre-decryption plaintext-size API. Future DPAPI results can be wrapped after
`CryptProtectData` or `CryptUnprotectData` returns its allocated blob, without
an undocumented size formula or an unauthenticated plaintext-length hint.

Every byte array has an explicit length. `NULL` input is accepted only at zero
length. The 24-byte empty NONE envelope is valid; allocating empty unprotect
returns `NULL` and length zero. Caller-buffer failure leaves output bytes
unchanged; non-size failures report zero length. Allocating failures clear
pointer and length; release successful allocations with `obscura64_free`.
Caller input/output buffers must not overlap. Do not choose `NONE` for
sensitive data or as a stand-in for Preview 2 protection.

## External operation-model check

Microsoft [CryptProtectData](https://learn.microsoft.com/en-us/windows/win32/api/dpapi/nf-dpapi-cryptprotectdata)
and [CryptUnprotectData](https://learn.microsoft.com/en-us/windows/win32/api/dpapi/nf-dpapi-cryptunprotectdata)
return operation-produced `DATA_BLOB` buffers owned under `LocalFree`, without
an exact pre-operation output-size API. Electron
[safeStorage](https://www.electronjs.org/docs/latest/api/safe-storage) returns
an encrypted `Buffer`. The higher-level
[Fernet API](https://cryptography.io/en/latest/fernet/) returns encrypted and
decrypted bytes without exposing raw cipher selection in each call. These
different APIs support one narrow conclusion for Obscura64: perform the
selected semantic operation first, then report or copy its actual result.

## V1 compatibility and architecture decision

V1 public entry points remain available without signature changes. The V1
Managed Payload, Current State (160 bytes), and History (336 bytes) formats
remain unchanged and documented in their historical documents. The frozen
4096-Profile library and its binary SHA-256
`8842cc4aa32bcb300937835f72aaacfd088568d1bfecd01811603c4f16e7280a`
remain unchanged. V1 data is not automatically recognized by the Preview 1
`unprotect` API; V1 reading and migration belong to Preview 2. The V2 source
module depends only on the public status/allocator contract, not V1 project
state or the V1 Provider ABI. Existing V1 APIs are the compatibility boundary.

| V1 component | V2 classification | Reason |
| --- | --- | --- |
| 4096 Profile Library V1 | V1 compatibility only | Frozen identity for released V1 data; no V2 ordinary parameter |
| Profile codec | V1 compatibility only | Decodes released V1 representations |
| Managed Payload V1 | V1 compatibility only | Historical format, future migration input |
| Provider ABI v1 | Advanced-only | Preserves V1 custom codecs; not a V2 plugin system |
| Project ID and generation | V1 compatibility only | V1 project identity/rotation, not V2 byte API |
| Latest-three Profile history | V1 compatibility only | V1 state recovery requirement |
| `current.state` and `history.state` | V1 compatibility only | Released state formats stay readable |
| LocalAppData mirror | V1 compatibility only | V1 project redundancy |
| Automatic project-state recovery | V1 compatibility only | V1 project lifecycle behavior |
| Builtin disaster scanner | Advanced-only | Legacy recovery tooling, outside ordinary V2 path |
| Project-open lifecycle | V1 compatibility only | V2 protect/unprotect requires no project |
| Current public API | Retained but refactored | V1 calls preserved; V2 ordinary calls added separately |
| Persistence module | V1 compatibility only | No Preview 1 V2 file API; future file layer may reuse verified primitives |
| Locking module | V1 compatibility only | V2 byte operations have no project lock |

No V1-only component is a required V2 ordinary mechanism. No working V1
module was deleted merely for architectural appearance. The source boundary
is the standalone `obscura64_v2_envelope.c` module and its minimal public API.
The operation-model correction found no need to alter the 24-byte wire layout:
magic, version, header size, kind, zero-only flags, body length, and exact-size
parsing already support operation-produced bodies.

## Deferred

Preview 1 does not implement Casual or DPAPI protection, V1 migration,
protected-file I/O, backups, V2 CLI tools, cryptographic policy selection,
or cross-platform support. Preview 5 owns the planned CMake, MSVC, Clang,
broader native x64, CI, and hardening validation. Preview 1 was manually
compiled and tested with GCC/G++ 16.2.0 on x64 Windows; CMake/CTest was not
available in this environment.
