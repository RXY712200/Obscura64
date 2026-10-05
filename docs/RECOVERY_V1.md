# Obscura64 V1 Redundancy and Recovery

## Authority and storage

A valid `<project>/.obscura64/current.state` is the unique highest authority. A
newer, different, or foreign backup never replaces it. Normal open succeeds with
valid current even when history or redundancy cannot be repaired.

Exactly one external mirror is used:

`<FOLDERID_LocalAppData>\Obscura64\Projects\<64 lowercase hex locator>\`

Production resolves LocalAppData using `SHGetKnownFolderPath` and releases its
allocation using `CoTaskMemFree`. No COM initialization, environment override,
Registry, ProgramData, RoamingAppData, cloud copy, or device identity is used.

The locator hashes the final normalized DOS path returned by
`GetFinalPathNameByHandleW` on a real project directory handle. BCrypt SHA-256
receives the resolved UTF-16 path bytes without the NUL terminator. Caller trailing
separators and alternate path spelling resolved by the handle converge. Resolved
casing is preserved to avoid conflating case-sensitive directories. This is a
local lookup key, not a secret, Project ID, or authentication mechanism.

The mirror reuses Current State V1 (160 bytes) and History V1 (336 bytes), without
wrappers or path metadata. Generation one has a self-contained mirror history:
same Project ID, count zero. Project generation one may still omit history.

Copying/moving a valid project preserves Project ID, generation, and Profile. A
successful open creates the mirror at its new locator. Old mirror directories
may remain orphaned; no garbage collection is implemented. The project-directory
state carries portable identity; LocalAppData is machine/user/path-local and is
not the state users should copy as the main project.

## Coordination and synchronization

Normal open reads current under the Stage 5 shared project lock. It releases that
lock before trying immediate exclusive maintenance. A failed maintenance lock or
backup write cannot demote a successfully constructed Context. Maintenance
re-reads authoritative disk state under exclusive coordination, repairs history
when legitimate evidence permits, then synchronizes a valid managed pair.

Force uses the same exclusive lock for recovery, managed validation, mutation,
and best-effort mirror synchronization. Mirror commit order is history first,
current second, using verified persistence. Existing files are never intentionally
rewritten in place. A mirror current failure after history commit can leave a
safe overlap; later maintenance repairs it. Other interrupted/stale mirror pairs
may be incompatible and must be rejected as complete snapshots. A successful
primary Force or recovery is never rolled back because mirror synchronization
failed. Stage 5 flush and process-crash guarantees apply; arbitrary hardware
power-loss durability is not certified.

## One recovery decision path

`obscura64_recovery_ensure` is private and runs under exclusive project locking.
Both open and Force use this policy; redundancy performs no authority selection.
If open detects missing/corrupt current under shared coordination, it releases
shared, acquires exclusive immediately, and rechecks disk state. Contention
returns BUSY. There is no special locking bypass for recovery.

If current is valid but its managed history is unusable:

1. A valid backup snapshot whose current exactly matches main can restore its
   matching history.
2. A valid same-identity backup exactly one generation behind can contribute its
   current plus prior entries, newest first, maximum three. Overlap is included
   once. The candidate must pass managed validation before any write.
3. Missing generations are never invented. Normal open remains available if
   repair is impossible; Force returns UNRECOVERABLE before selecting a Profile.

If current is unusable in an existing `.obscura64` container, sources are tried:

1. Exact validated backup current, paired with compatible project or backup
   history where available.
2. Valid project history entry zero promoted to current.
3. Valid backup history entry zero promoted to current, with history copied into
   the project.
4. Otherwise UNRECOVERABLE.

A structurally valid local history anchors Project ID and excludes foreign
external sources. Without local identity evidence, incompatible valid external
Project IDs are ambiguous: recovery fails rather than guessing by generation.
Every promoted history must form a permitted managed overlap with its entry zero;
generic-valid gaps or incomplete windows are not sufficient.

An exact valid backup current without compatible history can restore normal
runtime without fabricating history. Force still requires a mutation-safe managed
pair and returns UNRECOVERABLE if it cannot reconstruct one. History-derived
recovery never increments generation or generates a Profile/identity. Promotion
of project history leaves its bytes unchanged. Recovered named disk files are
re-read and validated before a Context is returned; mirror maintenance follows
best-effort. Missing targets use safe create-new; corrupt targets use verified
replacement.

No `.obscura64` container means fresh initialization, even if the locator has an
old backup. The new project may replace that stale mirror. An existing container
never silently receives a fresh identity after failed recovery.

`OBSCURA64_UNRECOVERABLE` means there is existing managed-project evidence but no
validated source reconstructs the needed runtime/mutation state. Invalid inputs,
Provider errors, primary filesystem access failures, and locking conflicts retain
their own statuses.

## Recovery limits and tests

Exact-current recovery preserves Profile/generation and can decode data produced
with that Profile. History fallback can roll back Profile/generation; data encoded
only with a lost newer Profile may not decode after rollback. Stage 7 broader
Profile discovery using Managed Payload validation is planned, not implemented.
Recovery never scans 4096 Profiles, treats readability as evidence, or uses temp
files as sources.

SHA-256 detects accidental corruption, not attacker-resistant authentication. A
knowledgeable writer can modify these known formats and recompute their digests.

The formal redundancy/recovery tests use a private compile-time
`OBSCURA64_REDUNDANCY_TESTING` build. Only that build reads the temporary-root
environment variable `OBSCURA64_TEST_BACKUP_ROOT`; absent override means mirror
unavailable and never falls through to real LocalAppData. CMake test-support and
dedicated instrumented executables are separate from the production library.
Fault controls remain under `OBSCURA64_TESTING`. Neither switch is a public API or
a production runtime option. All generated test files are beneath owned Temp
roots; recursive cleanup refuses paths outside them and refuses reparse points.

Stage 6 is complete after the full GCC regression and invariant audit. Stage 7
remains next. No release or tag is created by this stage.
