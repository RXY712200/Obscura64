#include "obscura64_internal.h"
#include <stdio.h>
#include <string.h>

typedef struct policy_case {
    obscura64_status status;
    int read_fallback;
    int write_replaceable;
} policy_case;

static const policy_case cases[] = {
    {OBSCURA64_OK, 0, 0},
    {OBSCURA64_INVALID_ARGUMENT, 0, 0},
    {OBSCURA64_INVALID_PROFILE, 0, 0},
    {OBSCURA64_INVALID_PROVIDER, 0, 0},
    {OBSCURA64_INVALID_DATA, 0, 0},
    {OBSCURA64_BUFFER_TOO_SMALL, 0, 0},
    {OBSCURA64_SIZE_OVERFLOW, 0, 0},
    {OBSCURA64_OUT_OF_MEMORY, 0, 0},
    {OBSCURA64_IO_ERROR, 0, 0},
    {OBSCURA64_STATE_MISSING, 0, 0},
    {OBSCURA64_STATE_CORRUPT, 0, 0},
    {OBSCURA64_RNG_FAILURE, 0, 0},
    {OBSCURA64_BUSY, 0, 0},
    {OBSCURA64_UNRECOVERABLE, 0, 0},
    {OBSCURA64_ENVELOPE_CORRUPT, 1, 1},
    {OBSCURA64_UNSUPPORTED_VERSION, 0, 0},
    {OBSCURA64_UNSUPPORTED_PROTECTION, 0, 0},
    {OBSCURA64_CASUAL_CORRUPT, 1, 1},
    {OBSCURA64_PROTECTION_FAILURE, 1, 0},
    {OBSCURA64_UNRECOGNIZED_DATA, 1, 1},
    {OBSCURA64_LEGACY_AMBIGUOUS, 1, 0},
    {OBSCURA64_LEGACY_SCAN_FAILURE, 0, 0},
    {OBSCURA64_PROTECTED_CORRUPT, 1, 1},
    {OBSCURA64_FILE_NOT_FOUND, 1, 1}
};

int main(void)
{
    size_t i;
    unsigned int checks = 0;
    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        if (obscura64_file_read_fallback_eligible(cases[i].status) !=
            cases[i].read_fallback) {
            fprintf(stderr, "read policy failed for status %d\n", (int)cases[i].status);
            return 1;
        }
        ++checks;
        if (obscura64_status_string(cases[i].status) == NULL ||
            strcmp(obscura64_status_string(cases[i].status), "unknown status") == 0) {
            fprintf(stderr, "status string missing for %d\n", (int)cases[i].status);
            return 1;
        }
        ++checks;
        if (obscura64_file_write_replaceable_failure(cases[i].status) !=
            cases[i].write_replaceable) {
            fprintf(stderr, "write policy failed for status %d\n", (int)cases[i].status);
            return 1;
        }
        ++checks;
    }
    if (obscura64_file_read_fallback_eligible((obscura64_status)9999) ||
        obscura64_file_write_replaceable_failure((obscura64_status)9999)) {
        fputs("unknown status became permissive\n", stderr);
        return 1;
    }
    checks += 2;
    if (strcmp(obscura64_status_string((obscura64_status)9999),
        "unknown status") != 0) return 1;
    ++checks;
    printf("V2 file policy checks: %u\n", checks);
    return 0;
}
