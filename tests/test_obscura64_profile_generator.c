#include "obscura64_internal.h"
#include "obscura64_profile_generator_internal.h"

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

int main(void)
{
    char first_profile[OBSCURA64_ALPHABET_SIZE + 1];
    char profile[OBSCURA64_ALPHABET_SIZE + 1];
    size_t i;
    int have_first = 0;
    int differs_from_first = 0;
    size_t generated_count = 0;

    for (i = 0; i < 256; ++i) {
        const obscura64_profile_generator_status status =
            obscura64_profile_generator_generate(profile);

        check(status == OBSCURA64_PROFILE_GENERATOR_SUCCESS,
              "generate one Profile successfully");
        if (status != OBSCURA64_PROFILE_GENERATOR_SUCCESS) {
            continue;
        }
        ++generated_count;
        check(obscura64_profile_is_valid(profile, OBSCURA64_ALPHABET_SIZE),
              "generated Profile passes shared validation");
        check(memchr(profile, '=', OBSCURA64_ALPHABET_SIZE) == NULL,
              "generated Profile excludes padding");
        check(memchr(profile, '\0', OBSCURA64_ALPHABET_SIZE) == NULL,
              "generated Profile has no embedded NUL");
        if (!have_first) {
            memcpy(first_profile, profile, sizeof(first_profile));
            have_first = 1;
        } else if (memcmp(first_profile, profile, OBSCURA64_ALPHABET_SIZE) != 0) {
            differs_from_first = 1;
        }
    }

    check(generated_count == 256, "all 256 Profile generations succeeded");
    check(differs_from_first,
          "generated sample is not always identical");

    {
        static const uint32_t bounds[] = {1, 2, 3, 7, 8, 16, 63, 64};
        size_t bound_index;
        for (bound_index = 0;
             bound_index < sizeof(bounds) / sizeof(bounds[0]);
             ++bound_index) {
            size_t trial;
            for (trial = 0; trial < 100; ++trial) {
                uint32_t value = UINT32_MAX;
                const obscura64_profile_generator_status status =
                    obscura64_profile_generator_random_uniform(bounds[bound_index], &value);
                check(status == OBSCURA64_PROFILE_GENERATOR_SUCCESS,
                      "uniform bounded random call succeeds");
                check(status == OBSCURA64_PROFILE_GENERATOR_SUCCESS &&
                          value < bounds[bound_index],
                      "uniform bounded random result is in range");
            }
        }
    }

    check(obscura64_profile_generator_random_uniform(0, &(uint32_t){0}) ==
              OBSCURA64_PROFILE_GENERATOR_INVALID_ARGUMENT,
          "zero random bound is rejected");
    check(obscura64_profile_generator_random_uniform(1, NULL) ==
              OBSCURA64_PROFILE_GENERATOR_INVALID_ARGUMENT,
          "NULL random output is rejected");
    check(obscura64_profile_generator_generate(NULL) ==
              OBSCURA64_PROFILE_GENERATOR_INVALID_ARGUMENT,
          "NULL Profile output is rejected");

    {
        const size_t count = OBSCURA64_PROFILE_LIBRARY_COUNT;
        uint8_t *profiles = (uint8_t *)malloc(count * OBSCURA64_PROFILE_BYTES);
        obscura64_profile_generator_stats stats;
        size_t pair_comparisons = 0;
        obscura64_profile_generator_status status;

        check(profiles != NULL, "allocate full candidate set for test");
        if (profiles != NULL) {
            status = obscura64_profile_generator_generate_unique_set(
                profiles, count, &stats);
            check(status == OBSCURA64_PROFILE_GENERATOR_SUCCESS,
                  "generate 4096 unique Profiles in memory");
            check(status == OBSCURA64_PROFILE_GENERATOR_SUCCESS &&
                      stats.accepted_count == count,
                  "unique generator accepted count is 4096");
            check(status == OBSCURA64_PROFILE_GENERATOR_SUCCESS &&
                      stats.generation_attempts == count + stats.collisions,
                  "generation attempts account for collisions");
            status = obscura64_profile_generator_validate_unique_set(
                profiles, count, &pair_comparisons);
            check(status == OBSCURA64_PROFILE_GENERATOR_SUCCESS,
                  "full generated set validates");
            check(pair_comparisons == (size_t)8386560,
                  "full set compares all 8386560 pairs");
            free(profiles);
        }
    }

    {
        uint8_t set[4 * OBSCURA64_PROFILE_BYTES];
        size_t pair_comparisons = 0;
        size_t profile_index;
        for (profile_index = 0; profile_index < 3; ++profile_index) {
            size_t character_index;
            for (character_index = 0; character_index < OBSCURA64_PROFILE_BYTES;
                 ++character_index) {
                size_t source_index;
                if (profile_index == 0) {
                    source_index = character_index;
                } else if (profile_index == 1) {
                    source_index = OBSCURA64_PROFILE_BYTES - 1 - character_index;
                } else {
                    source_index = (character_index + 17) % OBSCURA64_PROFILE_BYTES;
                }
                set[profile_index * OBSCURA64_PROFILE_BYTES + character_index] =
                    (uint8_t)obscura64_v1_character_pool[source_index];
            }
        }
        memcpy(set + 3 * OBSCURA64_PROFILE_BYTES, set, OBSCURA64_PROFILE_BYTES);
        check(obscura64_profile_generator_validate_unique_set(set, 4, &pair_comparisons) ==
                  OBSCURA64_PROFILE_GENERATOR_DUPLICATE,
              "deterministic duplicate Profile is detected");
    }

    {
        uint8_t set[3 * OBSCURA64_PROFILE_BYTES];
        size_t pair_comparisons = 0;
        size_t profile_index;
        for (profile_index = 0; profile_index < 3; ++profile_index) {
            size_t character_index;
            for (character_index = 0; character_index < OBSCURA64_PROFILE_BYTES;
                 ++character_index) {
                size_t source_index;
                if (profile_index == 0) {
                    source_index = character_index;
                } else if (profile_index == 1) {
                    source_index = OBSCURA64_PROFILE_BYTES - 1 - character_index;
                } else {
                    source_index = (character_index + 23) % OBSCURA64_PROFILE_BYTES;
                }
                set[profile_index * OBSCURA64_PROFILE_BYTES + character_index] =
                    (uint8_t)obscura64_v1_character_pool[source_index];
            }
        }
        check(obscura64_profile_generator_validate_unique_set(set, 3, &pair_comparisons) ==
                  OBSCURA64_PROFILE_GENERATOR_SUCCESS,
              "deterministic distinct Profile set is accepted");
        check(pair_comparisons == 3,
              "small deterministic set validates every pair");
    }

    if (failures != 0) {
        fprintf(stderr, "%zu of %zu generator checks failed\n", failures, checks);
        return 1;
    }
    printf("profile generator checks passed: %zu\n", checks);
    return 0;
}
