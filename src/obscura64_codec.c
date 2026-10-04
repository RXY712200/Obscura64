#include "obscura64_internal.h"

#include <string.h>

static obscura64_core_status obscura64_make_reverse_map(
    const char *profile,
    size_t profile_len,
    uint8_t reverse_map[256])
{
    if (profile == NULL) {
        return OBSCURA64_CORE_STATUS_INVALID_ARGUMENT;
    }
    if (!obscura64_profile_is_valid(profile, profile_len)) {
        return OBSCURA64_CORE_STATUS_INVALID_PROFILE;
    }
    if (!obscura64_profile_build_reverse_map(profile, profile_len, reverse_map)) {
        return OBSCURA64_CORE_STATUS_INVALID_PROFILE;
    }
    return OBSCURA64_CORE_STATUS_SUCCESS;
}

obscura64_core_status obscura64_codec_encoded_size(size_t input_len, size_t *output_len)
{
    size_t groups;

    if (output_len == NULL) {
        return OBSCURA64_CORE_STATUS_INVALID_ARGUMENT;
    }
    *output_len = 0;
    groups = input_len / 3;
    if (input_len % 3 != 0) {
        ++groups;
    }
    if (groups > SIZE_MAX / 4) {
        return OBSCURA64_CORE_STATUS_SIZE_OVERFLOW;
    }
    *output_len = groups * 4;
    return OBSCURA64_CORE_STATUS_SUCCESS;
}

obscura64_core_status obscura64_codec_decoded_size(
    const char *input,
    size_t input_len,
    const char *profile,
    size_t profile_len,
    size_t *output_len)
{
    uint8_t reverse_map[256];
    obscura64_core_status status;
    size_t i;
    size_t decoded_len;

    if (output_len == NULL) {
        return OBSCURA64_CORE_STATUS_INVALID_ARGUMENT;
    }
    *output_len = 0;
    if (input_len != 0 && input == NULL) {
        return OBSCURA64_CORE_STATUS_INVALID_ARGUMENT;
    }
    status = obscura64_make_reverse_map(profile, profile_len, reverse_map);
    if (status != OBSCURA64_CORE_STATUS_SUCCESS) {
        return status;
    }
    if (input_len == 0) {
        return OBSCURA64_CORE_STATUS_SUCCESS;
    }
    if (input_len % 4 != 0) {
        return OBSCURA64_CORE_STATUS_INVALID_DATA;
    }

    decoded_len = (input_len / 4) * 3;
    for (i = 0; i < input_len; i += 4) {
        const int final_quantum = (i + 4 == input_len);
        const unsigned char c0 = (unsigned char)input[i];
        const unsigned char c1 = (unsigned char)input[i + 1];
        const unsigned char c2 = (unsigned char)input[i + 2];
        const unsigned char c3 = (unsigned char)input[i + 3];
        const uint8_t s0 = reverse_map[c0];
        const uint8_t s1 = reverse_map[c1];

        if (s0 == OBSCURA64_REVERSE_INVALID || s1 == OBSCURA64_REVERSE_INVALID) {
            return OBSCURA64_CORE_STATUS_INVALID_DATA;
        }
        if (c2 == '=') {
            if (!final_quantum || c3 != '=' || (s1 & UINT8_C(0x0F)) != 0) {
                return OBSCURA64_CORE_STATUS_INVALID_DATA;
            }
            decoded_len -= 2;
        } else {
            const uint8_t s2 = reverse_map[c2];
            if (s2 == OBSCURA64_REVERSE_INVALID) {
                return OBSCURA64_CORE_STATUS_INVALID_DATA;
            }
            if (c3 == '=') {
                if (!final_quantum || (s2 & UINT8_C(0x03)) != 0) {
                    return OBSCURA64_CORE_STATUS_INVALID_DATA;
                }
                --decoded_len;
            } else if (reverse_map[c3] == OBSCURA64_REVERSE_INVALID) {
                return OBSCURA64_CORE_STATUS_INVALID_DATA;
            }
        }
    }

    *output_len = decoded_len;
    return OBSCURA64_CORE_STATUS_SUCCESS;
}

