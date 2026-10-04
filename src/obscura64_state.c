#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>

#include "obscura64_internal.h"
#include "obscura64_profiles_v1.h"

#include <string.h>

#define STATE_OFFSET_MAGIC 0U
#define STATE_OFFSET_VERSION 8U
#define STATE_OFFSET_HEADER_SIZE 10U
#define STATE_OFFSET_TOTAL_SIZE 12U
#define STATE_OFFSET_LIBRARY_VERSION 16U
#define STATE_OFFSET_PROFILE_SIZE 20U
#define STATE_OFFSET_FLAGS 22U
#define STATE_OFFSET_GENERATION 24U
#define STATE_OFFSET_PROJECT_ID 32U
#define STATE_OFFSET_RESERVED 48U
#define STATE_OFFSET_PROFILE 64U

static const unsigned char state_magic[8] = {'O', 'B', '6', '4', 'S', 'T', '0', '1'};

static void write_u16_le(unsigned char *output, uint16_t value)
{
    output[0] = (unsigned char)(value & UINT16_C(0x00FF));
    output[1] = (unsigned char)(value >> 8);
}

static void write_u32_le(unsigned char *output, uint32_t value)
{
    size_t i;
    for (i = 0; i < 4; ++i) {
        output[i] = (unsigned char)(value >> (i * 8));
    }
}

static void write_u64_le(unsigned char *output, uint64_t value)
{
    size_t i;
    for (i = 0; i < 8; ++i) {
        output[i] = (unsigned char)(value >> (i * 8));
    }
}

static uint16_t read_u16_le(const unsigned char *input)
{
    return (uint16_t)((uint16_t)input[0] | ((uint16_t)input[1] << 8));
}

static uint32_t read_u32_le(const unsigned char *input)
{
    return (uint32_t)input[0] | ((uint32_t)input[1] << 8) |
           ((uint32_t)input[2] << 16) | ((uint32_t)input[3] << 24);
}

static uint64_t read_u64_le(const unsigned char *input)
{
    uint64_t value = 0;
    size_t i;
    for (i = 0; i < 8; ++i) {
        value |= (uint64_t)input[i] << (i * 8);
    }
    return value;
}

static NTSTATUS state_hash(const unsigned char *data, unsigned char digest[32])
{
    BCRYPT_ALG_HANDLE algorithm = NULL;
    BCRYPT_HASH_HANDLE hash = NULL;
    NTSTATUS status;

    status = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, NULL, 0);
    if (!BCRYPT_SUCCESS(status)) {
        return status;
    }
    status = BCryptCreateHash(algorithm, &hash, NULL, 0, NULL, 0, 0);
    if (BCRYPT_SUCCESS(status)) {
        status = BCryptHashData(hash, (PUCHAR)data, OBSCURA64_STATE_V1_HASH_OFFSET, 0);
    }
    if (BCRYPT_SUCCESS(status)) {
        status = BCryptFinishHash(hash, digest, 32, 0);
    }
    if (hash != NULL) {
        (void)BCryptDestroyHash(hash);
    }
    (void)BCryptCloseAlgorithmProvider(algorithm, 0);
    return status;
}

static int project_id_is_nonzero(const uint8_t project_id[OBSCURA64_PROJECT_ID_SIZE])
{
    size_t i;
    unsigned char combined = 0;
    for (i = 0; i < OBSCURA64_PROJECT_ID_SIZE; ++i) {
        combined |= project_id[i];
    }
    return combined != 0;
}

int obscura64_state_validate(const obscura64_project_state *state)
{
    uint16_t profile_id;
    if (state == NULL || state->generation == 0 ||
        !project_id_is_nonzero(state->project_id) ||
        !obscura64_profile_is_valid(state->profile, OBSCURA64_PROFILE_SIZE)) {
        return 0;
    }
    return obscura64_profile_library_find(
        (const unsigned char *)state->profile, &profile_id);
}

