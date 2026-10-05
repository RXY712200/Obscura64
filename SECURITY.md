# Security Policy

## Supported versions

Currently supported: **1.x**. Reports should identify the exact version, tag,
or commit; this policy does not promise a response deadline.

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

**Obscura64 is NOT encryption.** It supplies reversible obfuscation and corruption
validation, not cryptographic confidentiality or authentication. Missing those
cryptographic properties is an explicit design boundary, not itself a vulnerability.

Reversibility, public algorithm details, the frozen public Profile Library,
lack of cryptographic confidentiality or authentication, a knowledgeable
writer's ability to recompute public SHA-256 digests, and lack of DRM, anti-cheat,
or professional reverse-engineering resistance are not vulnerabilities by
themselves. SHA-256 is not a secret authentication mechanism.
