# Security Policy

## Supported versions

Stable **2.0.0** and the previous stable **1.x** release are supported for
security reports. Identify the exact release, tag, or commit and affected
Windows/toolchain environment. This policy does not promise a response deadline.
Pre-release tags remain available as immutable historical checkpoints.

## Reporting a vulnerability

For a genuine security-sensitive finding, prefer GitHub's
[private vulnerability reporting](https://github.com/RXY712200/Obscura64/security/advisories/new).
Include the affected version/commit and component, reproduction or a safe
proof-of-concept, impact, and relevant compiler, architecture, and Windows
environment. Remove unrelated secrets and personal data. Do not disclose
exploit-sensitive details in a public Issue while private coordination is appropriate.

If private reporting is unavailable, open a public Issue requesting a private
reporting channel without including sensitive details. Ordinary bugs and
compatibility problems may use [public Issues](https://github.com/RXY712200/Obscura64/issues).

## What counts as a security issue

Memory-safety errors, unintended arbitrary file access, unsafe path handling,
privilege-boundary violations, recovery behavior that violates documented
project identity/state guarantees, corruption that causes behavior outside
documented failure handling, and other implementation defects may qualify.

## Explicit non-goals

V1 and V2 Casual supply reversible obfuscation and corruption validation, not
cryptographic confidentiality or attacker authentication. V2 `NONE` supplies
no protection. V2 CurrentUser delegates genuine current-user protection to
Windows DPAPI; the inner Obscura64 digest checks corruption after DPAPI
unprotect. Neither mechanism makes client-side state a server trust boundary
or prevents an authorized local process from creating valid data. CurrentUser
is not portable. Obscura64 does not implement its own encryption algorithm.

Reversibility, public algorithm details, the frozen public Profile Library,
Casual's lack of cryptographic confidentiality or authentication, a knowledgeable
writer's ability to recompute public SHA-256 digests, and lack of DRM, anti-cheat,
or professional reverse-engineering resistance are not vulnerabilities by
themselves. SHA-256 is not a secret authentication mechanism.
