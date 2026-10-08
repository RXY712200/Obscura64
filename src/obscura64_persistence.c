#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include "obscura64_internal.h"

#include <stdlib.h>
#include <string.h>

#ifdef OBSCURA64_TESTING
#define FAULT(point, target, temp) obscura64_test_fault(point, target, temp)
#else
#define FAULT(point, target, temp) 0
#endif

#define TEMP_RETRIES 32U
#define TEMP_SUFFIX_LENGTH 21U /* .tmp. plus sixteen hex digits */

static int valid_arguments(const wchar_t *path, const void *data, size_t size,
                           obscura64_persistence_verify_fn verify)
{
#if SIZE_MAX > INT64_MAX
    if (size > (size_t)INT64_MAX) return 0;
#endif
    return path != NULL && path[0] != L'\0' && data != NULL && size != 0 &&
           verify != NULL;
}

static obscura64_status write_exact(HANDLE file, const unsigned char *data, size_t size)
{
    size_t position = 0;
    while (position < size) {
        size_t remaining = size - position;
        DWORD request = remaining > MAXDWORD ? MAXDWORD : (DWORD)remaining;
        DWORD written = 0;
        if (!WriteFile(file, data + position, request, &written, NULL) ||
            written == 0 || written > request) return OBSCURA64_IO_ERROR;
        position += written;
    }
    return OBSCURA64_OK;
}

obscura64_status obscura64_persistence_read_verified(
    const wchar_t *path, unsigned char *output, size_t expected_size,
    obscura64_persistence_verify_fn verify, void *user_data)
{
    HANDLE file;
    LARGE_INTEGER actual_size;
    unsigned char *bytes;
    unsigned char extra;
    DWORD count;
    size_t position = 0;
    obscura64_status status = OBSCURA64_IO_ERROR;
    DWORD attributes;
    if (!valid_arguments(path, output, expected_size, verify))
        return OBSCURA64_INVALID_ARGUMENT;
    attributes = GetFileAttributesW(path);
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        DWORD error = GetLastError();
        return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND ?
            OBSCURA64_STATE_MISSING : OBSCURA64_IO_ERROR;
    }
    if ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) return OBSCURA64_STATE_CORRUPT;
    file = CreateFileW(path, GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND ?
            OBSCURA64_STATE_MISSING : OBSCURA64_IO_ERROR;
    }
    bytes = (unsigned char *)malloc(expected_size);
    if (bytes == NULL) { CloseHandle(file); return OBSCURA64_OUT_OF_MEMORY; }
    if (!GetFileSizeEx(file, &actual_size)) goto done;
    if (actual_size.QuadPart < 0 || (uint64_t)actual_size.QuadPart != expected_size) {
        status = OBSCURA64_STATE_CORRUPT; goto done;
    }
    while (position < expected_size) {
        size_t remaining = expected_size - position;
        DWORD request = remaining > MAXDWORD ? MAXDWORD : (DWORD)remaining;
        count = 0;
        if (!ReadFile(file, bytes + position, request, &count, NULL)) goto done;
        if (count == 0 || count > request) { status = OBSCURA64_STATE_CORRUPT; goto done; }
        position += count;
    }
    if (!ReadFile(file, &extra, 1, &count, NULL)) goto done;
    if (count != 0) { status = OBSCURA64_STATE_CORRUPT; goto done; }
    status = OBSCURA64_OK;
done:
    if (!CloseHandle(file)) status = OBSCURA64_IO_ERROR;
    if (status == OBSCURA64_OK) status = verify(bytes, expected_size, user_data);
    if (status == OBSCURA64_OK) memcpy(output, bytes, expected_size);
    free(bytes);
    return status;
}

typedef struct verify_payload {
    const unsigned char *expected;
    obscura64_persistence_verify_fn verify;
    void *user_data;
} verify_payload;

static obscura64_status verify_exact_payload(
    const unsigned char *data, size_t size, void *user_data)
{
    verify_payload *payload = (verify_payload *)user_data;
    if (memcmp(data, payload->expected, size) != 0) return OBSCURA64_STATE_CORRUPT;
    return payload->verify(data, size, payload->user_data);
}

static obscura64_status verify_file(const wchar_t *path, const unsigned char *data,
    size_t size, obscura64_persistence_verify_fn verify, void *user_data)
{
    unsigned char *readback = (unsigned char *)malloc(size);
    verify_payload payload = { data, verify, user_data };
    obscura64_status status;
    if (readback == NULL) return OBSCURA64_OUT_OF_MEMORY;
    status = obscura64_persistence_read_verified(path, readback, size,
                                                verify_exact_payload, &payload);
    free(readback);
    return status;
}

obscura64_status obscura64_persistence_create_new_verified(
    const wchar_t *path, const unsigned char *data, size_t size,
    obscura64_persistence_verify_fn verify, void *user_data)
{
    HANDLE file;
    obscura64_status status;
    if (!valid_arguments(path, data, size, verify)) return OBSCURA64_INVALID_ARGUMENT;
    file = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                       CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        return error == ERROR_FILE_EXISTS || error == ERROR_ALREADY_EXISTS ?
            OBSCURA64_BUSY : OBSCURA64_IO_ERROR;
    }
    status = write_exact(file, data, size);
    if (status == OBSCURA64_OK && !FlushFileBuffers(file)) status = OBSCURA64_IO_ERROR;
    if (!CloseHandle(file)) status = OBSCURA64_IO_ERROR;
    if (status == OBSCURA64_OK) status = verify_file(path, data, size, verify, user_data);
    if (status != OBSCURA64_OK) (void)DeleteFileW(path); /* Own CREATE_NEW winner only. */
    return status;
}

