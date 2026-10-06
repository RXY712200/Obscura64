# Obscura64 2.0.0-preview.2 — Protection and legacy

Preview 2 makes the self-describing V2 envelope usable without a V1 project.
The 24-byte outer envelope from Preview 1 is unchanged. Ordinary callers
choose the semantic protection they need; they do not choose cipher algorithms,
DPAPI flags, Profiles, project identity, or a key-management policy.

## Ordinary operation

`obscura64_protect_alloc(protection, bytes, length, &envelope, &size)` is the
recommended path. `obscura64_unprotect_alloc(envelope, size, &bytes, &length)`
dispatches from the V2 kind field, or recognizes supported V1 builtin Managed
Payload input. Both are binary-safe, and successful allocations are released
with `obscura64_free`. There is no implicit protection default.

Caller-buffer operations execute the full backend operation into temporary
storage before copying. `BUFFER_TOO_SMALL` reports the actual result size from
that operation. A query and retry are **two operations**: for a variable-output
backend, the first observed size is not a promise about the second result.
Use allocating operations to avoid retry logic. Failure leaves caller output
bytes unchanged.

`obscura64_unprotect_alloc_ex` adds one optional success classification:
`OBSCURA64_FORMAT_V2` or
`OBSCURA64_FORMAT_V1_LEGACY_UPGRADE_RECOMMENDED`. The basic unprotect API
requires no metadata argument. On failure the optional classification is
`OBSCURA64_FORMAT_UNKNOWN`.

## Protection semantics and security boundary

| Choice | Behavior | Security limit |
| --- | --- | --- |
| `NONE` (kind 0) | Plain body, explicit opt-in | No confidentiality, integrity, or authentication |
| `CASUAL` (kind 1) | Self-contained Profile encoding and SHA-256 corruption validation | Reversible obfuscation, not encryption or attacker authentication; a knowledgeable writer can recompute the digest |
| `CURRENT_USER` (kind 2) | Windows current-user DPAPI protected blob | OS-provided user-bound protection; not portable or a server trust boundary |

CurrentUser puts a small, validated Obscura64 plaintext record **inside** the
raw DPAPI blob. It calls `CryptProtectData` and `CryptUnprotectData` with
`CRYPTPROTECT_UI_FORBIDDEN`, no optional entropy, and **without**
`CRYPTPROTECT_LOCAL_MACHINE`. Windows owns DPAPI output until released with
`LocalFree`; Obscura64 copies it into a public `obscura64_free` allocation.
The complete plaintext record is checked against `DWORD` limits before
narrowing. A DPAPI unprotect failure returns `OBSCURA64_PROTECTION_FAILURE`
unless Windows unambiguously reports out-of-memory. It does not guess
wrong-user versus corrupt-blob. A successful DPAPI unprotect must also pass
the inner record's
exact structure and SHA-256 digest, or `OBSCURA64_PROTECTED_CORRUPT` is
returned. Windows documents that DPAPI can sometimes succeed with corrupted
output; the inner validation catches that case. Copying an envelope to another
user or machine is not promised to work. Local tests do not establish
cross-user or cross-machine behavior.
DPAPI protects bytes in the current user's Windows environment; it does not
make client-side state authoritative to a server.

### CurrentUser plaintext record V1

The outer V2 kind-2 body remains a raw DPAPI blob. Inside its encrypted
plaintext is this canonical record, with no external state or user ID:

| Plaintext offset | Bytes | Field |
| --- | ---: | --- |
| 0 | 8 | Magic `OB64CU01` |
| 8 | 2 | Record version = 1, little-endian |
| 10 | 2 | Header size = 56, little-endian |
| 12 | 4 | Flags = 0 |
| 16 | 8 | Exact application payload length, little-endian |
| 24 | 32 | SHA-256 of bytes 0–23 followed by application payload |
| 56 | remainder | Application bytes |

Exact length, fixed fields, and digest are required. Empty application bytes
produce a 56-byte record. The digest checks Obscura64 data after DPAPI returns;
it does not make client-side data authoritative. An authorized local process
can itself call DPAPI and create valid protected data. This is not DRM or
anti-cheat.

## Casual body V1

The outer kind is 1. The body is a dedicated V2 Casual representation:

| Body offset | Bytes | Field |
| --- | ---: | --- |
| 0 | 1 | Casual body version = 1 |
| 1 | 1 | Flags = 0 |
| 2 | 2 | Frozen Profile Library V1 ID, little-endian, 0–4095 |
| 4 | 4 | Reserved = 0 |
| 8 | 32 | SHA-256 of body bytes 0–7 followed by decoded application bytes |
| 40 | remainder | Canonical padded Profile codec output |

The outer exact-length rule and strict Profile decoder apply. Empty payload
has zero encoded bytes but still has the 40-byte body. Protect selects a random
frozen Profile via the existing BCrypt system CSPRNG selector. The ID is stored
openly; it is **not secret**. The digest detects accidental corruption and
wrong data, not deliberate modification by a knowledgeable writer. Casual
requires no project directory, `current.state`, history, or LocalAppData mirror.
Malformed Casual bodies return `OBSCURA64_CASUAL_CORRUPT`.

The frozen Profile Library, raw Profile codec, and runtime selector are now
**internal V2 Casual primitives as well as V1 compatibility dependencies**.
V1 Managed Payload is not nested inside this body. Project ID, generation,
latest-three Profile history, V1 state files, mirror, and V1 project-open
remain outside the ordinary V2 path. Provider ABI v1 and the disaster CLI
remain advanced/legacy facilities.

## Legacy recognition and migration

An input beginning with the full eight-byte `OB64ENV2` magic is always parsed
as V2. Malformed or unsupported V2 input never falls through to V1 scanning.
Otherwise, the shared builtin disaster scanner first rejects impossible V1
Managed inputs: fewer than 88 bytes, length not divisible by four, characters
outside the frozen pool, or invalid `=` positions. It then considers **all
4096** frozen Profiles against the first 20 encoded characters. Those
characters derive solely from the first 15 fixed bytes of the canonical V1
Managed Payload header. Only prefix candidates receive a full decode and
strict V1 Managed Payload structure/SHA-256 validation. A prefix match alone
never establishes validity. Exactly one match succeeds and reports
legacy/upgrade recommended. Zero matches return
`OBSCURA64_UNRECOGNIZED_DATA`; multiple matches return
`OBSCURA64_LEGACY_AMBIGUOUS`. A scanner failure returns
`OBSCURA64_LEGACY_SCAN_FAILURE`. The optimized scan still considers 4096
Profiles and can be slower than a normal V2 read; valid V2 envelopes never
run it. No readability or text heuristic is used.

`obscura64_migrate_v1_managed_alloc` performs the narrow supported path:
unique builtin V1 Managed Payload decode, then application bytes protected
as V2 NONE, CASUAL, or CURRENT_USER. It does not invent project state, mutate
V1 state, perform application-schema migration, or build a general graph of
old and new protection versions. Arbitrary V1 custom Provider data is **not**
automatically recognized or migrated; callers still use their existing V1
Provider/context API for that data. Preview 4 owns the full upgrade workflow.

## Deferred

Preview 3 owns protected-file read/write, verified replacement, one validated
backup, fallback, and file-layer concurrency/failure semantics. This preview
adds no V2 file persistence or recovery policy. Preview 4 owns CLI inspection,
verification, and upgrade tooling. Preview 5 owns broader hardening and toolchain
validation.
