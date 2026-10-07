# V2 Preview 4 — Upgrade and maintenance tooling

Development version: `2.0.0-preview.4`. V1 and V2 wire formats, Provider ABI,
and the frozen Profile Library remain unchanged.

## Explicit file upgrade

`obscura64_upgrade_file(path_utf8, target_protection)` fully reads a supported
representation, preserves its application bytes, and writes the current V2
format with the explicitly selected `NONE`, `CASUAL`, or `CURRENT_USER`
semantic. There is no default, implicit algorithm choice, or security-level
ordering. `CURRENT_USER` to `NONE` is allowed only by explicit request.

The Preview 3 full UTF-8 Windows path contract applies. One exclusive,
nonblocking `.ob64.lock` covers selection, protection, backup, and final
replacement. Contention returns `OBSCURA64_BUSY`. No separate decoded-plaintext
sidecar is written. An explicitly selected `NONE` target itself stores clear
application bytes, including in the verified replacement temp.

Valid PRIMARY is preferred. Its **exact protected bytes** become validated
`.ob64.bak` before verified replacement. If PRIMARY is current V2 with the
requested semantic, the operation succeeds without touching either file.
When PRIMARY is absent or has a replaceable data error, a valid BACKUP may
supply the bytes; BACKUP remains unchanged. Replaceable statuses are
`FILE_NOT_FOUND`, `ENVELOPE_CORRUPT`, `CASUAL_CORRUPT`,
`PROTECTED_CORRUPT`, and `UNRECOGNIZED_DATA`.

`PROTECTION_FAILURE`, `LEGACY_AMBIGUOUS`, unsupported version/protection,
I/O, allocation, scan, size, and unknown errors block replacement. In
particular, DPAPI failure may mean a valid object is inaccessible in the
current Windows context. The only automatically supported V1 input is builtin
Managed Payload using the frozen Profiles. Custom V1 Providers require the V1
context API. No project identity or history is invented.

The existing verified same-directory temp/write/flush/reopen/replace/final
validation path is reused. A detectable post-rename failure attempts to
restore valid source bytes. Old or new valid PRIMARY may remain after failure;
the validated backup is retained. Stale temps are never sources. Preview 3's
crash and durability limitations remain.

## Maintenance CLI

Build CMake target `obscura64_cli` to get `obscura64.exe`:

```text
obscura64 inspect <file>
obscura64 verify <file>
obscura64 upgrade <file> --to none|casual|current-user
```

The CLI uses UTF-16 `wmain` arguments, resolves ordinary relative or absolute
Windows paths, and converts them strictly to full UTF-8 for the library. It
never uses lossy ANSI conversion. `inspect` and `verify` open exactly the
named file, without backup fallback or mutation; naming `.ob64.bak`
explicitly inspects/verifies that backup.

`inspect` reports structural outer format, V2 version and semantic when
safely interpretable, protected length, structural status, and whether the
representation is currently supported. It can identify a CurrentUser envelope
without DPAPI success. An unsupported V2 version is identified by magic and
version only; malformed V2 never falls through to V1. Positive V1
identification uses the full supported legacy recognition path.

`verify` performs normal full unprotect. For `NONE`, success establishes only
canonical envelope structure and length, **not original-content integrity**.
For `CASUAL`, digest matching checks accidental corruption, not attacker
authentication. For `CURRENT_USER`, success requires DPAPI and inner-record
validation in this Windows context; failure does not necessarily mean
corruption. V1 format/digest success does not imply cryptographic authenticity.

`upgrade` reports PRIMARY/BACKUP source, source format, V2 source semantic,
target semantic, and changed yes/no. No command prints application plaintext.
Exit codes: `0` success, `1` data/operation failure, `2` usage error. Output is
human-readable with no promised stable JSON schema. The specialized V1
`obscura64_recover` tool remains separate and unchanged.

## Preview 5 boundary

Filesystem alias coordination (hard links, symlinks/reparse points, 8.3),
real CMake/CTest, MSVC/Clang, CI, sanitizers/fuzzing/static analysis, and final
API/package closure remain deferred. Preview 4 adds no generic repair,
backup history, cryptographic knobs, schema, or data-management layer.