obscura64_core_status obscura64_codec_encode(
    const uint8_t *input,
    size_t input_len,
    const char *profile,
    size_t profile_len,
    char *output,
    size_t output_capacity,
    size_t *output_len)
{
    size_t required_len;
    size_t input_pos = 0;
    size_t output_pos = 0;
    obscura64_core_status status;

    if (output_len == NULL) {
        return OBSCURA64_CORE_STATUS_INVALID_ARGUMENT;
    }
    *output_len = 0;
    if (input_len != 0 && input == NULL) {
        return OBSCURA64_CORE_STATUS_INVALID_ARGUMENT;
    }
    if (profile == NULL) {
        return OBSCURA64_CORE_STATUS_INVALID_ARGUMENT;
    }
    if (!obscura64_profile_is_valid(profile, profile_len)) {
        return OBSCURA64_CORE_STATUS_INVALID_PROFILE;
    }
    status = obscura64_codec_encoded_size(input_len, &required_len);
    if (status != OBSCURA64_CORE_STATUS_SUCCESS) {
        return status;
    }
    if (output_capacity < required_len) {
        *output_len = required_len;
        return OBSCURA64_CORE_STATUS_BUFFER_TOO_SMALL;
    }
    if (required_len != 0 && output == NULL) {
        return OBSCURA64_CORE_STATUS_INVALID_ARGUMENT;
    }

    while (input_pos < input_len) {
        const size_t remaining = input_len - input_pos;
        const uint8_t b0 = input[input_pos++];
        const uint8_t b1 = remaining > 1 ? input[input_pos++] : 0;
        const uint8_t b2 = remaining > 2 ? input[input_pos++] : 0;
        const uint8_t i0 = (uint8_t)(b0 >> 2);
        const uint8_t i1 = (uint8_t)(((b0 & 0x03U) << 4) | (b1 >> 4));
        const uint8_t i2 = (uint8_t)(((b1 & 0x0FU) << 2) | (b2 >> 6));
        const uint8_t i3 = (uint8_t)(b2 & 0x3FU);

        output[output_pos++] = profile[i0];
        output[output_pos++] = profile[i1];
        output[output_pos++] = remaining > 1 ? profile[i2] : '=';
        output[output_pos++] = remaining > 2 ? profile[i3] : '=';
    }

    *output_len = output_pos;
    return OBSCURA64_CORE_STATUS_SUCCESS;
}

obscura64_core_status obscura64_codec_decode(
    const char *input,
    size_t input_len,
    const char *profile,
    size_t profile_len,
    uint8_t *output,
    size_t output_capacity,
    size_t *output_len)
{
    uint8_t reverse_map[256];
    size_t required_len;
    size_t input_pos;
    size_t output_pos = 0;
    obscura64_core_status status;

    if (output_len == NULL) {
        return OBSCURA64_CORE_STATUS_INVALID_ARGUMENT;
    }
    *output_len = 0;
    if (input_len != 0 && input == NULL) {
        return OBSCURA64_CORE_STATUS_INVALID_ARGUMENT;
    }
    status = obscura64_make_reverse_map(profile, profile_len, reverse_map);
    if (status != OBSCURA64_CORE_STATUS_SUCCESS) {
        return status;
    }
    status = obscura64_codec_decoded_size(input, input_len, profile, profile_len, &required_len);
    if (status != OBSCURA64_CORE_STATUS_SUCCESS) {
        return status;
    }
    if (output_capacity < required_len) {
        *output_len = required_len;
        return OBSCURA64_CORE_STATUS_BUFFER_TOO_SMALL;
    }
    if (required_len != 0 && output == NULL) {
        return OBSCURA64_CORE_STATUS_INVALID_ARGUMENT;
    }

    for (input_pos = 0; input_pos < input_len; input_pos += 4) {
        const uint8_t s0 = reverse_map[(unsigned char)input[input_pos]];
        const uint8_t s1 = reverse_map[(unsigned char)input[input_pos + 1]];
        const unsigned char c2 = (unsigned char)input[input_pos + 2];
        const unsigned char c3 = (unsigned char)input[input_pos + 3];
        const uint8_t s2 = c2 == '=' ? 0 : reverse_map[c2];
        const uint8_t s3 = c3 == '=' ? 0 : reverse_map[c3];

        output[output_pos++] = (uint8_t)((s0 << 2) | (s1 >> 4));
        if (c2 != '=') {
            output[output_pos++] = (uint8_t)((s1 << 4) | (s2 >> 2));
            if (c3 != '=') {
                output[output_pos++] = (uint8_t)((s2 << 6) | s3);
            }
        }
    }

    *output_len = output_pos;
    return OBSCURA64_CORE_STATUS_SUCCESS;
}
