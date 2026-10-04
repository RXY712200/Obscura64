# Obscura64 — V1 Specification

**Status:** Stage 0 Design Freeze  
**Platform:** Windows only  
**Implementation language:** C

**Module positioning:** Obscura64 is a C language module that other Windows C projects can call directly. One of its design priorities is simple calling. Ordinary callers should eventually mainly need:

```c
#include "obscura64.h"
```

and a simple flow similar to:

```text
open/init
encode
decode
close
```

This records a usage-complexity goal only; it does not define final function signatures.

## 1. Module Purpose

- This module is a project-level randomized Base64 variant module.
- Obscura64 is a lightweight reversible obfuscation layer for software-internal local data. Its primary goal is to make such data less immediately readable and less convenient for ordinary users to manually edit.
- Example use cases include game save data, software runtime state, local internal configuration, cache records, internal identifiers, temporary application data, device-related application data, and licensing-related state.
- Obscura64 is not cryptographic encryption and is not intended to resist professional reverse engineering. Licensing or security-sensitive systems must not rely on Obscura64 secrecy alone.
- It uses a custom 64-character Alphabet/Profile to encode arbitrary byte data in a Base64-style format.
- Each calling project owns its own Profile.
- A Profile is project state, not device identity.
- Profile selection is not based on machine GUID, CPU, disk serial, MAC address, or Windows SID. Copying valid project state to another Windows machine does not automatically select another Profile.
- When the whole project is copied to another Windows computer, it should continue using the original Profile as long as the project state remains available.
- Module source code, algorithms, and file formats may be public.
- The module is an encoding / obfuscation layer.
- It is not an independent cryptographic encryption scheme.
- Calling projects must not base their overall security on “others do not know the Obscura64 algorithm.”

## 2. Input Data Model

The Obscura64 core does not determine which “base” or file type the input originally belongs to. The core input is uniformly treated as:

```text
byte buffer + length
```

It can therefore process:

- text
- binary data
- hexadecimal text
- decimal-number text
- octal text
- image content
- database data blocks
- configuration data
- arbitrary bytes from `0x00` through `0xFF`

Obscura64 does not interpret hexadecimal digits as a number and convert that number to “base 64.” It processes only the byte sequence actually provided.

## 3. Fixed Character Pool

The V1 fixed character pool is:

```text
ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnpqrstuvwxyz23456789@#$%&!?~
```

It is specified to contain exactly 64 characters. Its stated composition is:

- 24 uppercase letters
- 24 lowercase letters
- digits 2 through 9
- 8 special characters: `@ # $ % & ! ? ~`

The following characters are explicitly excluded:

```text
I
l
1
O
o
0
-
_
^
+
```

The main reasons are improved human readability and avoiding unnecessary symbol ambiguity.

This character pool does not promise to be:

- URL-safe
- shell-safe
- Windows-filename-safe

It is only an encoding-data Alphabet.

## 4. Profile System

- Profile Library V1 contains exactly 4096 fixed, unique Profiles.
- `4096 = 2^12`.
- Profile IDs 0 through 4095 are permanently bound to their frozen V1 entries.
- Each Profile is a complete permutation of the fixed 64-character pool.
- Every character must appear exactly once in each Profile.
- All 4096 Profiles must be distinct.
- Candidate generation rejects duplicate Profiles and retries.
- A second independent full-set uniqueness validation is required before freezing.
- Profile order 0 through 4095 became meaningful when the candidate was frozen in Stage 2.3.
- The canonical library representation is exactly 262144 bytes: the 64 bytes of each Profile concatenated in ID order.
- Canonical SHA-256: `8842cc4aa32bcb300937835f72aaacfd088568d1bfecd01811603c4f16e7280a`.
- Default Profile Library V1 will be embedded with the module distribution rather than requiring ordinary callers to manage a separate runtime Profile Library file.
- Ordinary users do not need an external Profile Library file.
- The library is not treated as secret.
- Its exact generated representation was frozen in Stage 2.3.
- Future incompatible Profile Library changes require a new library/version identity; V1 must not be silently altered.

**4096 is not a security-strength parameter.** Its main purpose is to change the extreme-disaster-recovery problem from “a completely random lost Profile may be permanently unrecoverable” to “recovery can try a finite set of 4096 candidates.”

