#include "obscura64.h"
#include "obscura64_internal.h"

static obscura64_status obscura64_provider_map_core_status(
    obscura64_core_status status)
{
    switch (status) {
    case OBSCURA64_CORE_STATUS_SUCCESS:
        return OBSCURA64_OK;
    case OBSCURA64_CORE_STATUS_INVALID_ARGUMENT:
        return OBSCURA64_INVALID_ARGUMENT;
    case OBSCURA64_CORE_STATUS_INVALID_PROFILE:
        return OBSCURA64_INVALID_PROFILE;
    case OBSCURA64_CORE_STATUS_INVALID_DATA:
        return OBSCURA64_INVALID_DATA;
    case OBSCURA64_CORE_STATUS_BUFFER_TOO_SMALL:
        return OBSCURA64_BUFFER_TOO_SMALL;
    case OBSCURA64_CORE_STATUS_SIZE_OVERFLOW:
        return OBSCURA64_SIZE_OVERFLOW;
    default:
        return OBSCURA64_INVALID_ARGUMENT;
    }
}

static obscura64_status obscura64_builtin_encoded_size(
    void *user_data,
    const char profile[OBSCURA64_PROFILE_SIZE],
    size_t input_len,
    size_t *encoded_len)
{
    (void)user_data;
    (void)profile;
    return obscura64_provider_map_core_status(
        obscura64_codec_encoded_size(input_len, encoded_len));
}

static obscura64_status obscura64_builtin_decoded_size(
    void *user_data,
    const char profile[OBSCURA64_PROFILE_SIZE],
    const char *encoded,
    size_t encoded_len,
    size_t *decoded_len)
{
    (void)user_data;
    return obscura64_provider_map_core_status(obscura64_codec_decoded_size(
        encoded, encoded_len, profile, OBSCURA64_PROFILE_SIZE, decoded_len));
}

static obscura64_status obscura64_builtin_encode(
    void *user_data,
    const char profile[OBSCURA64_PROFILE_SIZE],
    const void *input,
    size_t input_len,
    char *output,
    size_t output_capacity,
    size_t *output_len)
{
    (void)user_data;
    return obscura64_provider_map_core_status(obscura64_codec_encode(
        (const uint8_t *)input, input_len, profile, OBSCURA64_PROFILE_SIZE,
        output, output_capacity, output_len));
}

static obscura64_status obscura64_builtin_decode(
    void *user_data,
    const char profile[OBSCURA64_PROFILE_SIZE],
    const char *encoded,
    size_t encoded_len,
    void *output,
    size_t output_capacity,
    size_t *output_len)
{
    (void)user_data;
    return obscura64_provider_map_core_status(obscura64_codec_decode(
        encoded, encoded_len, profile, OBSCURA64_PROFILE_SIZE,
        (uint8_t *)output, output_capacity, output_len));
}

const obscura64_provider obscura64_builtin_provider_instance = {
    sizeof(obscura64_provider),
    OBSCURA64_PROVIDER_ABI_VERSION,
    NULL,
    obscura64_builtin_encoded_size,
    obscura64_builtin_decoded_size,
    obscura64_builtin_encode,
    obscura64_builtin_decode
};

const obscura64_provider *obscura64_builtin_provider(void)
{
    return &obscura64_builtin_provider_instance;
}
