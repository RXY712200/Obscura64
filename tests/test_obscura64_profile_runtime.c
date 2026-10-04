#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>

#include "obscura64_internal.h"
#include "obscura64_profiles_v1.h"

#include <stdio.h>
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
    static const uint16_t boundary_ids[] = {0, 1, 4094, 4095};
    uint8_t profile[OBSCURA64_PROFILE_SIZE];
    uint8_t before[OBSCURA64_PROFILE_SIZE];
    size_t i;
    int saw_different_id = 0;
    int have_first_random_id = 0;
    uint16_t first_id = UINT16_MAX;
    size_t random_successes = 0;

    for (i = 0; i < sizeof(boundary_ids) / sizeof(boundary_ids[0]); ++i) {
        const uint16_t id = boundary_ids[i];
        const obscura64_core_status status = obscura64_profile_library_get(id, profile);
        check(status == OBSCURA64_CORE_STATUS_SUCCESS, "boundary Profile ID lookup succeeds");
        check(memcmp(profile, obscura64_profiles_v1[id], OBSCURA64_PROFILE_SIZE) == 0,
              "boundary lookup copies exact 64 bytes");
    }

    memset(profile, 0xA5, sizeof(profile));
    memcpy(before, profile, sizeof(profile));
    check(obscura64_profile_library_get(4096, profile) == OBSCURA64_CORE_STATUS_INVALID_ARGUMENT,
          "Profile ID 4096 is rejected");
    check(memcmp(profile, before, sizeof(profile)) == 0,
          "ID 4096 failure leaves output unchanged");
    check(obscura64_profile_library_get(UINT16_MAX, profile) ==
              OBSCURA64_CORE_STATUS_INVALID_ARGUMENT,
          "Profile ID 65535 is rejected");
    check(memcmp(profile, before, sizeof(profile)) == 0,
          "ID 65535 failure leaves output unchanged");
    check(obscura64_profile_library_get(0, NULL) == OBSCURA64_CORE_STATUS_INVALID_ARGUMENT,
          "NULL lookup output is rejected");

    for (i = 0; i < OBSCURA64_PROFILE_COUNT; ++i) {
        const obscura64_core_status status = obscura64_profile_library_get((uint16_t)i, profile);
        check(status == OBSCURA64_CORE_STATUS_SUCCESS,
              "all Profile IDs lookup successfully");
        check(obscura64_profile_is_valid((const char *)profile, OBSCURA64_PROFILE_SIZE),
              "lookup returns a valid Profile");
        check(memcmp(profile, obscura64_profiles_v1[i], OBSCURA64_PROFILE_SIZE) == 0,
              "lookup returns the exact embedded Profile");
    }

    for (i = 0; i < 4096; ++i) {
        uint16_t id = UINT16_MAX;
        const obscura64_core_status status =
            obscura64_profile_runtime_select_random(&id, profile);
        check(status == OBSCURA64_CORE_STATUS_SUCCESS,
              "random selector succeeds");
        if (status != OBSCURA64_CORE_STATUS_SUCCESS) {
            continue;
        }
        ++random_successes;
        check(id < OBSCURA64_PROFILE_COUNT, "random Profile ID is in range");
        check(obscura64_profile_is_valid((const char *)profile, OBSCURA64_PROFILE_SIZE),
              "random selector returns a valid Profile");
        check(memcmp(profile, obscura64_profiles_v1[id], OBSCURA64_PROFILE_SIZE) == 0,
              "random selector copies Profile matching its ID");
        if (!have_first_random_id) {
            first_id = id;
            have_first_random_id = 1;
        } else if (id != first_id) {
            saw_different_id = 1;
        }
    }
    check(random_successes == 4096, "all 4096 random selections succeeded");
    check(saw_different_id, "random sample includes at least two IDs");

    memset(profile, 0x3C, sizeof(profile));
    memcpy(before, profile, sizeof(profile));
    check(obscura64_profile_runtime_select_random(NULL, profile) ==
              OBSCURA64_CORE_STATUS_INVALID_ARGUMENT,
          "NULL random ID output is rejected");
    check(memcmp(profile, before, sizeof(profile)) == 0,
          "invalid ID output pointer leaves Profile unchanged");
    {
        uint16_t id = UINT16_C(0x5A5A);
        check(obscura64_profile_runtime_select_random(&id, NULL) ==
                  OBSCURA64_CORE_STATUS_INVALID_ARGUMENT,
              "NULL random Profile output is rejected");
        check(id == UINT16_C(0x5A5A),
              "invalid Profile output pointer leaves ID unchanged");
    }

    if (failures != 0) {
        fprintf(stderr, "%zu of %zu runtime Profile checks failed\n", failures, checks);
        return 1;
    }
    printf("runtime Profile checks passed: %zu\n", checks);
    return 0;
}
