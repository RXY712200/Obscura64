#include "obscura64.h"
#include "obscura64_internal.h"

#include <stdlib.h>

obscura64_status obscura64_encoded_size(
    const obscura64_context *context,
    size_t input_len,
    size_t *encoded_len)
{
    size_t result = 0;
    obscura64_status status;

    if (encoded_len == NULL) {
        return OBSCURA64_INVALID_ARGUMENT;
    }
    *encoded_len = 0;
    if (context == NULL) {
        return OBSCURA64_INVALID_ARGUMENT;
    }
    status = context->provider.encoded_size(context->provider.user_data,
        context->profile, input_len, &result);
    if (status == OBSCURA64_OK) {
        *encoded_len = result;
    }
    return status;
}

obscura64_status obscura64_decoded_size(
    const obscura64_context *context,
    const char *encoded,
    size_t encoded_len,
    size_t *decoded_len)
{
    size_t result = 0;
    obscura64_status status;

    if (decoded_len == NULL) {
        return OBSCURA64_INVALID_ARGUMENT;
    }
    *decoded_len = 0;
    if (context == NULL) {
        return OBSCURA64_INVALID_ARGUMENT;
    }
    status = context->provider.decoded_size(context->provider.user_data,
        context->profile, encoded, encoded_len, &result);
    if (status == OBSCURA64_OK) {
        *decoded_len = result;
    }
    return status;
}

obscura64_status obscura64_encode(
    const obscura64_context *context,
    const void *input,
    size_t input_len,
    char *output,
    size_t output_capacity,
    size_t *output_len)
{
    if (output_len == NULL) {
        return OBSCURA64_INVALID_ARGUMENT;
    }
    *output_len = 0;
    if (context == NULL) {
        return OBSCURA64_INVALID_ARGUMENT;
    }
    {
        size_t result = 0;
        const obscura64_status status = context->provider.encode(
            context->provider.user_data, context->profile, input, input_len,
            output, output_capacity, &result);
        if (status == OBSCURA64_OK || status == OBSCURA64_BUFFER_TOO_SMALL) {
            *output_len = result;
        }
        return status;
    }
}

obscura64_status obscura64_decode(
    const obscura64_context *context,
    const char *encoded,
    size_t encoded_len,
    void *output,
    size_t output_capacity,
    size_t *output_len)
{
    if (output_len == NULL) {
        return OBSCURA64_INVALID_ARGUMENT;
    }
    *output_len = 0;
    if (context == NULL) {
        return OBSCURA64_INVALID_ARGUMENT;
    }
    {
        size_t result = 0;
        const obscura64_status status = context->provider.decode(
            context->provider.user_data, context->profile, encoded, encoded_len,
            output, output_capacity, &result);
        if (status == OBSCURA64_OK || status == OBSCURA64_BUFFER_TOO_SMALL) {
            *output_len = result;
        }
        return status;
    }
}

obscura64_status obscura64_encode_alloc(
    const obscura64_context *context,
    const void *input,
    size_t input_len,
    char **output,
    size_t *output_len)
{
    char *buffer;
    size_t required_len = 0;
    size_t written_len = 0;
    obscura64_status status;

    if (output != NULL) {
        *output = NULL;
    }
    if (output_len != NULL) {
        *output_len = 0;
    }
    if (output == NULL || output_len == NULL || context == NULL ||
        (input_len != 0 && input == NULL)) {
        return OBSCURA64_INVALID_ARGUMENT;
    }
    status = obscura64_encoded_size(context, input_len, &required_len);
    if (status != OBSCURA64_OK) {
        return status;
    }
    if (required_len == 0) {
        return OBSCURA64_OK;
    }

    buffer = (char *)malloc(required_len);
    if (buffer == NULL) {
        return OBSCURA64_OUT_OF_MEMORY;
    }
    status = obscura64_encode(context, input, input_len, buffer, required_len, &written_len);
    if (status != OBSCURA64_OK) {
        free(buffer);
        return status;
    }

    *output = buffer;
    *output_len = written_len;
    return OBSCURA64_OK;
}

obscura64_status obscura64_decode_alloc(
    const obscura64_context *context,
    const char *encoded,
    size_t encoded_len,
    void **output,
    size_t *output_len)
{
    void *buffer;
    size_t required_len = 0;
    size_t written_len = 0;
    obscura64_status status;

    if (output != NULL) {
        *output = NULL;
    }
    if (output_len != NULL) {
        *output_len = 0;
    }
    if (output == NULL || output_len == NULL || context == NULL) {
        return OBSCURA64_INVALID_ARGUMENT;
    }
    status = obscura64_decoded_size(context, encoded, encoded_len, &required_len);
    if (status != OBSCURA64_OK) {
        return status;
    }
    if (required_len == 0) {
        return OBSCURA64_OK;
    }

    buffer = malloc(required_len);
    if (buffer == NULL) {
        return OBSCURA64_OUT_OF_MEMORY;
    }
    status = obscura64_decode(context, encoded, encoded_len, buffer, required_len, &written_len);
    if (status != OBSCURA64_OK) {
        free(buffer);
        return status;
    }

    *output = buffer;
    *output_len = written_len;
    return OBSCURA64_OK;
}

void obscura64_free(void *ptr)
{
    free(ptr);
}

const char *obscura64_status_string(obscura64_status status)
{
    switch (status) {
    case OBSCURA64_OK:
        return "success";
    case OBSCURA64_INVALID_ARGUMENT:
        return "invalid argument";
    case OBSCURA64_INVALID_PROFILE:
        return "invalid profile";
    case OBSCURA64_INVALID_PROVIDER:
        return "invalid provider";
    case OBSCURA64_INVALID_DATA:
        return "invalid encoded data";
    case OBSCURA64_BUFFER_TOO_SMALL:
        return "output buffer too small";
    case OBSCURA64_SIZE_OVERFLOW:
        return "size overflow";
    case OBSCURA64_OUT_OF_MEMORY:
        return "out of memory";
    case OBSCURA64_IO_ERROR:
        return "I/O error";
    case OBSCURA64_STATE_MISSING:
        return "project state missing";
    case OBSCURA64_STATE_CORRUPT:
        return "project state corrupt";
    case OBSCURA64_RNG_FAILURE:
        return "random number generation failure";
    case OBSCURA64_BUSY:
        return "project operation busy";
    case OBSCURA64_UNRECOVERABLE:
        return "managed project state cannot be recovered";
    default:
        return "unknown status";
    }
}
