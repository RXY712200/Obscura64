# Obscura64 Profile Library V1

- Status: Frozen
- Profile count: 4096
- Profile size: 64 bytes
- Canonical payload size: 262144 bytes
- Profile IDs: 0~4095
- Canonical representation: concatenation of Profile 0 through Profile 4095
- Canonical SHA-256: `8842cc4aa32bcb300937835f72aaacfd088568d1bfecd01811603c4f16e7280a`
- Source candidate: `generated/obscura64_profiles_v1_candidate.txt`
- Canonical binary: `generated/obscura64_profiles_v1.bin`
- The candidate's first-line magic `RB64_PROFILE_LIBRARY_CANDIDATE_V1` is retained byte-for-byte from the pre-rename Stage 2 freeze artifact format. It is historical artifact metadata and does not denote the current project name, Obscura64.
- Runtime representation: embedded internal C array
- The Profile Library is public and non-secret.
- SHA-256 identifies and verifies the library version/integrity; it does not provide encryption or security strength.
- V1 Profile order and content must never change after freeze.