static obscura64_status replace_once(const wchar_t *path, const unsigned char *data,
    size_t size, obscura64_persistence_verify_fn verify, void *user_data, int *renamed)
{
    static const wchar_t hex[] = L"0123456789abcdef";
    size_t length = wcslen(path);
    wchar_t *temporary;
    HANDLE file = INVALID_HANDLE_VALUE;
    unsigned int attempt;
    obscura64_status status = OBSCURA64_IO_ERROR;
    *renamed = 0;
    if (length > SIZE_MAX / sizeof(wchar_t) - TEMP_SUFFIX_LENGTH - 1U)
        return OBSCURA64_SIZE_OVERFLOW;
    temporary = (wchar_t *)malloc((length + TEMP_SUFFIX_LENGTH + 1U) * sizeof(*temporary));
    if (temporary == NULL) return OBSCURA64_OUT_OF_MEMORY;
    memcpy(temporary, path, length * sizeof(*temporary));
    memcpy(temporary + length, L".tmp.", 5U * sizeof(*temporary));
    temporary[length + TEMP_SUFFIX_LENGTH] = L'\0';
    for (size_t i = 0; i < 16U; ++i) temporary[length + 5U + i] = L'0';
    if (FAULT(OBSCURA64_FAULT_TEMP_CREATE, path, temporary)) goto done;
    for (attempt = 0; attempt < TEMP_RETRIES; ++attempt) {
        unsigned char random[8];
        size_t i;
        if (!BCRYPT_SUCCESS(BCryptGenRandom(NULL, random, sizeof(random),
                                            BCRYPT_USE_SYSTEM_PREFERRED_RNG))) {
            status = OBSCURA64_RNG_FAILURE; goto done;
        }
        for (i = 0; i < sizeof(random); ++i) {
            temporary[length + 5U + 2U * i] = hex[random[i] >> 4];
            temporary[length + 6U + 2U * i] = hex[random[i] & 15U];
        }
        file = CreateFileW(temporary, GENERIC_WRITE, 0, NULL, CREATE_NEW,
                           FILE_ATTRIBUTE_NORMAL, NULL);
        if (file != INVALID_HANDLE_VALUE) break;
        {
            DWORD error = GetLastError();
            if (error != ERROR_FILE_EXISTS && error != ERROR_ALREADY_EXISTS) goto done;
        }
    }
    if (file == INVALID_HANDLE_VALUE) goto done; /* Never remove collision files. */
    status = FAULT(OBSCURA64_FAULT_TEMP_WRITE, path, temporary) ?
        OBSCURA64_IO_ERROR : write_exact(file, data, size);
    if (status == OBSCURA64_OK && (FAULT(OBSCURA64_FAULT_TEMP_FLUSH, path, temporary) ||
        !FlushFileBuffers(file))) status = OBSCURA64_IO_ERROR;
    if (!CloseHandle(file)) status = OBSCURA64_IO_ERROR;
    if (status == OBSCURA64_OK && FAULT(OBSCURA64_FAULT_TEMP_REOPEN, path, temporary)) status = OBSCURA64_IO_ERROR;
    if (status == OBSCURA64_OK) status = verify_file(temporary, data, size, verify, user_data);
    if (status == OBSCURA64_OK && FAULT(OBSCURA64_FAULT_PRE_VERIFY, path, temporary)) status = OBSCURA64_STATE_CORRUPT;
    if (status == OBSCURA64_OK) {
        if (FAULT(OBSCURA64_FAULT_REPLACE, path, temporary) ||
            !MoveFileExW(temporary, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            status = OBSCURA64_IO_ERROR;
        else {
            *renamed = 1;
            /* AFTER_REPLACE is an observation/crash point in test builds only. */
#ifdef OBSCURA64_TESTING
            if (FAULT(OBSCURA64_FAULT_AFTER_REPLACE, path, temporary) ||
                FAULT(OBSCURA64_FAULT_FINAL_REOPEN, path, temporary))
                status = OBSCURA64_IO_ERROR;
            else
#endif
                status = verify_file(path, data, size, verify, user_data);
            if (status == OBSCURA64_OK && FAULT(OBSCURA64_FAULT_POST_VERIFY, path, temporary))
                status = OBSCURA64_STATE_CORRUPT;
        }
    }
    if (!*renamed) (void)DeleteFileW(temporary); /* Only our successfully created temp. */
done:
    free(temporary);
    return status;
}

obscura64_status obscura64_persistence_replace_verified(
    const wchar_t *path, const unsigned char *data, size_t size,
    obscura64_persistence_verify_fn verify, void *user_data,
    const unsigned char *old_data)
{
    int renamed;
    obscura64_status status;
    if (!valid_arguments(path, data, size, verify)) return OBSCURA64_INVALID_ARGUMENT;
    status = replace_once(path, data, size, verify, user_data, &renamed);
    if (status != OBSCURA64_OK && renamed && old_data != NULL) {
        int restored;
        if (replace_once(path, old_data, size, verify, user_data, &restored) != OBSCURA64_OK)
            return OBSCURA64_IO_ERROR;
    }
    return status;
}

obscura64_status obscura64_persistence_replace_blob_verified(
    const wchar_t *path, const unsigned char *data, size_t size,
    obscura64_persistence_verify_fn verify, void *user_data, int *renamed)
{
    if (renamed != NULL) *renamed = 0;
    if (renamed == NULL || !valid_arguments(path, data, size, verify))
        return OBSCURA64_INVALID_ARGUMENT;
    return replace_once(path, data, size, verify, user_data, renamed);
}
