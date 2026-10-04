#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>

#include "obscura64_internal.h"
#include "obscura64_profiles_v1.h"

#include <stdio.h>
#include <string.h>

static unsigned int checks;

#define CHECK(condition, label) do { \
    ++checks; \
    if (!(condition)) { \
        fprintf(stderr, "FAIL: %s\n", label); \
        return 1; \
    } \
} while (0)

static void set_u16(unsigned char *p, uint16_t v)
{
    p[0] = (unsigned char)v;
    p[1] = (unsigned char)(v >> 8);
}

static void set_u32(unsigned char *p, uint32_t v)
{
    size_t i;
    for (i = 0; i < 4; ++i) p[i] = (unsigned char)(v >> (8 * i));
}

static void set_u64(unsigned char *p, uint64_t v)
{
    size_t i;
    for (i = 0; i < 8; ++i) p[i] = (unsigned char)(v >> (8 * i));
}

static int rehash(unsigned char blob[OBSCURA64_STATE_V1_SIZE])
{
    BCRYPT_ALG_HANDLE algorithm = NULL;
    BCRYPT_HASH_HANDLE hash = NULL;
    unsigned char digest[32];
    NTSTATUS status = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, NULL, 0);
    if (!BCRYPT_SUCCESS(status)) return 0;
    status = BCryptCreateHash(algorithm, &hash, NULL, 0, NULL, 0, 0);
    if (BCRYPT_SUCCESS(status)) status = BCryptHashData(hash, blob, 128, 0);
    if (BCRYPT_SUCCESS(status)) status = BCryptFinishHash(hash, digest, sizeof(digest), 0);
    if (hash != NULL) (void)BCryptDestroyHash(hash);
    (void)BCryptCloseAlgorithmProvider(algorithm, 0);
    if (!BCRYPT_SUCCESS(status)) return 0;
    memcpy(blob + 128, digest, sizeof(digest));
    return 1;
}

static void init_state(obscura64_project_state *state, uint16_t profile_id, uint64_t generation)
{
    size_t i;
    memset(state, 0, sizeof(*state));
    for (i = 0; i < OBSCURA64_PROJECT_ID_SIZE; ++i) {
        state->project_id[i] = (uint8_t)(i + 1);
    }
    state->generation = generation;
    memcpy(state->profile, obscura64_profiles_v1[profile_id], OBSCURA64_PROFILE_SIZE);
}

static int expect_invalid_preserves(const unsigned char *blob, size_t length)
{
    obscura64_project_state out;
    obscura64_project_state before;
    memset(&out, 0xA5, sizeof(out));
    before = out;
    if (obscura64_state_deserialize(blob, length, &out) == OBSCURA64_CORE_STATUS_SUCCESS) return 0;
    return memcmp(&out, &before, sizeof(out)) == 0;
}

