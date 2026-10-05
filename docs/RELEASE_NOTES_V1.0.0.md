# Obscura64 v1.0.0

Obscura64 1.0.0 is a source-only release.

Obscura64 is a lightweight Windows C11 reversible obfuscation module for
software-internal local data. New applications should use Managed Payload APIs;
Raw Codec remains available for intentionally unvalidated reversible encoding.

Capabilities include binary-safe C/C++ APIs, custom Providers, a frozen 4096
Profile library, stable project state with latest-three history, verified file
replacement and process locking, LocalAppData redundancy and automatic recovery,
and a standalone builtin Managed Payload Profile-recovery CLI. CLI output never
overwrites an existing file and never rebuilds project identity or state.

Managed SHA-256 detects corruption/wrong Profile and validates recovery candidates;
it is not cryptographic authentication. Obscura64 is NOT encryption, anti-cheat,
DRM, or protection against professional reverse engineering. A knowledgeable
writer can modify and re-hash the public formats.

Exact backup recovery preserves Profile/generation. History fallback may roll
back to an older Profile; newer data may then be unusable. The disaster CLI scans
only the frozen builtin codec and cannot recover arbitrary custom-Provider data.

Profile Library V1: 4096 Profiles, 262144 canonical bytes, SHA-256:
`8842cc4aa32bcb300937835f72aaacfd088568d1bfecd01811603c4f16e7280a`.

Verified development environment: MinGW GCC/G++ 10.2.0, i686-w64-mingw32, C11 and
C++ consumer probes. CMake received static audit only: configure/build/CTest was
not executed. MSVC, Clang, and x64 testing are not claimed. File flush and verified
replacement do not certify arbitrary hardware power-loss durability.

MIT license. Source archives are sufficient; no release binary is required.
