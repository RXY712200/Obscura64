#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>

#include "obscura64_v2_internal.h"

#include <stdlib.h>
#include <string.h>

static obscura64_status dpapi_error(void)
{
    DWORD error = GetLastError();
    if (error == ERROR_NOT_ENOUGH_MEMORY || error == ERROR_OUTOFMEMORY)
        return OBSCURA64_OUT_OF_MEMORY;
    /* Windows does not reliably distinguish wrong user from damaged blob. */
    return OBSCURA64_PROTECTION_FAILURE;
}

static obscura64_status copy_windows_blob(DATA_BLOB *blob,
    void **output, size_t *output_len)
{
    void *result = NULL;
    if (blob->cbData != 0) {
        if (blob->pbData == NULL) return OBSCURA64_PROTECTION_FAILURE;
        result = malloc((size_t)blob->cbData);
        if (result == NULL) return OBSCURA64_OUT_OF_MEMORY;
        memcpy(result, blob->pbData, (size_t)blob->cbData);
    }
    *output = result;
    *output_len = (size_t)blob->cbData;
    return OBSCURA64_OK;
}

obscura64_status obscura64_v2_current_user_protect(const void *input,
    size_t length, void **body, size_t *body_len)
{
    DATA_BLOB source, protected_blob = {0, NULL};
    unsigned char *record = NULL;
    size_t record_len = 0;
    obscura64_status status;
    if (body != NULL) *body = NULL;
    if (body_len != NULL) *body_len = 0;
    if (body == NULL || body_len == NULL || (input == NULL && length != 0))
        return OBSCURA64_INVALID_ARGUMENT;
    status = obscura64_v2_current_user_record_build(input, length,
        &record, &record_len);
    if (status != OBSCURA64_OK) return status;
    source.cbData = (DWORD)record_len; /* Builder checks the complete record. */
    source.pbData = record;
    if (!CryptProtectData(&source, NULL, NULL, NULL, NULL,
        CRYPTPROTECT_UI_FORBIDDEN, &protected_blob)) {
        status = dpapi_error();
        SecureZeroMemory(record, record_len);
        free(record);
        LocalFree(protected_blob.pbData);
        return status;
    }
    SecureZeroMemory(record, record_len);
    free(record);
    status = copy_windows_blob(&protected_blob, body, body_len);
    LocalFree(protected_blob.pbData);
    return status;
}

obscura64_status obscura64_v2_current_user_unprotect(const void *body,
    size_t body_len, void **output, size_t *output_len)
{
    DATA_BLOB source, plain_blob = {0, NULL};
    unsigned char empty = 0;
    const unsigned char *payload = NULL;
    size_t payload_len = 0;
    obscura64_status status;
    if (output != NULL) *output = NULL;
    if (output_len != NULL) *output_len = 0;
    if (output == NULL || output_len == NULL || (body == NULL && body_len != 0))
        return OBSCURA64_INVALID_ARGUMENT;
    if (body_len > MAXDWORD) return OBSCURA64_SIZE_OVERFLOW;
    source.cbData = (DWORD)body_len;
    source.pbData = (BYTE *)(body_len != 0 ? body : &empty);
    if (!CryptUnprotectData(&source, NULL, NULL, NULL, NULL,
        CRYPTPROTECT_UI_FORBIDDEN, &plain_blob)) {
        status = dpapi_error();
        LocalFree(plain_blob.pbData);
        return status;
    }
    status = obscura64_v2_current_user_record_parse(plain_blob.pbData,
        (size_t)plain_blob.cbData, &payload, &payload_len);
    if (status == OBSCURA64_OK && payload_len != 0) {
        *output = malloc(payload_len);
        if (*output == NULL) status = OBSCURA64_OUT_OF_MEMORY;
        else { memcpy(*output, payload, payload_len); *output_len = payload_len; }
    }
    if (plain_blob.pbData != NULL)
        SecureZeroMemory(plain_blob.pbData, plain_blob.cbData);
    LocalFree(plain_blob.pbData);
    return status;
}
