#include "obscura64.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int checks;
#define CHECK(name, condition) do { ++checks; if (!(condition)) { \
    fprintf(stderr, "FAIL: %s (line %d)\n", name, __LINE__); return 1; \
} } while (0)

static int expect_parse(const unsigned char *data, size_t len, obscura64_status expected)
{
    size_t n = 123;
    void *result = (void *)1;
    obscura64_status got = obscura64_unprotect_alloc(data, len, &result, &n);
    CHECK("parse status", got == expected);
    CHECK("failure clears allocation", result == NULL && n == 0);
    return 0;
}

int main(void)
{
    const unsigned char payload[] = { 0, 1, 255, 0, 'X' };
    const unsigned char canonical[] = {
        'O','B','6','4','E','N','V','2', 2,0, 24,0, 0,0, 0,0,
        5,0,0,0,0,0,0,0, 0,1,255,0,'X'
    };
    unsigned char data[sizeof(canonical) + 1];
    unsigned char output[sizeof(payload)];
    void *allocated = (void *)1;
    size_t n = 999;
    CHECK("version", OBSCURA64_VERSION_MAJOR == 2 &&
        strcmp(OBSCURA64_VERSION_PRERELEASE, "preview.1") == 0);
    CHECK("semantic values", OBSCURA64_PROTECTION_NONE == 0 &&
        OBSCURA64_PROTECTION_CASUAL == 1 && OBSCURA64_PROTECTION_CURRENT_USER == 2);
    CHECK("operation-driven size query", obscura64_protect(OBSCURA64_PROTECTION_NONE,
        payload, sizeof(payload), NULL, 0, &n) == OBSCURA64_BUFFER_TOO_SMALL && n == sizeof(canonical));
    CHECK("overflow", obscura64_protect_alloc(OBSCURA64_PROTECTION_NONE,
        payload, SIZE_MAX, &allocated, &n) == OBSCURA64_SIZE_OVERFLOW && allocated == NULL && n == 0);
    CHECK("null input", obscura64_protect(OBSCURA64_PROTECTION_NONE,
        NULL, 1, data, sizeof(data), &n) == OBSCURA64_INVALID_ARGUMENT && n == 0);
    CHECK("unsupported Casual", obscura64_protect_alloc(OBSCURA64_PROTECTION_CASUAL,
        payload, sizeof(payload), &allocated, &n) == OBSCURA64_UNSUPPORTED_PROTECTION && allocated == NULL && n == 0);
    CHECK("unsupported CurrentUser", obscura64_protect_alloc(OBSCURA64_PROTECTION_CURRENT_USER,
        payload, sizeof(payload), &allocated, &n) == OBSCURA64_UNSUPPORTED_PROTECTION && allocated == NULL && n == 0);
    CHECK("invalid semantic", obscura64_protect_alloc((obscura64_protection)99,
        payload, sizeof(payload), &allocated, &n) == OBSCURA64_UNSUPPORTED_PROTECTION && allocated == NULL && n == 0);
    memset(data, 0xA5, sizeof(data));
    CHECK("short output", obscura64_protect(OBSCURA64_PROTECTION_NONE,
        payload, sizeof(payload), data, sizeof(canonical)-1, &n) == OBSCURA64_BUFFER_TOO_SMALL && n == sizeof(canonical));
    CHECK("short output unchanged", data[0] == 0xA5 && data[sizeof(canonical)-1] == 0xA5);
    CHECK("protect", obscura64_protect(OBSCURA64_PROTECTION_NONE,
        payload, sizeof(payload), data, sizeof(data), &n) == OBSCURA64_OK && n == sizeof(canonical));
    CHECK("canonical bytes", memcmp(data, canonical, sizeof(canonical)) == 0);
    CHECK("decode size query", obscura64_unprotect(data, n, NULL, 0, &n) == OBSCURA64_BUFFER_TOO_SMALL && n == sizeof(payload));
    memset(output, 0xA5, sizeof(output));
    CHECK("unprotect short", obscura64_unprotect(data, sizeof(canonical), output, sizeof(output)-1, &n) == OBSCURA64_BUFFER_TOO_SMALL && n == sizeof(payload));
    CHECK("unprotect atomic", output[0] == 0xA5 && output[sizeof(output)-1] == 0xA5);
    CHECK("unprotect", obscura64_unprotect(data, sizeof(canonical), output, sizeof(output), &n) == OBSCURA64_OK && n == sizeof(payload));
    CHECK("binary roundtrip", memcmp(output, payload, n) == 0);
    CHECK("truncated header", expect_parse(data, 23, OBSCURA64_ENVELOPE_CORRUPT) == 0);
    CHECK("truncated body", expect_parse(data, sizeof(canonical)-1, OBSCURA64_ENVELOPE_CORRUPT) == 0);
    CHECK("trailing data", expect_parse(data, sizeof(canonical)+1, OBSCURA64_ENVELOPE_CORRUPT) == 0);
    data[0] ^= 1; CHECK("magic", expect_parse(data, sizeof(canonical), OBSCURA64_ENVELOPE_CORRUPT) == 0); data[0] ^= 1;
    data[8] = 3; CHECK("version dispatch", expect_parse(data, sizeof(canonical), OBSCURA64_UNSUPPORTED_VERSION) == 0); data[8] = 2;
    data[10] = 25; CHECK("header size", expect_parse(data, sizeof(canonical), OBSCURA64_ENVELOPE_CORRUPT) == 0); data[10] = 24;
    data[12] = 1; CHECK("kind dispatch", expect_parse(data, sizeof(canonical), OBSCURA64_UNSUPPORTED_PROTECTION) == 0); data[12] = 0;
    data[12] = 2; CHECK("CurrentUser not implemented", expect_parse(data, sizeof(canonical), OBSCURA64_UNSUPPORTED_PROTECTION) == 0); data[12] = 0;
    data[12] = 255; CHECK("unknown kind", expect_parse(data, sizeof(canonical), OBSCURA64_UNSUPPORTED_PROTECTION) == 0); data[12] = 0;
    data[14] = 1; CHECK("unknown flags", expect_parse(data, sizeof(canonical), OBSCURA64_ENVELOPE_CORRUPT) == 0); data[14] = 0;
    memset(output, 0xA5, sizeof(output));
    data[14] = 1; n = 123;
    CHECK("corrupt decode status", obscura64_unprotect(data, sizeof(canonical), output, sizeof(output), &n) == OBSCURA64_ENVELOPE_CORRUPT && n == 0);
    CHECK("corrupt decode atomic", output[0] == 0xA5 && output[sizeof(output)-1] == 0xA5);
    data[14] = 0;
    data[16] = 6; CHECK("length mismatch", expect_parse(data, sizeof(canonical), OBSCURA64_ENVELOPE_CORRUPT) == 0); data[16] = 5;
    memset(data + 16, 0xff, 8); CHECK("wire overflow", expect_parse(data, sizeof(canonical), OBSCURA64_SIZE_OVERFLOW) == 0);
    memcpy(data, canonical, sizeof(canonical));
    CHECK("alloc protect", obscura64_protect_alloc(OBSCURA64_PROTECTION_NONE,
        payload, sizeof(payload), &allocated, &n) == OBSCURA64_OK && n == sizeof(canonical) && memcmp(allocated, canonical, n) == 0);
    obscura64_free(allocated);
    allocated = (void *)1; n = 123;
    CHECK("alloc failure clearing", obscura64_unprotect_alloc(data, 2, &allocated, &n) == OBSCURA64_ENVELOPE_CORRUPT && allocated == NULL && n == 0);
    allocated = (void *)1; n = 123;
    CHECK("alloc protect argument clearing", obscura64_protect_alloc(OBSCURA64_PROTECTION_NONE,
        NULL, 1, &allocated, &n) == OBSCURA64_INVALID_ARGUMENT && allocated == NULL && n == 0);
    CHECK("alloc unprotect", obscura64_unprotect_alloc(data, sizeof(canonical), &allocated, &n) == OBSCURA64_OK && n == sizeof(payload) && memcmp(allocated, payload, n) == 0);
    obscura64_free(allocated);
    CHECK("empty protect", obscura64_protect(OBSCURA64_PROTECTION_NONE,
        NULL, 0, data, sizeof(data), &n) == OBSCURA64_OK && n == 24);
    CHECK("empty unprotect", obscura64_unprotect(data, 24, NULL, 0, &n) == OBSCURA64_OK && n == 0);
    allocated = (void *)1; n = 123;
    CHECK("empty alloc", obscura64_unprotect_alloc(data, 24, &allocated, &n) == OBSCURA64_OK && allocated == NULL && n == 0);
    CHECK("null envelope", obscura64_unprotect(NULL, 24, output, sizeof(output), &n) == OBSCURA64_INVALID_ARGUMENT && n == 0);
    printf("V2 envelope: %d checks passed\n", checks);
    return 0;
}
