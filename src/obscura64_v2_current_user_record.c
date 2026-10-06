#include "obscura64_v2_internal.h"
#include "obscura64_sha256.h"

#include <stdlib.h>
#include <string.h>

#define CU_HEADER_SIZE 56U
#define CU_DIGEST_OFFSET 24U

static const unsigned char cu_magic[8] = {'O','B','6','4','C','U','0','1'};

static void write_u64(unsigned char *p, uint64_t value)
{
    size_t i;
    for (i = 0; i < 8; ++i) p[i] = (unsigned char)(value >> (8U * i));
}

static uint64_t read_u64(const unsigned char *p)
{
    uint64_t value = 0;
    size_t i;
    for (i = 0; i < 8; ++i) value |= (uint64_t)p[i] << (8U * i);
    return value;
}

obscura64_status obscura64_v2_current_user_record_build(const void *input,
    size_t input_len, unsigned char **record, size_t *record_len)
{
    unsigned char *p;
    obscura64_status status;
    if (record != NULL) *record = NULL;
    if (record_len != NULL) *record_len = 0;
    if (record == NULL || record_len == NULL ||
        (input == NULL && input_len != 0)) return OBSCURA64_INVALID_ARGUMENT;
    /* DATA_BLOB.cbData must hold the complete plaintext record. */
    if (input_len > UINT32_MAX - CU_HEADER_SIZE ||
        input_len > SIZE_MAX - CU_HEADER_SIZE) return OBSCURA64_SIZE_OVERFLOW;
    p = (unsigned char *)malloc(CU_HEADER_SIZE + input_len);
    if (p == NULL) return OBSCURA64_OUT_OF_MEMORY;
    memset(p, 0, CU_HEADER_SIZE);
    memcpy(p, cu_magic, sizeof(cu_magic));
    p[8] = 1; /* inner version, little-endian u16 */
    p[10] = CU_HEADER_SIZE; /* header size, little-endian u16 */
    write_u64(p + 16, (uint64_t)input_len);
    if (input_len != 0) memcpy(p + CU_HEADER_SIZE, input, input_len);
    status = obscura64_sha256_segments(p, CU_DIGEST_OFFSET,
        p + CU_HEADER_SIZE, input_len, p + CU_DIGEST_OFFSET);
    if (status != OBSCURA64_OK) { free(p); return status; }
    *record = p;
    *record_len = CU_HEADER_SIZE + input_len;
    return OBSCURA64_OK;
}

obscura64_status obscura64_v2_current_user_record_parse(const unsigned char *record,
    size_t record_len, const unsigned char **payload, size_t *payload_len)
{
    uint64_t stored;
    unsigned char digest[32];
    obscura64_status status;
    if (payload != NULL) *payload = NULL;
    if (payload_len != NULL) *payload_len = 0;
    if (payload == NULL || payload_len == NULL)
        return OBSCURA64_INVALID_ARGUMENT;
    if (record == NULL || record_len < CU_HEADER_SIZE ||
        memcmp(record, cu_magic, sizeof(cu_magic)) != 0 ||
        record[8] != 1 || record[9] != 0 ||
        record[10] != CU_HEADER_SIZE || record[11] != 0 ||
        record[12] != 0 || record[13] != 0 ||
        record[14] != 0 || record[15] != 0)
        return OBSCURA64_PROTECTED_CORRUPT;
    stored = read_u64(record + 16);
    if (stored > SIZE_MAX - CU_HEADER_SIZE ||
        stored != record_len - CU_HEADER_SIZE)
        return OBSCURA64_PROTECTED_CORRUPT;
    status = obscura64_sha256_segments(record, CU_DIGEST_OFFSET,
        record + CU_HEADER_SIZE, (size_t)stored, digest);
    if (status != OBSCURA64_OK) return status;
    if (memcmp(digest, record + CU_DIGEST_OFFSET, sizeof(digest)) != 0)
        return OBSCURA64_PROTECTED_CORRUPT;
    *payload = record + CU_HEADER_SIZE;
    *payload_len = (size_t)stored;
    return OBSCURA64_OK;
}
