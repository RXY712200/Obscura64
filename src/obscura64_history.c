#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include "obscura64_internal.h"
#include <string.h>

static void put_le(unsigned char *p, uint64_t value, size_t size)
{
    size_t i;
    for (i = 0; i < size; ++i) p[i] = (unsigned char)(value >> (8U * i));
}

static uint64_t get_le(const unsigned char *p, size_t size)
{
    size_t i;
    uint64_t value = 0;
    for (i = 0; i < size; ++i) value |= (uint64_t)p[i] << (8U * i);
    return value;
}

static int all_zero(const void *data, size_t size)
{
    const unsigned char *p = (const unsigned char *)data;
    size_t i;
    for (i = 0; i < size; ++i) if (p[i] != 0) return 0;
    return 1;
}

static int history_hash(const unsigned char *data, unsigned char digest[32])
{
    BCRYPT_ALG_HANDLE algorithm = NULL;
    BCRYPT_HASH_HANDLE hash = NULL;
    NTSTATUS status = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, NULL, 0);
    if (!BCRYPT_SUCCESS(status)) return 0;
    status = BCryptCreateHash(algorithm, &hash, NULL, 0, NULL, 0, 0);
    if (BCRYPT_SUCCESS(status))
        status = BCryptHashData(hash, (PUCHAR)data, OBSCURA64_HISTORY_HASH_OFFSET, 0);
    if (BCRYPT_SUCCESS(status)) status = BCryptFinishHash(hash, digest, 32, 0);
    if (hash != NULL) (void)BCryptDestroyHash(hash);
    (void)BCryptCloseAlgorithmProvider(algorithm, 0);
    return BCRYPT_SUCCESS(status);
}

int obscura64_history_validate(const obscura64_project_history *history)
{
    size_t i;
    uint16_t profile_id;
    if (history == NULL || history->count > OBSCURA64_HISTORY_CAPACITY ||
        all_zero(history->project_id, OBSCURA64_PROJECT_ID_SIZE)) return 0;
    for (i = 0; i < OBSCURA64_HISTORY_CAPACITY; ++i) {
        const obscura64_history_entry *entry = &history->entries[i];
        if (i < history->count) {
            if (entry->generation == 0 ||
                !obscura64_profile_is_valid(entry->profile, OBSCURA64_PROFILE_SIZE) ||
                !obscura64_profile_library_find((const unsigned char *)entry->profile, &profile_id))
                return 0;
            if (i != 0 && history->entries[i - 1].generation <= entry->generation) return 0;
        } else if (entry->generation != 0 || !all_zero(entry->profile, OBSCURA64_PROFILE_SIZE)) {
            return 0;
        }
    }
    return 1;
}

obscura64_core_status obscura64_history_serialize(
    const obscura64_project_history *history,
    unsigned char output[OBSCURA64_HISTORY_V1_SIZE])
{
    unsigned char temporary[OBSCURA64_HISTORY_V1_SIZE] = {0};
    size_t i;
    if (history == NULL || output == NULL) return OBSCURA64_CORE_STATUS_INVALID_ARGUMENT;
    if (!obscura64_history_validate(history)) return OBSCURA64_CORE_STATUS_INVALID_DATA;
    memcpy(temporary, "OB64HI01", 8);
    put_le(temporary + 8, 1, 2);
    put_le(temporary + 10, 64, 2);
    put_le(temporary + 12, OBSCURA64_HISTORY_V1_SIZE, 4);
    put_le(temporary + 16, OBSCURA64_PROFILE_LIBRARY_VERSION, 4);
    put_le(temporary + 20, OBSCURA64_HISTORY_ENTRY_SIZE, 2);
    put_le(temporary + 22, OBSCURA64_HISTORY_CAPACITY, 2);
    put_le(temporary + 24, history->count, 2);
    memcpy(temporary + 32, history->project_id, OBSCURA64_PROJECT_ID_SIZE);
    for (i = 0; i < history->count; ++i) {
        unsigned char *entry = temporary + 64 + i * OBSCURA64_HISTORY_ENTRY_SIZE;
        put_le(entry, history->entries[i].generation, 8);
        memcpy(entry + 8, history->entries[i].profile, OBSCURA64_PROFILE_SIZE);
    }
    if (!history_hash(temporary, temporary + OBSCURA64_HISTORY_HASH_OFFSET))
        return OBSCURA64_CORE_STATUS_CRYPTO_FAILURE;
    memcpy(output, temporary, sizeof(temporary));
    return OBSCURA64_CORE_STATUS_SUCCESS;
}

obscura64_core_status obscura64_history_deserialize(
    const unsigned char *data, size_t length,
    obscura64_project_history *history)
{
    obscura64_project_history temporary;
    unsigned char digest[32];
    size_t i;
    if (data == NULL || history == NULL) return OBSCURA64_CORE_STATUS_INVALID_ARGUMENT;
    if (length != OBSCURA64_HISTORY_V1_SIZE || memcmp(data, "OB64HI01", 8) != 0 ||
        get_le(data + 8, 2) != 1 || get_le(data + 10, 2) != 64 ||
        get_le(data + 12, 4) != OBSCURA64_HISTORY_V1_SIZE ||
        get_le(data + 16, 4) != OBSCURA64_PROFILE_LIBRARY_VERSION ||
        get_le(data + 20, 2) != OBSCURA64_HISTORY_ENTRY_SIZE ||
        get_le(data + 22, 2) != OBSCURA64_HISTORY_CAPACITY ||
        get_le(data + 24, 2) > OBSCURA64_HISTORY_CAPACITY ||
        get_le(data + 26, 2) != 0 || !all_zero(data + 28, 4) || !all_zero(data + 48, 16))
        return OBSCURA64_CORE_STATUS_INVALID_DATA;
    if (!history_hash(data, digest)) return OBSCURA64_CORE_STATUS_CRYPTO_FAILURE;
    if (memcmp(data + OBSCURA64_HISTORY_HASH_OFFSET, digest, sizeof(digest)) != 0)
        return OBSCURA64_CORE_STATUS_INVALID_DATA;
    memset(&temporary, 0, sizeof(temporary));
    memcpy(temporary.project_id, data + 32, OBSCURA64_PROJECT_ID_SIZE);
    temporary.count = (uint16_t)get_le(data + 24, 2);
    for (i = 0; i < OBSCURA64_HISTORY_CAPACITY; ++i) {
        const unsigned char *entry = data + 64 + i * OBSCURA64_HISTORY_ENTRY_SIZE;
        if (i < temporary.count) {
            if (!all_zero(entry + 72, 8)) return OBSCURA64_CORE_STATUS_INVALID_DATA;
            temporary.entries[i].generation = get_le(entry, 8);
            memcpy(temporary.entries[i].profile, entry + 8, OBSCURA64_PROFILE_SIZE);
        } else if (!all_zero(entry, OBSCURA64_HISTORY_ENTRY_SIZE)) {
            return OBSCURA64_CORE_STATUS_INVALID_DATA;
        }
    }
    if (!obscura64_history_validate(&temporary)) return OBSCURA64_CORE_STATUS_INVALID_DATA;
    *history = temporary;
    return OBSCURA64_CORE_STATUS_SUCCESS;
}