The normal runtime path must not iterate through all 4096 Profiles.

## 5. Project Independence

On the same computer, Project A, Project B, and Project C may use completely different Profiles. Projects do not share one computer-wide Profile.

The actual project-local state required for normal operation is the complete current 64-character permutation. Normal operation does not require storing the Profile ID. Profile IDs are mainly for the complete Profile Library and disaster-recovery management.

## 6. Normal Usage Simplicity

Ordinary callers should not be forced to understand or manually manage:

- Profile Library
- Profile ID
- 64-character internal mapping
- History storage
- redundant backups
- Storage Backend
- Provider
- recovery state machine
- registry storage

These should be handled internally by the module as far as possible.

The ordinary API and Advanced API must be separate. Provider and custom-recovery advanced features must not complicate the ordinary call flow. The public API targets one main public header, `obscura64.h`. Core implementation uses C, and the header supports C++ projects through `extern "C"`.

V1 should depend only on:

- the C standard library
- Windows system APIs

Ordinary use must not require installing an additional third-party runtime library.

### Stage 3.1 Public C API Principles

- The ordinary public surface is provided through `obscura64.h`; internal implementation headers are not part of the caller-facing API.
- The context is opaque to callers and owns a copy of the complete 64-byte Profile supplied at construction.
- Byte input and output use explicit lengths and caller-owned buffers. No NUL terminator is implied for encoded or decoded data.
- The module creates and destroys its context. Profile persistence and project-level initialization remain later-stage responsibilities; Stage 3.1 does not expose random Profile selection or Profile IDs through the public API.
- Public operations report a status value. This API boundary does not add file, Provider, history, recovery, or storage behavior.

### Stage 3.2 Buffer and Allocation Principles

- Caller-buffer encode/decode APIs remain available for advanced callers and callers avoiding additional allocations.
- Allocating convenience APIs are available for ordinary use. Returned data buffers use module-owned allocation and must be released with `obscura64_free`.
- Both API styles use explicit lengths. Encoded and decoded results are not NUL-terminated; empty allocating operations succeed with a `NULL` buffer and length zero.
- Normal project-level open/init remains deferred until project persistence is implemented in Stage 4.

## 7. Raw Data Encoding

The core must support both:

```text
arbitrary bytes → current Profile → Obscura64 encoded data
```

and:

```text
Obscura64 encoded data → current Profile → exact original bytes
```

Binary data containing `0x00` must be supported. The implementation must not use `strlen` to determine the length of original binary data. All binary interfaces must be based on explicit lengths. Public raw codec API signatures and ownership are recorded in Stages 3.1 and 3.2.

### V1 Padding Rule

- V1 uses the fixed character `=` as Base64 padding.
- `=` is not part of the fixed 64-character pool.
- `=` is not part of any Profile's 64-character permutation.
- Profiles map only values 0 through 63.
- When padding is required, V1 encoded output uses the standard Base64-style trailing `=` rule.
- Specific strict decoding rules will be completed in Stage 1.3.

### Strict Raw Decode Rules (Stage 1.3)

- V1 strict decode accepts only canonical padded Obscura64; empty input is also valid.
- Except for empty input, encoded length must be a multiple of 4.
- Whitespace is not ignored.
- `=` appears only in valid padding positions in the final quantum.
- Non-zero unused pad bits are rejected.
- The raw core uses explicit lengths and supports embedded NUL bytes.

## 8. File Usage Model

Obscura64 is a module, not a transparent file system. V1 does not attempt to intercept or replace Windows/C standard-library `fopen`, `fread`, or `fwrite` calls. If a caller wants on-disk file contents to remain in Obscura64 form, it must explicitly use file-helper interfaces provided by Obscura64 or call the encoding API first.

### Whole-file encoding

Suitable for small or medium important data files, configuration, archives, metadata, and project-private data. Obscura64 data is stored on disk. When needed, the program decodes the disk data in memory:

```text
Obscura64 disk data → decode in memory → application uses original bytes
```

The normal flow does not require creating a plaintext temporary file.

### Partial/block encoding

For large files or structured data, using Obscura64 does not require encoding the entire file. The calling project should pass only the truly important fields or data blocks to Obscura64. For example:

```text
normal metadata → unchanged
large resource block → unchanged
important configuration → Obscura64
important project field → Obscura64
```

