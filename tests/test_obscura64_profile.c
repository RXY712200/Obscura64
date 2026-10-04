#include "obscura64_internal.h"

#include <stdio.h>
#include <string.h>

static int check(int condition, const char *name)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", name);
        return 0;
    }
    return 1;
}

int main(void)
{
    static const char expected_pool[] =
        "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnpqrstuvwxyz23456789@#$%&!?~";
    char profile[OBSCURA64_ALPHABET_SIZE];
    char invalid_profile[OBSCURA64_ALPHABET_SIZE];
    unsigned char pool_seen[256] = {0};
    uint8_t reverse_map[256];
    size_t i;
    int failures = 0;

    failures += !check(sizeof(expected_pool) - 1 == OBSCURA64_ALPHABET_SIZE,
                       "pool has exactly 64 characters");
    failures += !check(memcmp(obscura64_v1_character_pool, expected_pool,
                              OBSCURA64_ALPHABET_SIZE + 1) == 0,
                       "pool exactly matches V1 specification");
    for (i = 0; i < OBSCURA64_ALPHABET_SIZE; ++i) {
        const unsigned char value = (unsigned char)obscura64_v1_character_pool[i];
        if (value == '=' || pool_seen[value] != 0) {
            failures += !check(0, "pool characters are unique and exclude '='");
            break;
        }
        pool_seen[value] = 1;
    }
    if (i == OBSCURA64_ALPHABET_SIZE) {
        failures += !check(1, "pool characters are unique and exclude '='");
    }

    memcpy(profile, obscura64_v1_character_pool, OBSCURA64_ALPHABET_SIZE);
    failures += !check(obscura64_profile_is_valid(profile, sizeof(profile)),
                       "fixed pool is a valid profile");

    for (i = 0; i < OBSCURA64_ALPHABET_SIZE; ++i) {
        profile[i] = obscura64_v1_character_pool[OBSCURA64_ALPHABET_SIZE - 1 - i];
    }
    failures += !check(obscura64_profile_is_valid(profile, sizeof(profile)),
                       "reverse pool order is a valid profile");

    failures += !check(!obscura64_profile_is_valid(NULL, OBSCURA64_ALPHABET_SIZE),
                       "NULL profile is invalid");
    failures += !check(!obscura64_profile_is_valid(profile, 0), "zero length is invalid");
    failures += !check(!obscura64_profile_is_valid(profile, 63), "length 63 is invalid");
    failures += !check(!obscura64_profile_is_valid(profile, 65), "length 65 is invalid");

    memcpy(invalid_profile, profile, sizeof(profile));
    invalid_profile[0] = invalid_profile[1];
    failures += !check(!obscura64_profile_is_valid(invalid_profile, sizeof(invalid_profile)),
                       "duplicate character is invalid");

    memcpy(invalid_profile, profile, sizeof(profile));
    invalid_profile[0] = '=';
    failures += !check(!obscura64_profile_is_valid(invalid_profile, sizeof(invalid_profile)),
                       "'=' is invalid");
    memcpy(invalid_profile, profile, sizeof(profile));
    invalid_profile[0] = '0';
    failures += !check(!obscura64_profile_is_valid(invalid_profile, sizeof(invalid_profile)),
                       "'0' is invalid");
    memcpy(invalid_profile, profile, sizeof(profile));
    invalid_profile[0] = 'I';
    failures += !check(!obscura64_profile_is_valid(invalid_profile, sizeof(invalid_profile)),
                       "'I' is invalid");
    memcpy(invalid_profile, profile, sizeof(profile));
    invalid_profile[OBSCURA64_ALPHABET_SIZE / 2] = '\0';
    failures += !check(!obscura64_profile_is_valid(invalid_profile, sizeof(invalid_profile)),
                       "embedded NUL is invalid");

    failures += !check(obscura64_profile_build_reverse_map(profile, sizeof(profile),
                                                      reverse_map),
                       "reverse map builds for valid profile");
    for (i = 0; i < OBSCURA64_ALPHABET_SIZE; ++i) {
        if (reverse_map[(unsigned char)profile[i]] != i) {
            failures += !check(0, "reverse map maps every profile character");
            break;
        }
    }
    if (i == OBSCURA64_ALPHABET_SIZE) {
        failures += !check(1, "reverse map maps every profile character");
    }
    failures += !check(reverse_map[(unsigned char)'0'] == OBSCURA64_REVERSE_INVALID &&
                       reverse_map[(unsigned char)'I'] == OBSCURA64_REVERSE_INVALID &&
                       reverse_map[(unsigned char)'='] == OBSCURA64_REVERSE_INVALID,
                       "reverse map marks non-profile characters invalid");

    memset(reverse_map, 0x5A, sizeof(reverse_map));
    memcpy(invalid_profile, profile, sizeof(profile));
    invalid_profile[0] = '=';
    failures += !check(!obscura64_profile_build_reverse_map(
                           invalid_profile, sizeof(invalid_profile), reverse_map),
                       "reverse map rejects invalid profile");
    for (i = 0; i < sizeof(reverse_map); ++i) {
        if (reverse_map[i] != UINT8_C(0x5A)) {
            failures += !check(0, "reverse map unchanged after invalid profile");
            break;
        }
    }
    if (i == sizeof(reverse_map)) {
        failures += !check(1, "reverse map unchanged after invalid profile");
    }
    failures += !check(!obscura64_profile_build_reverse_map(profile, sizeof(profile), NULL),
                       "NULL reverse map is rejected");
    failures += !check(!obscura64_profile_build_reverse_map(NULL, OBSCURA64_ALPHABET_SIZE,
                                                       reverse_map),
                       "NULL profile rejected by reverse map builder");

    if (failures != 0) {
        fprintf(stderr, "%d profile test check(s) failed\n", failures);
        return 1;
    }

    return 0;
}
