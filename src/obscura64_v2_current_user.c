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
    unsigned char empty = 0;
    obscura64_status status;
    if (body != NULL) *body = NULL;
    if (body_len != NULL) *body_len = 0;
    if (body == NULL || body_len == NULL || (input == NULL && length != 0))
        return OBSCURA64_INVALID_ARGUMENT;
    if (length > MAXDWORD) return OBSCURA64_SIZE_OVERFLOW;
    source.cbData = (DWORD)length;
    source.pbData = (BYTE *)(length != 0 ? input : &empty);
    if (!CryptProtectData(&source, NULL, NULL, NULL, NULL,
        CRYPTPROTECT_UI_FORBIDDEN, &protected_blob)) {
        status = dpapi_error();
        LocalFree(protected_blob.pbData);
        return status;
    }
    status = copy_windows_blob(&protected_blob, body, body_len);
    LocalFree(protected_blob.pbData);
    return status;
}

obscura64_status obscura64_v2_current_user_unprotect(const void *body,
    size_t body_len, void **output, size_t *output_len)
{
    DATA_BLOB source, plain_blob = {0, NULL};
    unsigned char empty = 0;
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
    status = copy_windows_blob(&plain_blob, output, output_len);
    LocalFree(plain_blob.pbData);
    return status;
}
