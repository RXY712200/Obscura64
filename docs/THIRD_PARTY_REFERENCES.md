# Third-Party References

Obscura64 V1 does not directly copy any third-party Base64 source code. The core algorithm is implemented independently based on RFC 4648. Third-party projects are references for design, testing, and engineering only.

## jwerle/b64.c

- Repository: https://github.com/jwerle/b64.c
- License: MIT
- Use: Reference only.
- Pure C and a small Base64 implementation.
- Useful as a bit-operation reference.
- Uses a fixed standard alphabet.
- Decoder behavior is unsuitable for our custom characters and strict validation.
- Not used as a source-code base.

## joedf/base64.c

- Repository: https://github.com/joedf/base64.c
- License: MIT
- Use: Reference only.
- Simple C implementation.
- Useful as a bit-operation reference.
- Character mapping is hard-coded to standard Base64.
- Invalid-character behavior does not meet V1 goals.
- Not directly adopted.

## aklomp/base64

- Repository: https://github.com/aklomp/base64
- Use: Reference only.
- Mature C99 implementation.
- SIMD, runtime dispatch, OpenMP, streaming, and performance ideas are useful references.
- Significantly more complex than V1 requires.
- Useful only as a future performance reference.

## MFMokbel/Base64CPPLib

- Repository: https://github.com/MFMokbel/Base64CPPLib
- License: MIT
- Use: Reference only.
- Key references: exactly 64 characters, duplicate rejection, padding separation, strict invalid-input checking, and custom Alphabet support.
- Its C++ architecture is not copied.

## tplgy/cppcodec

- Repository: https://github.com/tplgy/cppcodec
- License: MIT
- Use: Reference only.
- Key references: strict decoding, explicit codec variants, padded/unpadded distinction, Crockford human-readable alphabet choices, and avoiding visually ambiguous characters.

## crodas/base64-secret-rs

- Repository: https://github.com/crodas/base64-secret-rs
- License: MIT
- Use: Reference only.
- Key references: shuffled Base64 Alphabet concept and explicit statement that this is obfuscation, not cryptographic encryption.
- Its implementation model differs from our fixed 4096-Profile recovery system.

## YARA

YARA's use of custom Base64 alphabets is a conceptual reference showing that mature software uses custom 64-character Alphabets. No YARA source code is copied.

## Future License Rule

If a later stage proposes copying, modifying, or vendoring any third-party source code, implementation must stop first and report:

- repository
- source file
- license
- intended copied/modified scope
- required copyright notice
- LICENSE/NOTICE requirements
- possible effect on the final project license

Third-party source code must not be added until license review is complete.
