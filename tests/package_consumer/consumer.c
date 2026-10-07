#include <obscura64.h>
#include <string.h>

int main(void)
{
    static const unsigned char input[] = {0, 1, 255};
    void *protected_data = 0, *decoded = 0;
    size_t protected_len = 0, decoded_len = 0;
    obscura64_status status = obscura64_protect_alloc(
        OBSCURA64_PROTECTION_NONE, input, sizeof(input),
        &protected_data, &protected_len);
    if (status == OBSCURA64_OK)
        status = obscura64_unprotect_alloc(protected_data, protected_len,
            &decoded, &decoded_len);
    status = status == OBSCURA64_OK && decoded_len == sizeof(input) &&
        memcmp(decoded, input, sizeof(input)) == 0 ? OBSCURA64_OK :
        OBSCURA64_PROTECTION_FAILURE;
    obscura64_free(decoded);
    obscura64_free(protected_data);
    return status == OBSCURA64_OK ? 0 : 1;
}
