# Managed Payload V1

Managed APIs are recommended for new applications. Raw Codec remains available
for deliberate reversible byte encoding and cannot identify wrong Profile use.
Managed Payload wraps application bytes above the context Provider, then uses its
existing encode/decode callbacks. Runtime custom and compile-time default
Providers are supported; the frozen disaster scanner supports the builtin codec
only, not arbitrary custom transformations.

## Canonical decoded layout

All integers are little-endian. No compiler structure is dumped.

| Offset | Size | Field | V1 value |
|---:|---:|---|---|
| 0 | 8 | Magic | `OB64MP01` |
| 8 | 2 | Format Version | 1 |
| 10 | 2 | Header Size | 32 |
| 12 | 4 | Flags | 0 |
| 16 | 8 | Payload Length | exact application byte count |
| 24 | 8 | Reserved | all zero |
| 32 | payload_length | Application bytes | binary safe |
| 32 + payload_length | 32 | SHA-256 | header + payload |

Total decoded envelope size is exactly `64 + payload_length`. The digest covers
bytes `0..31+payload_length` and excludes itself. Windows BCrypt SHA-256 is used.
The parser rejects wrong magic/version/header size, nonzero flags/reserved,
lengths that cannot fit process size_t, addition overflow, missing/truncated data,
trailing bytes, and digest mismatch before returning any payload view.

## Public contracts

- `obscura64_managed_encoded_size`: query encoded envelope byte count, with checked
  envelope and Provider size calculations.
- `obscura64_managed_decoded_size`: decode and validate the envelope before
  reporting the application byte count.
- `obscura64_managed_encode` / `obscura64_managed_decode`: caller-owned buffers;
  explicit lengths, no automatic NUL, unchanged buffer on failure. Too-small
  buffers return BUFFER_TOO_SMALL and the required output length. NULL output
  with capacity zero is a size request. Other failures report length zero.
- `obscura64_managed_encode_alloc` / `obscura64_managed_decode_alloc`: module
  allocation, released with `obscura64_free`. Failure clears pointer and length.
  Empty input still encodes a 64-byte envelope. Empty allocating decode succeeds
  with NULL/zero and never allocates zero bytes. Empty encoded text is not a valid
  Managed envelope.

Wrong Profile use and corrupted envelopes return INVALID_DATA; validation cannot
cryptographically identify the cause. Provider/runtime resource errors retain
their own status. These APIs use temporary buffers before committing caller
output, including when a failing Provider partially writes its temporary output.

## Security and recovery

This is reversible obfuscation, not encryption or authentication. SHA-256 provides
corruption detection, wrong-Profile rejection, and recovery candidate validation.
A knowledgeable attacker can decode/modify known data, recompute SHA, and re-encode.
There is no HMAC, secret key, anti-cheat boundary, or secure secret storage.

The standalone builtin disaster scanner checks all 4096 frozen Profiles and
accepts only one envelope-valid match. Zero matches fail; multiple matches are
ambiguous. It never uses readability/JSON heuristics or scans during normal open.
It recovers application bytes and a Profile, not lost Project ID/generation or
history. See [RECOVERY_V1.md](RECOVERY_V1.md).
