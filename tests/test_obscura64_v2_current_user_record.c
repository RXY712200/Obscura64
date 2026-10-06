#include "obscura64_v2_internal.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static unsigned int checks;
#define CHECK(label, ok) do { ++checks; if (!(ok)) { \
    fprintf(stderr, "FAIL %s line %d\n", label, __LINE__); return 1; \
} } while (0)

int main(void)
{
    const unsigned char raw[] = {0, 1, 255, 0, 'X'};
    const unsigned char fixture[] = {
        'O','B','6','4','C','U','0','1', 1,0,56,0, 0,0,0,0,
        5,0,0,0,0,0,0,0,
        0x3a,0xd4,0xa5,0xe6,0xbc,0xcf,0x51,0xc2,
        0xfc,0xd7,0x18,0x23,0x36,0x2b,0xba,0xcf,
        0x30,0x9e,0x7b,0x71,0xd6,0x69,0x36,0x95,
        0x8c,0xef,0x10,0x86,0xf3,0xad,0x81,0x74,
        0,1,255,0,'X'
    };
    unsigned char damaged[sizeof(fixture) + 1];
    unsigned char *record = NULL;
    const unsigned char *view = NULL;
    size_t record_len = 0, view_len = 0;
    obscura64_status status;
    CHECK("fixed inner record", obscura64_v2_current_user_record_build(raw,
        sizeof(raw), &record, &record_len) == OBSCURA64_OK &&
        record_len == sizeof(fixture) && memcmp(record, fixture, sizeof(fixture)) == 0);
    obscura64_free(record); record = NULL;
    CHECK("fixed record parses", obscura64_v2_current_user_record_parse(fixture,
        sizeof(fixture), &view, &view_len) == OBSCURA64_OK &&
        view_len == sizeof(raw) && memcmp(view, raw, sizeof(raw)) == 0);
    memcpy(damaged, fixture, sizeof(fixture));
    damaged[0] ^= 1;
    CHECK("wrong magic", obscura64_v2_current_user_record_parse(damaged,
        sizeof(fixture), &view, &view_len) == OBSCURA64_PROTECTED_CORRUPT);
    damaged[0] ^= 1; damaged[8] = 2;
    CHECK("wrong version", obscura64_v2_current_user_record_parse(damaged,
        sizeof(fixture), &view, &view_len) == OBSCURA64_PROTECTED_CORRUPT);
    damaged[8] = 1; damaged[10] = 55;
    CHECK("wrong header size", obscura64_v2_current_user_record_parse(damaged,
        sizeof(fixture), &view, &view_len) == OBSCURA64_PROTECTED_CORRUPT);
    damaged[10] = 56; damaged[12] = 1;
    CHECK("nonzero flags", obscura64_v2_current_user_record_parse(damaged,
        sizeof(fixture), &view, &view_len) == OBSCURA64_PROTECTED_CORRUPT);
    damaged[12] = 0; damaged[16] = 4;
    CHECK("length mismatch", obscura64_v2_current_user_record_parse(damaged,
        sizeof(fixture), &view, &view_len) == OBSCURA64_PROTECTED_CORRUPT);
    damaged[16] = 5;
    CHECK("truncated record", obscura64_v2_current_user_record_parse(damaged,
        sizeof(fixture)-1, &view, &view_len) == OBSCURA64_PROTECTED_CORRUPT);
    damaged[sizeof(fixture)] = 0;
    CHECK("trailing byte", obscura64_v2_current_user_record_parse(damaged,
        sizeof(damaged), &view, &view_len) == OBSCURA64_PROTECTED_CORRUPT);
    damaged[24] ^= 1;
    CHECK("digest corruption", obscura64_v2_current_user_record_parse(damaged,
        sizeof(fixture), &view, &view_len) == OBSCURA64_PROTECTED_CORRUPT);
    damaged[24] ^= 1; damaged[56] ^= 1;
    CHECK("payload corruption", obscura64_v2_current_user_record_parse(damaged,
        sizeof(fixture), &view, &view_len) == OBSCURA64_PROTECTED_CORRUPT);
    CHECK("failure clears view", view == NULL && view_len == 0);
    CHECK("short record", obscura64_v2_current_user_record_parse(fixture,
        55, &view, &view_len) == OBSCURA64_PROTECTED_CORRUPT);
    CHECK("empty record built", obscura64_v2_current_user_record_build(NULL,
        0, &record, &record_len) == OBSCURA64_OK && record_len == 56);
    CHECK("empty record parses", obscura64_v2_current_user_record_parse(record,
        record_len, &view, &view_len) == OBSCURA64_OK && view_len == 0);
    obscura64_free(record); record = NULL;
#if SIZE_MAX > UINT32_MAX
    status = obscura64_v2_current_user_record_build(raw,
        (size_t)UINT32_MAX - 55U, &record, &record_len);
    CHECK("complete record DWORD overflow", status == OBSCURA64_SIZE_OVERFLOW && record == NULL);
#else
    (void)status;
#endif
    printf("V2 CurrentUser record: %u checks passed\n", checks);
    return 0;
}