int main(void)
{
    obscura64_project_state input, output;
    unsigned char blob[OBSCURA64_STATE_V1_SIZE];
    unsigned char altered[OBSCURA64_STATE_V1_SIZE];
    size_t i;
    int found_external = 0;
    char outside[OBSCURA64_PROFILE_SIZE];

    init_state(&input, 0, 1);
    CHECK(obscura64_state_validate(&input), "valid state profile 0");
    CHECK(obscura64_state_serialize(&input, blob) == OBSCURA64_CORE_STATUS_SUCCESS,
          "serialize profile 0 generation 1");
    CHECK(obscura64_state_deserialize(blob, sizeof(blob), &output) == OBSCURA64_CORE_STATUS_SUCCESS,
          "deserialize profile 0 generation 1");
    CHECK(memcmp(&input, &output, sizeof(input)) == 0, "round trip profile 0");

    init_state(&input, 4095, 27);
    CHECK(obscura64_state_serialize(&input, blob) == OBSCURA64_CORE_STATUS_SUCCESS,
          "serialize profile 4095 later generation");
    CHECK(obscura64_state_deserialize(blob, sizeof(blob), &output) == OBSCURA64_CORE_STATUS_SUCCESS,
          "deserialize profile 4095 later generation");
    CHECK(memcmp(&input, &output, sizeof(input)) == 0, "round trip profile 4095");

    CHECK(sizeof(blob) == 160, "serialized size");
    CHECK(memcmp(blob, "OB64ST01", 8) == 0, "magic");
    CHECK(blob[8] == 1 && blob[9] == 0, "format version");
    CHECK(blob[10] == 64 && blob[11] == 0, "header size");
    CHECK(blob[12] == 160 && blob[13] == 0 && blob[14] == 0 && blob[15] == 0,
          "total size");
    CHECK(blob[16] == 1 && blob[17] == 0 && blob[18] == 0 && blob[19] == 0,
          "library version");
    CHECK(blob[20] == 64 && blob[21] == 0, "profile size");
    CHECK(blob[22] == 0 && blob[23] == 0, "flags");
    for (i = 48; i < 64; ++i) CHECK(blob[i] == 0, "reserved bytes zero");
    CHECK(memcmp(blob + 64, input.profile, 64) == 0, "full profile stored");
    CHECK(obscura64_profile_library_find(blob + 64, &(uint16_t){0}), "blob profile is library member");

    memcpy(altered, blob, sizeof(blob)); altered[0] ^= 1;
    CHECK(expect_invalid_preserves(altered, sizeof(altered)), "SHA/header corruption rejected");
    memcpy(altered, blob, sizeof(blob)); altered[24] ^= 1;
    CHECK(expect_invalid_preserves(altered, sizeof(altered)), "SHA/generation corruption rejected");
    memcpy(altered, blob, sizeof(blob)); altered[32] ^= 1;
    CHECK(expect_invalid_preserves(altered, sizeof(altered)), "SHA/project id corruption rejected");
    memcpy(altered, blob, sizeof(blob)); altered[64] ^= 1;
    CHECK(expect_invalid_preserves(altered, sizeof(altered)), "SHA/profile corruption rejected");
    memcpy(altered, blob, sizeof(blob)); altered[128] ^= 1;
    CHECK(expect_invalid_preserves(altered, sizeof(altered)), "digest corruption rejected");
    CHECK(expect_invalid_preserves(blob, 159), "short blob rejected");
    CHECK(expect_invalid_preserves(blob, 161), "long blob rejected");

#define STRUCTURAL_CASE(label, offset, value, width) do { \
    memcpy(altered, blob, sizeof(blob)); \
    if ((width) == 1) altered[(offset)] = (unsigned char)(value); \
    else if ((width) == 2) set_u16(altered + (offset), (uint16_t)(value)); \
    else if ((width) == 4) set_u32(altered + (offset), (uint32_t)(value)); \
    else set_u64(altered + (offset), (uint64_t)(value)); \
    CHECK(rehash(altered), "rehash structural test blob"); \
    CHECK(expect_invalid_preserves(altered, sizeof(altered)), label); \
} while (0)
    STRUCTURAL_CASE("version rejected", 8, 2, 2);
    STRUCTURAL_CASE("header size rejected", 10, 63, 2);
    STRUCTURAL_CASE("total size rejected", 12, 159, 4);
    STRUCTURAL_CASE("library version rejected", 16, 2, 4);
    STRUCTURAL_CASE("profile size rejected", 20, 63, 2);
    STRUCTURAL_CASE("flags rejected", 22, 1, 2);
    STRUCTURAL_CASE("reserved byte rejected", 53, 1, 1);
    STRUCTURAL_CASE("zero generation rejected", 24, 0, 8);
    memcpy(altered, blob, sizeof(blob)); memset(altered + 32, 0, 16);
    CHECK(rehash(altered), "rehash zero project id blob");
    CHECK(expect_invalid_preserves(altered, sizeof(altered)), "zero project id rejected");

    memcpy(altered, blob, sizeof(blob)); altered[64] = '=';
    CHECK(rehash(altered), "rehash illegal profile blob");
    CHECK(expect_invalid_preserves(altered, sizeof(altered)), "illegal profile rejected");
    memcpy(altered, blob, sizeof(blob)); altered[65] = altered[64];
    CHECK(rehash(altered), "rehash duplicate profile blob");
    CHECK(expect_invalid_preserves(altered, sizeof(altered)), "duplicate profile rejected");

    for (i = 1; i < OBSCURA64_PROFILE_SIZE && !found_external; ++i) {
        size_t j;
        for (j = 0; j < OBSCURA64_PROFILE_SIZE; ++j) {
            outside[j] = obscura64_v1_character_pool[(j + i) % OBSCURA64_PROFILE_SIZE];
        }
        if (obscura64_profile_is_valid(outside, OBSCURA64_PROFILE_SIZE) &&
            !obscura64_profile_library_find((const unsigned char *)outside, &(uint16_t){0})) {
            found_external = 1;
        }
    }
    CHECK(found_external, "deterministically found valid profile outside library");
    memcpy(altered, blob, sizeof(blob)); memcpy(altered + 64, outside, sizeof(outside));
    CHECK(rehash(altered), "rehash external profile blob");
    CHECK(expect_invalid_preserves(altered, sizeof(altered)), "external valid permutation rejected");

    init_state(&input, 0, 0);
    memset(blob, 0x5A, sizeof(blob));
    memcpy(altered, blob, sizeof(blob));
    CHECK(obscura64_state_serialize(&input, blob) != OBSCURA64_CORE_STATUS_SUCCESS,
          "invalid serialize fails");
    CHECK(memcmp(blob, altered, sizeof(blob)) == 0, "serialize failure atomic");
    CHECK(expect_invalid_preserves(altered, sizeof(altered)), "deserialize failure atomic");

    printf("PASS: %u state checks\n", checks);
    return 0;
}
