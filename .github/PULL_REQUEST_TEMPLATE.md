## Summary

Describe the change and the problem it addresses.

## Why

Explain why this change is needed and what behavior is affected.

## Validation

List toolchain/architecture, commands, results, and any untested areas.

## Compatibility impact

- Public API:
- File formats:
- Provider ABI:
- Frozen Profile Library:

Profile Library V1 bytes, ID order, and SHA must remain unchanged. Incompatible
library/format/ABI changes require an explicitly agreed versioned design.

## Checklist

- [ ] Relevant tests were added or updated when behavior changed.
- [ ] Applicable existing tests pass; remaining validation gaps are documented.
- [ ] Public API, file-format, Provider ABI, and Profile Library impacts are stated.
- [ ] Relevant documentation is updated, or no documentation change is needed.
- [ ] No build artifacts, caches, runtime `.obscura64` state, or secrets are included.
- [ ] Security claims remain accurate: reversible obfuscation, not encryption.