obscura64_core_status obscura64_state_serialize(
    const obscura64_project_state *state,
    unsigned char output[OBSCURA64_STATE_V1_SIZE])
{
    unsigned char temporary[OBSCURA64_STATE_V1_SIZE] = {0};
    unsigned char digest[32];
    NTSTATUS hash_status;

    if (state == NULL || output == NULL) {
        return OBSCURA64_CORE_STATUS_INVALID_ARGUMENT;
    }
    if (!obscura64_state_validate(state)) {
        return OBSCURA64_CORE_STATUS_INVALID_DATA;
    }

    memcpy(temporary + STATE_OFFSET_MAGIC, state_magic, sizeof(state_magic));
    write_u16_le(temporary + STATE_OFFSET_VERSION, 1);
    write_u16_le(temporary + STATE_OFFSET_HEADER_SIZE, OBSCURA64_STATE_V1_HEADER_SIZE);
    write_u32_le(temporary + STATE_OFFSET_TOTAL_SIZE, OBSCURA64_STATE_V1_SIZE);
    write_u32_le(temporary + STATE_OFFSET_LIBRARY_VERSION, 1);
    write_u16_le(temporary + STATE_OFFSET_PROFILE_SIZE, OBSCURA64_PROFILE_SIZE);
    write_u16_le(temporary + STATE_OFFSET_FLAGS, 0);
    write_u64_le(temporary + STATE_OFFSET_GENERATION, state->generation);
    memcpy(temporary + STATE_OFFSET_PROJECT_ID, state->project_id, OBSCURA64_PROJECT_ID_SIZE);
    memcpy(temporary + STATE_OFFSET_PROFILE, state->profile, OBSCURA64_PROFILE_SIZE);

    hash_status = state_hash(temporary, digest);
    if (!BCRYPT_SUCCESS(hash_status)) {
        return OBSCURA64_CORE_STATUS_CRYPTO_FAILURE;
    }
    memcpy(temporary + OBSCURA64_STATE_V1_HASH_OFFSET, digest, sizeof(digest));
    memcpy(output, temporary, sizeof(temporary));
    return OBSCURA64_CORE_STATUS_SUCCESS;
}

obscura64_core_status obscura64_state_deserialize(
    const unsigned char *data,
    size_t data_len,
    obscura64_project_state *out_state)
{
    obscura64_project_state temporary;
    unsigned char expected_digest[32];
    uint16_t profile_id;
    NTSTATUS hash_status;

    if (data == NULL || out_state == NULL) {
        return OBSCURA64_CORE_STATUS_INVALID_ARGUMENT;
    }
    if (data_len != OBSCURA64_STATE_V1_SIZE) {
        return OBSCURA64_CORE_STATUS_INVALID_DATA;
    }
    if (memcmp(data + STATE_OFFSET_MAGIC, state_magic, sizeof(state_magic)) != 0 ||
        read_u16_le(data + STATE_OFFSET_VERSION) != 1 ||
        read_u16_le(data + STATE_OFFSET_HEADER_SIZE) != OBSCURA64_STATE_V1_HEADER_SIZE ||
        read_u32_le(data + STATE_OFFSET_TOTAL_SIZE) != OBSCURA64_STATE_V1_SIZE ||
        read_u32_le(data + STATE_OFFSET_LIBRARY_VERSION) != 1 ||
        read_u16_le(data + STATE_OFFSET_PROFILE_SIZE) != OBSCURA64_PROFILE_SIZE ||
        read_u16_le(data + STATE_OFFSET_FLAGS) != 0) {
        return OBSCURA64_CORE_STATUS_INVALID_DATA;
    }
    {
        size_t i;
        for (i = 0; i < 16; ++i) {
            if (data[STATE_OFFSET_RESERVED + i] != 0) {
                return OBSCURA64_CORE_STATUS_INVALID_DATA;
            }
        }
    }

    hash_status = state_hash(data, expected_digest);
    if (!BCRYPT_SUCCESS(hash_status)) {
        return OBSCURA64_CORE_STATUS_CRYPTO_FAILURE;
    }
    if (memcmp(data + OBSCURA64_STATE_V1_HASH_OFFSET, expected_digest,
               sizeof(expected_digest)) != 0) {
        return OBSCURA64_CORE_STATUS_INVALID_DATA;
    }

    memcpy(temporary.project_id, data + STATE_OFFSET_PROJECT_ID, OBSCURA64_PROJECT_ID_SIZE);
    temporary.generation = read_u64_le(data + STATE_OFFSET_GENERATION);
    memcpy(temporary.profile, data + STATE_OFFSET_PROFILE, OBSCURA64_PROFILE_SIZE);
    if (temporary.generation == 0 || !project_id_is_nonzero(temporary.project_id) ||
        !obscura64_profile_is_valid(temporary.profile, OBSCURA64_PROFILE_SIZE) ||
        !obscura64_profile_library_find(
            (const unsigned char *)temporary.profile, &profile_id)) {
        return OBSCURA64_CORE_STATUS_INVALID_DATA;
    }

    *out_state = temporary;
    return OBSCURA64_CORE_STATUS_SUCCESS;
}