Obscura64 does not decide which parts are important; the calling project decides which bytes to pass in.

### Large-file streaming

Streaming encode/decode for extremely large files is not a V1 core goal. Consider a streaming API only if a real need arises later. Do not complicate V1 for hypothetical future needs.

## 9. Source Code Scope

The calling project's own `.c` and `.h` source files are not in Obscura64's automatic protection scope by default. Obscura64 does not provide:

- C source encryption
- compiler interception
- transparent source decoding
- source-code DRM

If these capabilities are needed later, treat them as a separate project or feature.

## 10. Project State

The valid primary state in the project directory is always the highest-authority source.

Normal initialization:

- If the project has never been initialized, create project state and select a Profile.
- If a valid Profile already exists, ordinary initialization must continue using the original Profile.
- Ordinary initialization must never randomize a new Profile.

Only explicit **Force Reinitialize** may replace the current Profile.

## 11. History

### Force Reinitialize and latest-three retention

Force is explicit through `obscura64_force_reinitialize` or the Advanced custom Provider variant. It requires existing valid current state, preserves Project ID, increments generation by one with overflow checking, and requires a different frozen Profile. Existing contexts keep their old Profile; callers must retire them themselves.

The previous current generation/Profile is saved and verified in history before current is replaced. Force automatically retains the latest three historical generations, newest first, and discards the oldest when full. The fixed 336-byte History V1 format is described in `PROJECT_HISTORY_V1.md`.

Historical generations are strictly decreasing, with no continuity requirement. Profiles may repeat across non-adjacent generations. History generations must be less than current.generation; a matching entry-0/current overlap from a failed current replacement is the sole exception and is normalized during Force. Same-generation Profile conflicts and future history generations are rejected without rewriting current or history. The generic history parser stays strict and rejects duplicate generations.

Force uses a minimal CREATE_NEW `operation.lock` and returns BUSY when the lock exists. Basic replacement writes and verifies a temp, uses MoveFileExW with replacement/write-through flags, and verifies the target. Full persistence hardening remains Stage 5. History is not automatically used to recover current state.

Before Force Reinitialize, the current Profile must first be placed into history. V1 stores at most the most recent three generations of Profiles. When there are more than three generations, evict the oldest record. The current Profile and historical Profiles are distinct concepts.

## 12. Redundancy and Recovery

Project-directory state is authoritative. State in other Windows locations is only a redundant backup.

If the project primary state is valid but another copy disagrees, the project primary state wins. An invalid or old copy must not overwrite a valid project primary state.

If the project primary state is damaged, validate redundant copies. Only a copy that can be confirmed complete, valid, and matching the project may be used for recovery. Historical state may be tried if necessary. If all automatic recovery fails, return an explicit `UNRECOVERABLE` state.

It is prohibited to silently generate a new Profile because state is missing or damaged.

## 13. Disaster Recovery

The complete 4096-Profile Library is mainly for extreme disaster recovery. A normal application must not automatically scan all 4096 Profiles. A future disaster-recovery tool may try Profile 0 through Profile 4095 one by one, but it must have a reliable way to determine which Profile is correct. A candidate must not be considered recovered merely because its decoded output “looks like normal text.”

V1 will later define a Managed Payload Envelope that includes enough information to validate candidate data:

- Magic
- Version
- Length
- Integrity information

Raw Codec and Managed Codec must be distinct:

- **Raw Codec:** only handles bytes ↔ Obscura64.
- **Managed Codec:** adds a structured Envelope to support reliable validation and disaster recovery.

The specific format will be frozen in a later stage.

## 14. Provider Principle

The module provides a default codec implementation. Ordinary users can remain unaware of Providers. Stage 3.3 defines the Advanced Provider ABI for codec strategy replacement, runtime injection, and compile-time default selection. Providers do not own project persistence, history, registry, or recovery. Core lifecycle logic for state, history, and recovery must not be tightly coupled to a particular encoding algorithm.

## 15. Reliability Requirements

V1 reliability goals:

- Ordinary repeated initialization must not change the Profile.
- Two processes initializing simultaneously must not create two different project identities.
- An interrupted state write must not easily damage the only usable state.
- State must be verifiable after writing.
- Later implementation should use temp write + verify + atomic replace.
- Later implementation should provide necessary interprocess locking.
- An invalid backup must not overwrite valid primary state.
- When recovery is impossible, the module must not secretly create a new identity.

