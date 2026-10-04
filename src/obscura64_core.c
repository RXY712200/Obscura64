#include "obscura64.h"
#include "obscura64_internal.h"

const char obscura64_v1_character_pool[OBSCURA64_ALPHABET_SIZE + 1] =
    "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnpqrstuvwxyz23456789@#$%&!?~";

int obscura64_profile_is_valid(const char *profile, size_t length)
{
    unsigned char seen[256] = {0};
    size_t i;

    if (profile == NULL || length != OBSCURA64_ALPHABET_SIZE) {
        return 0;
    }

    for (i = 0; i < length; ++i) {
        const unsigned char value = (unsigned char)profile[i];
        size_t pool_index;
        int found = 0;

        if (value == '\0' || seen[value] != 0) {
            return 0;
        }

        for (pool_index = 0; pool_index < OBSCURA64_ALPHABET_SIZE; ++pool_index) {
            if ((unsigned char)obscura64_v1_character_pool[pool_index] == value) {
                found = 1;
                break;
            }
        }

        if (!found) {
            return 0;
        }

        seen[value] = 1;
    }

    return 1;
}

int obscura64_profile_build_reverse_map(
    const char *profile,
    size_t length,
    uint8_t reverse_map[256])
{
    size_t i;

    if (profile == NULL || reverse_map == NULL ||
        !obscura64_profile_is_valid(profile, length)) {
        return 0;
    }

    for (i = 0; i < 256; ++i) {
        reverse_map[i] = OBSCURA64_REVERSE_INVALID;
    }

    for (i = 0; i < OBSCURA64_ALPHABET_SIZE; ++i) {
        reverse_map[(unsigned char)profile[i]] = (uint8_t)i;
    }

    return 1;
}
