#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>

#include "obscura64_internal.h"
#include "obscura64_profiles_v1.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t checks;
static size_t failures;

static void check(int condition, const char *name)
{
    ++checks;
    if (!condition) {
        ++failures;
        fprintf(stderr, "FAIL: %s\n", name);
    }
}

static int embedded_sha256(char output[65])
{
    static const char digits[] = "0123456789abcdef";
    BCRYPT_ALG_HANDLE algorithm = NULL;
    BCRYPT_HASH_HANDLE hash = NULL;
    PUCHAR object = NULL;
    ULONG object_length = 0;
    ULONG returned = 0;
    uint8_t digest[32];
    NTSTATUS status;
    size_t i;

    status = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM,
                                         NULL, 0);
    if (!BCRYPT_SUCCESS(status)) {
        return 0;
    }
    status = BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
                               (PUCHAR)&object_length, sizeof(object_length),
                               &returned, 0);
    if (!BCRYPT_SUCCESS(status) || object_length == 0) {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return 0;
    }
    object = (PUCHAR)malloc(object_length);
    if (object == NULL) {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return 0;
    }
    status = BCryptCreateHash(algorithm, &hash, object, object_length, NULL, 0, 0);
    if (BCRYPT_SUCCESS(status)) {
        status = BCryptHashData(hash, (PUCHAR)&obscura64_profiles_v1[0][0],
                                (ULONG)sizeof(obscura64_profiles_v1), 0);
    }
    if (BCRYPT_SUCCESS(status)) {
        status = BCryptFinishHash(hash, digest, sizeof(digest), 0);
    }
    if (hash != NULL) {
        BCryptDestroyHash(hash);
    }
    free(object);
    BCryptCloseAlgorithmProvider(algorithm, 0);
    if (!BCRYPT_SUCCESS(status)) {
        return 0;
    }
    for (i = 0; i < sizeof(digest); ++i) {
        output[2 * i] = digits[digest[i] >> 4];
        output[2 * i + 1] = digits[digest[i] & 0x0F];
    }
    output[64] = '\0';
    return 1;
}

int main(void)
{
    static const char expected_profile_0[] =
        "?npNQ6USPfrhsX3Eukx4bYtjKBDVcCd9T!78W&zHRJi$FLa5Mw@~yvG2meZ#qg%A";
    static const char expected_profile_1[] =
        "Pf!5X9uhEvrgaLy$~KBtFie2w3?Wq8sk7C&UVQ%cYdnmD@HbjRNpSATG6z4ZM#xJ";
    static const char expected_profile_4095[] =
        "uzgA?HQ8e3PsFTqW6&#tiKcRU2Y!w5CNdSLxaDjE~n7Mb$XmfprhZJkBv9y4V%G@";
    const size_t payload_size = (size_t)OBSCURA64_PROFILE_COUNT * OBSCURA64_PROFILE_SIZE;
    uint8_t *canonical;
    FILE *file;
    size_t i;
    int every_profile_valid = 1;
    int no_duplicate_pairs = 1;
    char embedded_hash[65];

    check(OBSCURA64_PROFILE_COUNT == 4096U, "Profile count constant is 4096");
    check(OBSCURA64_PROFILE_SIZE == 64U, "Profile size constant is 64");
    check(payload_size == 262144U, "canonical payload size is 262144");
    check(sizeof(obscura64_profiles_v1) == 262144U,
          "embedded array sizeof is exactly 262144");

    for (i = 0; i < OBSCURA64_PROFILE_COUNT; ++i) {
        if (!obscura64_profile_is_valid((const char *)obscura64_profiles_v1[i],
                                   OBSCURA64_PROFILE_SIZE)) {
            every_profile_valid = 0;
            fprintf(stderr, "invalid embedded Profile at ID %04u\n", (unsigned int)i);
        }
        ++checks;
    }
    check(every_profile_valid, "all embedded Profiles pass validation");

    for (i = 0; i < OBSCURA64_PROFILE_COUNT; ++i) {
        size_t j;
        for (j = i + 1; j < OBSCURA64_PROFILE_COUNT; ++j) {
            ++checks;
            if (memcmp(obscura64_profiles_v1[i], obscura64_profiles_v1[j],
                       OBSCURA64_PROFILE_SIZE) == 0) {
                no_duplicate_pairs = 0;
            }
        }
    }
    check(no_duplicate_pairs, "all 8386560 Profile pairs are unique");

    check(memcmp(obscura64_profiles_v1[0], expected_profile_0, OBSCURA64_PROFILE_SIZE) == 0,
          "Profile 0 matches frozen candidate");
    check(memcmp(obscura64_profiles_v1[1], expected_profile_1, OBSCURA64_PROFILE_SIZE) == 0,
          "Profile 1 matches frozen candidate");
    check(memcmp(obscura64_profiles_v1[4095], expected_profile_4095,
                 OBSCURA64_PROFILE_SIZE) == 0,
          "Profile 4095 matches frozen candidate");

    canonical = (uint8_t *)malloc(payload_size);
    check(canonical != NULL, "allocate canonical binary comparison buffer");
    if (canonical == NULL) {
        return 1;
    }
    file = fopen("generated/obscura64_profiles_v1.bin", "rb");
    check(file != NULL, "open canonical binary");
    if (file == NULL) {
        free(canonical);
        return 1;
    }
    check(fread(canonical, 1, payload_size, file) == payload_size,
          "read complete canonical payload");
    check(fgetc(file) == EOF && !ferror(file),
          "canonical binary has no extra bytes");
    check(fclose(file) == 0, "close canonical binary");
    check(memcmp(canonical, &obscura64_profiles_v1[0][0], payload_size) == 0,
          "canonical binary equals embedded bytes");
    free(canonical);

    check(embedded_sha256(embedded_hash), "compute embedded BCrypt SHA-256");
    check(strcmp(embedded_hash, OBSCURA64_PROFILE_LIBRARY_V1_SHA256) == 0,
          "embedded SHA-256 matches frozen constant");

    if (failures != 0) {
        fprintf(stderr, "%zu of %zu Profile Library checks failed\n", failures, checks);
        return 1;
    }
    printf("Profile Library checks passed: %zu\n", checks);
    return 0;
}
