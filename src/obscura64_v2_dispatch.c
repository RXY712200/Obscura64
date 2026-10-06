#include "obscura64_v2_internal.h"

#include <stdlib.h>
#include <string.h>

obscura64_status obscura64_protect_alloc(obscura64_protection protection,
    const void *input, size_t input_len, void **output, size_t *output_len)
{
    void *body = NULL;
    size_t body_len = 0;
    obscura64_status status;
    if (output != NULL) *output = NULL;
    if (output_len != NULL) *output_len = 0;
    if (output == NULL || output_len == NULL || (input == NULL && input_len != 0))
        return OBSCURA64_INVALID_ARGUMENT;
    switch (protection) {
    case OBSCURA64_PROTECTION_NONE:
        return obscura64_v2_envelope_build((uint16_t)protection,
            input, input_len, output, output_len);
    case OBSCURA64_PROTECTION_CASUAL:
        status = obscura64_v2_casual_protect(input, input_len, &body, &body_len);
        break;
    case OBSCURA64_PROTECTION_CURRENT_USER:
        status = obscura64_v2_current_user_protect(input, input_len, &body, &body_len);
        break;
    default:
        return OBSCURA64_UNSUPPORTED_PROTECTION;
    }
    if (status != OBSCURA64_OK) return status;
    status = obscura64_v2_envelope_build((uint16_t)protection,
        body, body_len, output, output_len);
    free(body);
    return status;
}

obscura64_status obscura64_unprotect_alloc_ex(const void *protected_data,
    size_t protected_len, void **output, size_t *output_len,
    obscura64_data_format *format)
{
    uint16_t kind;
    const unsigned char *body;
    size_t body_len;
    void *result = NULL;
    size_t result_len = 0;
    obscura64_status status;
    if (output != NULL) *output = NULL;
    if (output_len != NULL) *output_len = 0;
    if (format != NULL) *format = OBSCURA64_FORMAT_UNKNOWN;
    if (output == NULL || output_len == NULL ||
        (protected_data == NULL && protected_len != 0))
        return OBSCURA64_INVALID_ARGUMENT;
    if (!obscura64_v2_has_magic(protected_data, protected_len)) {
        status = obscura64_v2_legacy_read(protected_data, protected_len,
            &result, &result_len);
        if (status == OBSCURA64_OK) {
            *output = result;
            *output_len = result_len;
            if (format != NULL)
                *format = OBSCURA64_FORMAT_V1_LEGACY_UPGRADE_RECOMMENDED;
        }
        return status;
    }
    status = obscura64_v2_envelope_parse(protected_data, protected_len,
        &kind, &body, &body_len);
    if (status != OBSCURA64_OK) return status;
    switch (kind) {
    case OBSCURA64_PROTECTION_NONE:
        if (body_len != 0) {
            result = malloc(body_len);
            if (result == NULL) return OBSCURA64_OUT_OF_MEMORY;
            memcpy(result, body, body_len);
        }
        result_len = body_len;
        break;
    case OBSCURA64_PROTECTION_CASUAL:
        status = obscura64_v2_casual_unprotect(body, body_len,
            &result, &result_len);
        break;
    case OBSCURA64_PROTECTION_CURRENT_USER:
        status = obscura64_v2_current_user_unprotect(body, body_len,
            &result, &result_len);
        break;
    default:
        return OBSCURA64_UNSUPPORTED_PROTECTION;
    }
    if (status != OBSCURA64_OK) return status;
    *output = result;
    *output_len = result_len;
    if (format != NULL) *format = OBSCURA64_FORMAT_V2;
    return OBSCURA64_OK;
}

obscura64_status obscura64_unprotect_alloc(const void *protected_data,
    size_t protected_len, void **output, size_t *output_len)
{
    return obscura64_unprotect_alloc_ex(protected_data, protected_len,
        output, output_len, NULL);
}

/* Caller buffers are only modified after complete backend success. */
obscura64_status obscura64_protect(obscura64_protection protection,
    const void *input, size_t input_len, void *output,
    size_t output_capacity, size_t *output_len)
{
    void *allocated = NULL;
    size_t required = 0;
    obscura64_status status;
    if (output_len == NULL) return OBSCURA64_INVALID_ARGUMENT;
    *output_len = 0;
    if (output == NULL && output_capacity != 0) return OBSCURA64_INVALID_ARGUMENT;
    status = obscura64_protect_alloc(protection, input, input_len,
        &allocated, &required);
    if (status != OBSCURA64_OK) return status;
    if (output_capacity < required) {
        *output_len = required;
        free(allocated);
        return OBSCURA64_BUFFER_TOO_SMALL;
    }
    memcpy(output, allocated, required);
    *output_len = required;
    free(allocated);
    return OBSCURA64_OK;
}

obscura64_status obscura64_unprotect(const void *protected_data,
    size_t protected_len, void *output,
    size_t output_capacity, size_t *output_len)
{
    void *allocated = NULL;
    size_t required = 0;
    obscura64_status status;
    if (output_len == NULL) return OBSCURA64_INVALID_ARGUMENT;
    *output_len = 0;
    if (output == NULL && output_capacity != 0) return OBSCURA64_INVALID_ARGUMENT;
    status = obscura64_unprotect_alloc(protected_data, protected_len,
        &allocated, &required);
    if (status != OBSCURA64_OK) return status;
    if (output_capacity < required) {
        *output_len = required;
        free(allocated);
        return OBSCURA64_BUFFER_TOO_SMALL;
    }
    if (required != 0) memcpy(output, allocated, required);
    *output_len = required;
    free(allocated);
    return OBSCURA64_OK;
}

obscura64_status obscura64_migrate_v1_managed_alloc(
    obscura64_protection protection, const char *legacy_data, size_t legacy_len,
    void **output, size_t *output_len)
{
    void *plain = NULL;
    size_t plain_len = 0;
    obscura64_status status;
    if (output != NULL) *output = NULL;
    if (output_len != NULL) *output_len = 0;
    if (output == NULL || output_len == NULL ||
        (legacy_data == NULL && legacy_len != 0)) return OBSCURA64_INVALID_ARGUMENT;
    status = obscura64_v2_legacy_read(legacy_data, legacy_len, &plain, &plain_len);
    if (status != OBSCURA64_OK) return status;
    status = obscura64_protect_alloc(protection, plain, plain_len,
        output, output_len);
    free(plain);
    return status;
}
