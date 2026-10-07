#include "obscura64_v2_internal.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t state = UINT32_C(0x64a29d31);
static uint32_t next_value(void)
{
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

int main(void)
{
    static const unsigned char input[] = {0, 1, 2, 0, 255};
    unsigned char *record = NULL, scratch[256];
    void *casual = NULL;
    size_t record_len = 0, casual_len = 0, iterations = 100000, i;
    if (obscura64_v2_current_user_record_build(input, sizeof(input),
        &record, &record_len) != OBSCURA64_OK ||
        obscura64_v2_casual_protect_with_profile(7, input, sizeof(input),
        &casual, &casual_len) != OBSCURA64_OK) return 1;
    for (i = 0; i < iterations; ++i) {
        const unsigned char *seed;
        const unsigned char *body = (const unsigned char *)1;
        size_t seed_len, length, body_len = 123;
        uint16_t kind = 999;
        obscura64_status status;
        unsigned int mode = (unsigned int)(i % 3U);
        if (mode == 0) {
            static const unsigned char envelope[] = {
                'O','B','6','4','E','N','V','2',2,0,24,0,0,0,0,0,
                5,0,0,0,0,0,0,0,0,1,2,0,255
            };
            seed = envelope; seed_len = sizeof(envelope);
        } else if (mode == 1) {
            seed = record; seed_len = record_len;
        } else {
            seed = (const unsigned char *)casual; seed_len = casual_len;
        }
        if (seed_len > sizeof(scratch)) return 2;
        memcpy(scratch, seed, seed_len);
        length = (next_value() & 7U) == 0 ?
            (size_t)(next_value() % (seed_len + 1U)) : seed_len;
        if (length != 0) scratch[next_value() % length] ^=
            (unsigned char)(1U << (next_value() & 7U));
        if (mode == 0) {
            status = obscura64_v2_envelope_parse(scratch, length,
                &kind, &body, &body_len);
            if ((status != OBSCURA64_OK && (body != NULL || body_len != 0)) ||
                (status == OBSCURA64_OK &&
                 (body == NULL || (uintptr_t)body < (uintptr_t)scratch ||
                  (uintptr_t)body > (uintptr_t)(scratch + length) ||
                  body_len > (uintptr_t)(scratch + length) - (uintptr_t)body))) return 3;
        } else if (mode == 1) {
            status = obscura64_v2_current_user_record_parse(scratch,
                length, &body, &body_len);
            if ((status != OBSCURA64_OK && (body != NULL || body_len != 0)) ||
                (status == OBSCURA64_OK &&
                 (body == NULL || (uintptr_t)body < (uintptr_t)scratch ||
                  (uintptr_t)body > (uintptr_t)(scratch + length) ||
                  body_len > (uintptr_t)(scratch + length) - (uintptr_t)body))) return 4;
        } else {
            void *decoded = (void *)1;
            size_t decoded_len = 123;
            status = obscura64_v2_casual_unprotect(scratch, length,
                &decoded, &decoded_len);
            if (status != OBSCURA64_OK &&
                (decoded != NULL || decoded_len != 0)) return 5;
            obscura64_free(decoded);
        }
    }
    obscura64_free(casual);
    free(record);
    printf("mutation checks=%zu seed=%08x\n", iterations, state);
    return 0;
}
