#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include "obscura64_sha256.h"

static int hash_segment(BCRYPT_HASH_HANDLE hash, const void *bytes, size_t length)
{
    const unsigned char *p = (const unsigned char *)bytes;
    while (length != 0) {
        ULONG chunk = length > MAXDWORD ? MAXDWORD : (ULONG)length;
        if (!BCRYPT_SUCCESS(BCryptHashData(hash, (PUCHAR)p, chunk, 0))) return 0;
        p += chunk;
        length -= chunk;
    }
    return 1;
}

obscura64_status obscura64_sha256_segments(const void *first, size_t first_len,
    const void *second, size_t second_len, unsigned char digest[32])
{
    BCRYPT_ALG_HANDLE alg = NULL;
    BCRYPT_HASH_HANDLE hash = NULL;
    obscura64_status status = OBSCURA64_IO_ERROR;
    if ((first == NULL && first_len != 0) ||
        (second == NULL && second_len != 0) || digest == NULL)
        return OBSCURA64_INVALID_ARGUMENT;
    if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM,
        NULL, 0))) goto done;
    if (!BCRYPT_SUCCESS(BCryptCreateHash(alg, &hash, NULL, 0, NULL, 0, 0)))
        goto done;
    if (!hash_segment(hash, first, first_len) ||
        !hash_segment(hash, second, second_len)) goto done;
    if (BCRYPT_SUCCESS(BCryptFinishHash(hash, digest, 32, 0)))
        status = OBSCURA64_OK;
done:
    if (hash != NULL) BCryptDestroyHash(hash);
    if (alg != NULL) BCryptCloseAlgorithmProvider(alg, 0);
    return status;
}
