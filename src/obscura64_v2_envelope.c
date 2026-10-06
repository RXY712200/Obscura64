#include "obscura64.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* V2 wire format is deliberately independent of V1 project state. */
#define V2_HEADER_SIZE 24U
#define V2_VERSION 2U
#define V2_PREVIEW_PLAIN 0U

static const unsigned char v2_magic[8] = { 'O', 'B', '6', '4', 'E', 'N', 'V', '2' };

static uint16_t read_u16(const unsigned char *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint64_t read_u64(const unsigned char *p)
{
    uint64_t value = 0;
    unsigned int i;
    for (i = 0; i < 8; ++i) {
        value |= (uint64_t)p[i] << (8U * i);
    }
    return value;
}

static void write_u16(unsigned char *p, uint16_t value)
{
    p[0] = (unsigned char)value;
    p[1] = (unsigned char)(value >> 8);
}

static void write_u64(unsigned char *p, uint64_t value)
{
    unsigned int i;
    for (i = 0; i < 8; ++i) {
        p[i] = (unsigned char)(value >> (8U * i));
    }
}

obscura64_status obscura64_protected_size(size_t input_len, size_t *output_len)
{
    if (output_len == NULL) {
        return OBSCURA64_INVALID_ARGUMENT;
    }
    *output_len = 0;
    if (input_len > SIZE_MAX - V2_HEADER_SIZE) {
        return OBSCURA64_SIZE_OVERFLOW;
    }
#if SIZE_MAX > UINT64_MAX
    if (input_len > UINT64_MAX) {
        return OBSCURA64_SIZE_OVERFLOW;
    }
#endif
    *output_len = V2_HEADER_SIZE + input_len;
    return OBSCURA64_OK;
}

static obscura64_status parse_v2(const void *data, size_t length,
    const unsigned char **body, size_t *body_length)
{
    const unsigned char *p = (const unsigned char *)data;
    uint64_t encoded_length;
    if (data == NULL && length != 0) {
        return OBSCURA64_INVALID_ARGUMENT;
    }
    if (length < V2_HEADER_SIZE) {
        return OBSCURA64_ENVELOPE_CORRUPT;
    }
    if (memcmp(p, v2_magic, sizeof(v2_magic)) != 0) {
        return OBSCURA64_ENVELOPE_CORRUPT;
    }
    if (read_u16(p + 8) != V2_VERSION) {
        return OBSCURA64_UNSUPPORTED_VERSION;
    }
    if (read_u16(p + 10) != V2_HEADER_SIZE || read_u16(p + 14) != 0) {
        return OBSCURA64_ENVELOPE_CORRUPT;
    }
    if (read_u16(p + 12) != V2_PREVIEW_PLAIN) {
        return OBSCURA64_UNSUPPORTED_PROTECTION;
    }
    encoded_length = read_u64(p + 16);
    if (encoded_length > (uint64_t)(SIZE_MAX - V2_HEADER_SIZE)) {
        return OBSCURA64_SIZE_OVERFLOW;
    }
    if ((size_t)encoded_length != length - V2_HEADER_SIZE) {
        return OBSCURA64_ENVELOPE_CORRUPT;
    }
    *body = p + V2_HEADER_SIZE;
    *body_length = (size_t)encoded_length;
    return OBSCURA64_OK;
}

obscura64_status obscura64_unprotected_size(
    const void *protected_data, size_t protected_len, size_t *output_len)
{
    const unsigned char *body;
    size_t body_length;
    obscura64_status status;
    if (output_len == NULL) {
        return OBSCURA64_INVALID_ARGUMENT;
    }
    *output_len = 0;
    status = parse_v2(protected_data, protected_len, &body, &body_length);
    if (status == OBSCURA64_OK) {
        *output_len = body_length;
    }
    return status;
}

obscura64_status obscura64_protect(const void *input, size_t input_len,
    void *output, size_t output_capacity, size_t *output_len)
{
    unsigned char *p = (unsigned char *)output;
    size_t required;
    obscura64_status status;
    if (output_len == NULL) {
        return OBSCURA64_INVALID_ARGUMENT;
    }
    *output_len = 0;
    if ((input == NULL && input_len != 0) ||
        (output == NULL && output_capacity != 0)) {
        return OBSCURA64_INVALID_ARGUMENT;
    }
    status = obscura64_protected_size(input_len, &required);
    if (status != OBSCURA64_OK) {
        return status;
    }
    if (output_capacity < required) {
        *output_len = required;
        return OBSCURA64_BUFFER_TOO_SMALL;
    }
    if (input_len != 0) {
        memcpy(p + V2_HEADER_SIZE, input, input_len);
    }
    memcpy(p, v2_magic, sizeof(v2_magic));
    write_u16(p + 8, V2_VERSION);
    write_u16(p + 10, V2_HEADER_SIZE);
    write_u16(p + 12, V2_PREVIEW_PLAIN);
    write_u16(p + 14, 0);
    write_u64(p + 16, (uint64_t)input_len);
    *output_len = required;
    return OBSCURA64_OK;
}

obscura64_status obscura64_unprotect(const void *protected_data, size_t protected_len,
    void *output, size_t output_capacity, size_t *output_len)
{
    const unsigned char *body;
    size_t body_length;
    obscura64_status status;
    if (output_len == NULL) {
        return OBSCURA64_INVALID_ARGUMENT;
    }
    *output_len = 0;
    if (output == NULL && output_capacity != 0) {
        return OBSCURA64_INVALID_ARGUMENT;
    }
    status = parse_v2(protected_data, protected_len, &body, &body_length);
    if (status != OBSCURA64_OK) {
        return status;
    }
    if (output_capacity < body_length) {
        *output_len = body_length;
        return OBSCURA64_BUFFER_TOO_SMALL;
    }
    if (body_length != 0) {
        memcpy(output, body, body_length);
    }
    *output_len = body_length;
    return OBSCURA64_OK;
}

obscura64_status obscura64_protect_alloc(const void *input, size_t input_len,
    void **output, size_t *output_len)
{
    size_t required, written;
    void *buffer;
    obscura64_status status;
    if (output != NULL) *output = NULL;
    if (output_len != NULL) *output_len = 0;
    if (output == NULL || output_len == NULL || (input == NULL && input_len != 0)) {
        return OBSCURA64_INVALID_ARGUMENT;
    }
    status = obscura64_protected_size(input_len, &required);
    if (status != OBSCURA64_OK) return status;
    buffer = malloc(required);
    if (buffer == NULL) return OBSCURA64_OUT_OF_MEMORY;
    status = obscura64_protect(input, input_len, buffer, required, &written);
    if (status != OBSCURA64_OK) { free(buffer); return status; }
    *output = buffer;
    *output_len = written;
    return OBSCURA64_OK;
}

obscura64_status obscura64_unprotect_alloc(const void *protected_data, size_t protected_len,
    void **output, size_t *output_len)
{
    const unsigned char *body;
    size_t required, written;
    void *buffer;
    obscura64_status status;
    if (output != NULL) *output = NULL;
    if (output_len != NULL) *output_len = 0;
    if (output == NULL || output_len == NULL) return OBSCURA64_INVALID_ARGUMENT;
    status = parse_v2(protected_data, protected_len, &body, &required);
    if (status != OBSCURA64_OK) return status;
    if (required == 0) return OBSCURA64_OK;
    buffer = malloc(required);
    if (buffer == NULL) return OBSCURA64_OUT_OF_MEMORY;
    status = obscura64_unprotect(protected_data, protected_len, buffer, required, &written);
    if (status != OBSCURA64_OK) { free(buffer); return status; }
    *output = buffer;
    *output_len = written;
    return OBSCURA64_OK;
}
