#include <obscura64.h>
#include <cstring>

int main()
{
    const unsigned char input[] = {9, 0, 7};
    void *protected_data = nullptr;
    void *decoded = nullptr;
    size_t protected_len = 0, decoded_len = 0;
    obscura64_status status = obscura64_protect_alloc(
        OBSCURA64_PROTECTION_NONE, input, sizeof(input),
        &protected_data, &protected_len);
    if (status == OBSCURA64_OK)
        status = obscura64_unprotect_alloc(protected_data, protected_len,
            &decoded, &decoded_len);
    const bool okay = status == OBSCURA64_OK && decoded_len == sizeof(input) &&
        std::memcmp(decoded, input, sizeof(input)) == 0;
    obscura64_free(decoded);
    obscura64_free(protected_data);
    return okay ? 0 : 1;
}
