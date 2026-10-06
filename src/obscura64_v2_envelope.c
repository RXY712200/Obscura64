#include "obscura64_v2_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* V2 wire format is independent of V1 project state and Provider ABI. */
#define V2_VERSION 2U

static const unsigned char v2_magic[8] = { 'O', 'B', '6', '4', 'E', 'N', 'V', '2' };

static uint16_t read_u16(const unsigned char *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint64_t read_u64(const unsigned char *p)
{
    uint64_t value = 0;
    unsigned int i;
    for (i = 0; i < 8; ++i) value |= (uint64_t)p[i] << (8U * i);
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
    for (i = 0; i < 8; ++i) p[i] = (unsigned char)(value >> (8U * i));
}

/* The envelope builder accepts a completed backend body. Its length is known
 * from the backend result, never predicted from plaintext length. */
obscura64_status obscura64_v2_envelope_build(uint16_t kind, const void *body,
    size_t body_len, void **output, size_t *output_len)
{
    unsigned char *p;
    size_t total;
    if (body_len > SIZE_MAX - OBSCURA64_V2_HEADER_SIZE) return OBSCURA64_SIZE_OVERFLOW;
#if SIZE_MAX > UINT64_MAX
    if (body_len > UINT64_MAX) return OBSCURA64_SIZE_OVERFLOW;
#endif
    total = OBSCURA64_V2_HEADER_SIZE + body_len;
    p = (unsigned char *)malloc(total);
    if (p == NULL) return OBSCURA64_OUT_OF_MEMORY;
    memcpy(p, v2_magic, sizeof(v2_magic));
    write_u16(p + 8, V2_VERSION);
    write_u16(p + 10, OBSCURA64_V2_HEADER_SIZE);
    write_u16(p + 12, kind);
    write_u16(p + 14, 0);
    write_u64(p + 16, (uint64_t)body_len);
    if (body_len != 0) memcpy(p + OBSCURA64_V2_HEADER_SIZE, body, body_len);
    *output = p;
    *output_len = total;
    return OBSCURA64_OK;
}

obscura64_status obscura64_v2_envelope_parse(const void *data, size_t length,
    uint16_t *kind, const unsigned char **body, size_t *body_len)
{
    const unsigned char *p = (const unsigned char *)data;
    uint64_t encoded_length;
    if (data == NULL && length != 0) return OBSCURA64_INVALID_ARGUMENT;
    if (length < OBSCURA64_V2_HEADER_SIZE) return OBSCURA64_ENVELOPE_CORRUPT;
    if (memcmp(p, v2_magic, sizeof(v2_magic)) != 0)
        return OBSCURA64_ENVELOPE_CORRUPT;
    if (read_u16(p + 8) != V2_VERSION) return OBSCURA64_UNSUPPORTED_VERSION;
    if (read_u16(p + 10) != OBSCURA64_V2_HEADER_SIZE || read_u16(p + 14) != 0)
        return OBSCURA64_ENVELOPE_CORRUPT;
    encoded_length = read_u64(p + 16);
    if (encoded_length > (uint64_t)(SIZE_MAX - OBSCURA64_V2_HEADER_SIZE))
        return OBSCURA64_SIZE_OVERFLOW;
    if ((size_t)encoded_length != length - OBSCURA64_V2_HEADER_SIZE)
        return OBSCURA64_ENVELOPE_CORRUPT;
    *kind = read_u16(p + 12);
    *body = p + OBSCURA64_V2_HEADER_SIZE;
    *body_len = (size_t)encoded_length;
    return OBSCURA64_OK;
}

int obscura64_v2_has_magic(const void *data, size_t length)
{
    return data != NULL && length >= sizeof(v2_magic) &&
        memcmp(data, v2_magic, sizeof(v2_magic)) == 0;
}
