#include "obscura64.h"
#include "obscura64_v2_internal.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <wincrypt.h>

static unsigned int checks;
#define CHECK(label, ok) do { ++checks; if (!(ok)) { \
    fprintf(stderr, "FAIL %s line %d\n", label, __LINE__); return 1; \
} } while (0)

static int casual(void)
{
    const unsigned char raw[] = {0, 1, 255, 0, 'X'};
    const unsigned char fixture[] = {
        0x01,0,0,0,0,0,0,0,
        0x9e,0xf3,0xb2,0x33,0x9f,0x0f,0x24,0x42,0xfc,0x91,0xb0,0x71,
        0x24,0x3d,0x8f,0xe4,0x03,0xa9,0x06,0x49,0x6a,0x6c,0x5a,0x09,
        0x85,0x6b,0x38,0x42,0x6c,0xae,0x32,0x8a,
        '?','?','S','A','?','6','T','='
    };
    void *body = NULL, *wire = NULL, *plain = NULL;
    size_t body_len = 0, wire_len = 0, plain_len = 0, needed = 0;
    obscura64_data_format format = OBSCURA64_FORMAT_UNKNOWN;
    unsigned char output[sizeof(raw)];
    unsigned char protected_buffer[256];
    CHECK("deterministic fixture build", obscura64_v2_casual_protect_with_profile(
        0, raw, sizeof(raw), &body, &body_len) == OBSCURA64_OK);
    CHECK("frozen fixture", body_len == sizeof(fixture) && memcmp(body, fixture, body_len) == 0);
    obscura64_free(body);
    CHECK("Casual protect", obscura64_protect_alloc(OBSCURA64_PROTECTION_CASUAL,
        raw, sizeof(raw), &wire, &wire_len) == OBSCURA64_OK && wire_len > 24);
    memset(protected_buffer, 0xA5, sizeof(protected_buffer));
    CHECK("Casual protect buffer size", obscura64_protect(OBSCURA64_PROTECTION_CASUAL,
        raw, sizeof(raw), protected_buffer, 1, &needed) == OBSCURA64_BUFFER_TOO_SMALL && needed == wire_len);
    CHECK("Casual protect buffer atomic", protected_buffer[0] == 0xA5);
    CHECK("Casual protect buffer", obscura64_protect(OBSCURA64_PROTECTION_CASUAL,
        raw, sizeof(raw), protected_buffer, sizeof(protected_buffer), &needed) == OBSCURA64_OK && needed == wire_len);
    CHECK("kind field", ((unsigned char *)wire)[12] == 1 && ((unsigned char *)wire)[13] == 0);
    CHECK("Casual automatic unprotect", obscura64_unprotect_alloc_ex(wire, wire_len,
        &plain, &plain_len, &format) == OBSCURA64_OK && format == OBSCURA64_FORMAT_V2 &&
        plain_len == sizeof(raw) && memcmp(plain, raw, sizeof(raw)) == 0);
    obscura64_free(plain); plain = NULL;
    memset(output, 0xA5, sizeof(output));
    CHECK("Casual buffer too small", obscura64_unprotect(wire, wire_len, output,
        sizeof(output)-1, &needed) == OBSCURA64_BUFFER_TOO_SMALL && needed == sizeof(raw));
    CHECK("Casual buffer atomic", output[0] == 0xA5 && output[sizeof(output)-1] == 0xA5);
    CHECK("Casual buffer decode", obscura64_unprotect(wire, wire_len, output,
        sizeof(output), &needed) == OBSCURA64_OK && needed == sizeof(raw) &&
        memcmp(output, raw, sizeof(raw)) == 0);
    ((unsigned char *)wire)[24] = 2;
    CHECK("bad Casual version", obscura64_unprotect_alloc(wire, wire_len,
        &plain, &plain_len) == OBSCURA64_CASUAL_CORRUPT && plain == NULL);
    ((unsigned char *)wire)[24] = 1;
    ((unsigned char *)wire)[26] = 0xFF; ((unsigned char *)wire)[27] = 0xFF;
    CHECK("bad Profile ID", obscura64_unprotect_alloc(wire, wire_len,
        &plain, &plain_len) == OBSCURA64_CASUAL_CORRUPT);
    obscura64_free(wire); wire = NULL;
    CHECK("fixture envelope", obscura64_v2_envelope_build(1, fixture, sizeof(fixture),
        &wire, &wire_len) == OBSCURA64_OK);
    CHECK("known fixture decodes", obscura64_unprotect_alloc(wire, wire_len,
        &plain, &plain_len) == OBSCURA64_OK && plain_len == sizeof(raw) &&
        memcmp(plain, raw, sizeof(raw)) == 0);
    obscura64_free(plain); plain = NULL;
    ((unsigned char *)wire)[24+8] ^= 1;
    CHECK("digest corruption", obscura64_unprotect_alloc(wire, wire_len,
        &plain, &plain_len) == OBSCURA64_CASUAL_CORRUPT);
    ((unsigned char *)wire)[24+8] ^= 1;
    ((unsigned char *)wire)[wire_len-2] = '0';
    CHECK("encoded corruption", obscura64_unprotect_alloc(wire, wire_len,
        &plain, &plain_len) == OBSCURA64_CASUAL_CORRUPT);
    obscura64_free(wire);
    CHECK("empty Casual", obscura64_protect_alloc(OBSCURA64_PROTECTION_CASUAL,
        NULL, 0, &wire, &wire_len) == OBSCURA64_OK);
    CHECK("empty Casual roundtrip", obscura64_unprotect_alloc(wire, wire_len,
        &plain, &plain_len) == OBSCURA64_OK && plain == NULL && plain_len == 0);
    obscura64_free(wire);
    CHECK("Casual size overflow", obscura64_v2_casual_protect_with_profile(
        0, raw, SIZE_MAX, &body, &body_len) == OBSCURA64_SIZE_OVERFLOW && body == NULL);
    return 0;
}