Specific Windows implementation details are for a later stage.

## 16. Explicit Non-Goals for V1

- No true cryptographic encryption system.
- No guarantee that professional reverse engineers cannot recover data.
- No transparent source-code encryption.
- No file-system driver.
- No hooking of `fopen`, `fread`, or `fwrite`.
- No high-performance streaming system for extremely large files.
- No HPSort or other sorting module, because V1 has no actual sorting requirement.
- No SIMD Base64 optimization.
- No heavy approach such as TPM, hidden partitions, or unusual drive letters to hide the Profile Library.
- No requirement to support Linux or macOS.

## 17. Stage Plan

### Stage 4.1 Project State V1 format

- Project-managed state stores the complete 64-byte Profile, not only a Profile ID, and the Profile must be a member of Frozen Profile Library V1.
- A stable nonzero 128-bit Project ID is project-level identity that travels with copied project state; it is not device identity.
- Generation begins at 1 and changes only when a future Force Reinitialize operation creates a new generation.
- State V1 uses a canonical 160-byte representation with explicit little-endian integer fields and SHA-256 over bytes 0 through 127.
- The SHA-256 digest detects corruption; it is not attacker-resistant authentication because a writer can recompute it.
- Stage 4.1 implements the in-memory format only. Disk persistence is not implemented.

### Stage 4.2 Project open

- `obscura64_open` accepts an existing project directory as a UTF-8 path and uses the configured default Provider. `obscura64_open_with_provider` is the advanced equivalent for a caller-supplied codec Provider.
- The authoritative current state is `<project>/.obscura64/current.state`; it moves with the project. Windows path handling converts strict UTF-8 to UTF-16 and uses wide filesystem APIs.
- If `.obscura64` does not exist, first initialization creates generation 1 with a random nonzero Project ID and a randomly selected frozen V1 Profile. If `.obscura64` exists without `current.state`, open returns a missing-state status. Existing corrupt state is rejected and is never silently regenerated.
- Normal open validates and uses the stored Profile without changing the state. First creation uses `CREATE_NEW` and rereads the written state before returning a context.
- Stage 4.2 first-write handling is basic. Stronger atomic persistence and full process locking belong to Stage 5.

The fixed V1 major stages are:

1. **Stage 0 — specification freeze**
2. **Stage 1 — Base64 core**
3. **Stage 2 — 4096 Profile system**
4. **Stage 3 — public C module/API**
5. **Stage 4 — project state and three-generation history**
6. **Stage 5 — reliable state persistence**
7. **Stage 6 — redundancy and automatic recovery**
8. **Stage 7 — disaster recovery, full testing and release**

V1 is fixed at these eight major stages. Do not continue adding Stage 8, 9, 10, and so on indefinitely unless a genuine structural error is found. If a new need arises later, prefer V2 rather than indefinitely expanding V1.

### Stage 5.1 verified byte persistence

State V1 (160 bytes) and History V1 (336 bytes) formats remain unchanged. The private persistence module owns Wide Win32 byte I/O; serialization and semantic checks remain in the State/History modules via private verifier callbacks.

Replacement sequence: unique same-directory temp with BCrypt suffix and CREATE_NEW -> exact write with partial-write loop -> FlushFileBuffers -> close -> reopen and verify exact size/bytes and format -> MoveFileExW with MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH -> final reopen and verify. Existing authoritative files are never intentionally truncated or rewritten in place. Only temps created by this invocation are removed; unrelated and stale temp-like files are preserved. Post-replacement failure is reported, with the prior best-effort restoration protection retained.

First initialization still creates the authoritative file with CREATE_NEW, then writes, flushes, closes and verifies it. A losing initializer reads the winning state using the existing bounded retry behavior; it never overwrites the winner. Force still commits verified history before preparing/replacing current and retains the previous Profile on current replacement failure.

This improves write/replacement reliability but does not prove absolute power-loss durability under every filesystem, device or controller failure. The existing Force operation.lock is unchanged. Full multi-process locking belongs to Stage 5.3; crash/fault simulations belong to Stage 5.4. No automatic recovery is added; that remains Stage 6 work.
