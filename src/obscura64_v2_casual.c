#include "obscura64_v2_internal.h"
#include "obscura64_internal.h"
#include "obscura64_profiles_v1.h"
#include "obscura64_sha256.h"

#include <stdlib.h>
#include <string.h>

#define CASUAL_HEADER_SIZE 8U
#define CASUAL_DIGEST_SIZE 32U
#define CASUAL_PREFIX_SIZE (CASUAL_HEADER_SIZE + CASUAL_DIGEST_SIZE)

static obscura64_status casual_digest(const unsigned char header[CASUAL_HEADER_SIZE],
    const void *input, size_t length, unsigned char digest[CASUAL_DIGEST_SIZE])
{
    return obscura64_sha256_segments(header, CASUAL_HEADER_SIZE,
        input, length, digest);
}

obscura64_status obscura64_v2_casual_protect_with_profile(uint16_t profile_id,
    const void *input, size_t length, void **body, size_t *body_len)
{
    unsigned char *result;
    size_t encoded_len, written;
    obscura64_status status;
    if (body != NULL) *body = NULL;
    if (body_len != NULL) *body_len = 0;
    if (body == NULL || body_len == NULL || (input == NULL && length != 0))
        return OBSCURA64_INVALID_ARGUMENT;
    if (profile_id >= OBSCURA64_PROFILE_COUNT) return OBSCURA64_INVALID_ARGUMENT;
    if (obscura64_codec_encoded_size(length, &encoded_len) !=
        OBSCURA64_CORE_STATUS_SUCCESS || encoded_len > SIZE_MAX - CASUAL_PREFIX_SIZE)
        return OBSCURA64_SIZE_OVERFLOW;
    result = (unsigned char *)malloc(CASUAL_PREFIX_SIZE + encoded_len);
    if (result == NULL) return OBSCURA64_OUT_OF_MEMORY;
    memset(result, 0, CASUAL_HEADER_SIZE);
    result[0] = 1;
    result[2] = (unsigned char)profile_id;
    result[3] = (unsigned char)(profile_id >> 8);
    status = casual_digest(result, input, length, result + CASUAL_HEADER_SIZE);
    if (status == OBSCURA64_OK && obscura64_codec_encode(input, length,
        (const char *)obscura64_profiles_v1[profile_id], OBSCURA64_PROFILE_SIZE,
        (char *)result + CASUAL_PREFIX_SIZE, encoded_len, &written) !=
        OBSCURA64_CORE_STATUS_SUCCESS) status = OBSCURA64_CASUAL_CORRUPT;
    if (status != OBSCURA64_OK) { free(result); return status; }
    *body = result;
    *body_len = CASUAL_PREFIX_SIZE + written;
    return OBSCURA64_OK;
}

obscura64_status obscura64_v2_casual_protect(const void *input, size_t length,
    void **body, size_t *body_len)
{
    uint16_t profile_id;
    uint8_t profile[OBSCURA64_PROFILE_SIZE];
    if (body != NULL) *body = NULL;
    if (body_len != NULL) *body_len = 0;
    if (body == NULL || body_len == NULL || (input == NULL && length != 0))
        return OBSCURA64_INVALID_ARGUMENT;
    if (obscura64_profile_runtime_select_random(&profile_id, profile) !=
        OBSCURA64_CORE_STATUS_SUCCESS) return OBSCURA64_RNG_FAILURE;
    return obscura64_v2_casual_protect_with_profile(profile_id, input, length,
        body, body_len);
}

obscura64_status obscura64_v2_casual_unprotect(const void *body, size_t body_len,
    void **output, size_t *output_len)
{
    const unsigned char *p = (const unsigned char *)body;
    uint16_t profile_id;
    size_t decoded_len, written;
    unsigned char digest[CASUAL_DIGEST_SIZE];
    unsigned char *decoded = NULL;
    obscura64_status status;
    if (output != NULL) *output = NULL;
    if (output_len != NULL) *output_len = 0;
    if (output == NULL || output_len == NULL) return OBSCURA64_INVALID_ARGUMENT;
    if (body_len < CASUAL_PREFIX_SIZE || p == NULL || p[0] != 1 || p[1] != 0 ||
        p[4] != 0 || p[5] != 0 || p[6] != 0 || p[7] != 0)
        return OBSCURA64_CASUAL_CORRUPT;
    profile_id = (uint16_t)((uint16_t)p[2] | ((uint16_t)p[3] << 8));
    if (profile_id >= OBSCURA64_PROFILE_COUNT) return OBSCURA64_CASUAL_CORRUPT;
    if (obscura64_codec_decoded_size((const char *)p + CASUAL_PREFIX_SIZE,
        body_len - CASUAL_PREFIX_SIZE,
        (const char *)obscura64_profiles_v1[profile_id], OBSCURA64_PROFILE_SIZE,
        &decoded_len) != OBSCURA64_CORE_STATUS_SUCCESS)
        return OBSCURA64_CASUAL_CORRUPT;
    if (decoded_len != 0) {
        decoded = (unsigned char *)malloc(decoded_len);
        if (decoded == NULL) return OBSCURA64_OUT_OF_MEMORY;
    }
    if (obscura64_codec_decode((const char *)p + CASUAL_PREFIX_SIZE,
        body_len - CASUAL_PREFIX_SIZE,
        (const char *)obscura64_profiles_v1[profile_id], OBSCURA64_PROFILE_SIZE,
        decoded, decoded_len, &written) != OBSCURA64_CORE_STATUS_SUCCESS ||
        written != decoded_len) { free(decoded); return OBSCURA64_CASUAL_CORRUPT; }
    status = casual_digest(p, decoded, decoded_len, digest);
    if (status != OBSCURA64_OK) { free(decoded); return status; }
    if (memcmp(digest, p + CASUAL_HEADER_SIZE, CASUAL_DIGEST_SIZE) != 0) {
        free(decoded); return OBSCURA64_CASUAL_CORRUPT;
    }
    *output = decoded;
    *output_len = decoded_len;
    return OBSCURA64_OK;
}
