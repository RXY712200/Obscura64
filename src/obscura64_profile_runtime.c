#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>

#include "obscura64_internal.h"
#include "obscura64_profiles_v1.h"

#include <string.h>

obscura64_core_status obscura64_profile_library_get(
    uint16_t profile_id,
    uint8_t profile_out[OBSCURA64_PROFILE_SIZE])
{
    if (profile_out == NULL || profile_id >= OBSCURA64_PROFILE_COUNT) {
        return OBSCURA64_CORE_STATUS_INVALID_ARGUMENT;
    }

    memcpy(profile_out, obscura64_profiles_v1[profile_id], OBSCURA64_PROFILE_SIZE);
    return OBSCURA64_CORE_STATUS_SUCCESS;
}

int obscura64_profile_library_find(
    const unsigned char profile[OBSCURA64_PROFILE_SIZE],
    uint16_t *profile_id)
{
    uint16_t id;

    if (profile == NULL || profile_id == NULL) {
        return 0;
    }

    for (id = 0; id < OBSCURA64_PROFILE_COUNT; ++id) {
        if (memcmp(profile, obscura64_profiles_v1[id], OBSCURA64_PROFILE_SIZE) == 0) {
            *profile_id = id;
            return 1;
        }
    }

    return 0;
}

obscura64_core_status obscura64_profile_runtime_select_random(
    uint16_t *profile_id,
    uint8_t profile_out[OBSCURA64_PROFILE_SIZE])
{
    uint32_t random_value;
    uint16_t selected_id;
    uint8_t selected_profile[OBSCURA64_PROFILE_SIZE];
    NTSTATUS status;

    if (profile_id == NULL || profile_out == NULL) {
        return OBSCURA64_CORE_STATUS_INVALID_ARGUMENT;
    }

    status = BCryptGenRandom(NULL, (PUCHAR)&random_value,
                             (ULONG)sizeof(random_value),
                             BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if (!BCRYPT_SUCCESS(status)) {
        return OBSCURA64_CORE_STATUS_RNG_FAILURE;
    }

    selected_id = (uint16_t)(random_value & UINT32_C(0x0FFF));
    {
        const obscura64_core_status lookup_status =
            obscura64_profile_library_get(selected_id, selected_profile);
        if (lookup_status != OBSCURA64_CORE_STATUS_SUCCESS) {
            return lookup_status;
        }
    }

    memcpy(profile_out, selected_profile, OBSCURA64_PROFILE_SIZE);
    *profile_id = selected_id;
    return OBSCURA64_CORE_STATUS_SUCCESS;
}