static int current_user(void)
{
    const unsigned char raw[] = {0, 1, 255, 0, 'X'};
    void *wire = NULL, *plain = NULL;
    size_t wire_len = 0, plain_len = 0, needed = 0;
    unsigned char output[sizeof(raw)];
    unsigned char protected_buffer[1024];
    obscura64_data_format format = OBSCURA64_FORMAT_UNKNOWN;
    obscura64_status status;
    CHECK("CurrentUser protect", obscura64_protect_alloc(OBSCURA64_PROTECTION_CURRENT_USER,
        raw, sizeof(raw), &wire, &wire_len) == OBSCURA64_OK && wire_len > 24);
    memset(protected_buffer, 0xA5, sizeof(protected_buffer));
    CHECK("DPAPI protect observed size", obscura64_protect(OBSCURA64_PROTECTION_CURRENT_USER,
        raw, sizeof(raw), protected_buffer, 1, &needed) == OBSCURA64_BUFFER_TOO_SMALL && needed > 24);
    CHECK("DPAPI protect buffer atomic", protected_buffer[0] == 0xA5);
    CHECK("DPAPI protect caller buffer", obscura64_protect(OBSCURA64_PROTECTION_CURRENT_USER,
        raw, sizeof(raw), protected_buffer, sizeof(protected_buffer), &needed) == OBSCURA64_OK && needed > 24);
    CHECK("DPAPI kind", ((unsigned char *)wire)[12] == 2 && ((unsigned char *)wire)[13] == 0);
    CHECK("CurrentUser unprotect", obscura64_unprotect_alloc_ex(wire, wire_len,
        &plain, &plain_len, &format) == OBSCURA64_OK && format == OBSCURA64_FORMAT_V2 &&
        plain_len == sizeof(raw) && memcmp(plain, raw, sizeof(raw)) == 0);
    obscura64_free(plain); plain = NULL;
    memset(output, 0xA5, sizeof(output));
    CHECK("DPAPI observed size", obscura64_unprotect(wire, wire_len, output,
        sizeof(output)-1, &needed) == OBSCURA64_BUFFER_TOO_SMALL && needed == sizeof(raw));
    CHECK("DPAPI atomic", output[0] == 0xA5 && output[sizeof(output)-1] == 0xA5);
    CHECK("DPAPI caller buffer", obscura64_unprotect(wire, wire_len, output,
        sizeof(output), &needed) == OBSCURA64_OK && needed == sizeof(raw) &&
        memcmp(output, raw, sizeof(raw)) == 0);
    CHECK("DPAPI truncated body", obscura64_unprotect_alloc(wire, 24,
        &plain, &plain_len) == OBSCURA64_ENVELOPE_CORRUPT && plain == NULL);
    ((unsigned char *)wire)[24] ^= 1;
    status = obscura64_unprotect_alloc(wire, wire_len, &plain, &plain_len);
    CHECK("DPAPI corrupted body", (status == OBSCURA64_PROTECTION_FAILURE ||
        status == OBSCURA64_PROTECTED_CORRUPT) && plain == NULL);
    obscura64_free(wire); wire = NULL;
    {
        unsigned char *record = NULL;
        size_t record_len = 0;
        DATA_BLOB source, sealed = {0, NULL};
        CHECK("build inner record for integration", obscura64_v2_current_user_record_build(
            raw, sizeof(raw), &record, &record_len) == OBSCURA64_OK);
        record[0] ^= 1; /* DPAPI succeeds, but Obscura64 validation must fail. */
        source.cbData = (DWORD)record_len;
        source.pbData = record;
        CHECK("protect invalid inner record with real DPAPI", CryptProtectData(&source,
            NULL, NULL, NULL, NULL, CRYPTPROTECT_UI_FORBIDDEN, &sealed) != 0);
        obscura64_free(record);
        CHECK("wrap invalid protected record", obscura64_v2_envelope_build(2,
            sealed.pbData, sealed.cbData, &wire, &wire_len) == OBSCURA64_OK);
        LocalFree(sealed.pbData);
        CHECK("DPAPI success does not bypass inner validation",
            obscura64_unprotect_alloc(wire, wire_len, &plain, &plain_len) ==
            OBSCURA64_PROTECTED_CORRUPT && plain == NULL && plain_len == 0);
        obscura64_free(wire); wire = NULL;
    }
    CHECK("empty DPAPI protect", obscura64_protect_alloc(OBSCURA64_PROTECTION_CURRENT_USER,
        NULL, 0, &wire, &wire_len) == OBSCURA64_OK);
    CHECK("empty DPAPI unprotect", obscura64_unprotect_alloc(wire, wire_len,
        &plain, &plain_len) == OBSCURA64_OK && plain == NULL && plain_len == 0);
    obscura64_free(wire);
#if SIZE_MAX > MAXDWORD
    CHECK("DPAPI DWORD overflow", obscura64_v2_current_user_protect(raw,
        (size_t)MAXDWORD + 1U, &wire, &wire_len) == OBSCURA64_SIZE_OVERFLOW && wire == NULL);
    CHECK("DPAPI unprotect DWORD overflow", obscura64_v2_current_user_unprotect(raw,
        (size_t)MAXDWORD + 1U, &plain, &plain_len) == OBSCURA64_SIZE_OVERFLOW && plain == NULL);
#endif
    return 0;
}

int main(void)
{
    if (casual() || current_user()) return 1;
    printf("V2 protection: %u checks passed\n", checks);
    return 0;
}
