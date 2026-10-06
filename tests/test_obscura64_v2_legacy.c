#include "obscura64.h"
#include "obscura64_profiles_v1.h"

#include <stdio.h>
#include <string.h>

static unsigned int checks;
#define CHECK(label, ok) do { ++checks; if (!(ok)) { \
    fprintf(stderr, "FAIL %s line %d\n", label, __LINE__); return 1; \
} } while (0)

/* A genuine V1 custom Provider that stores Managed Payload bytes as-is. */
static obscura64_status identity_encoded_size(void *user_data,
    const char profile[OBSCURA64_PROFILE_SIZE], size_t input_len, size_t *output_len)
{
    (void)user_data; (void)profile;
    *output_len = input_len;
    return OBSCURA64_OK;
}
static obscura64_status identity_decoded_size(void *user_data,
    const char profile[OBSCURA64_PROFILE_SIZE], const char *encoded,
    size_t encoded_len, size_t *output_len)
{
    (void)user_data; (void)profile; (void)encoded;
    *output_len = encoded_len;
    return OBSCURA64_OK;
}
static obscura64_status identity_encode(void *user_data,
    const char profile[OBSCURA64_PROFILE_SIZE], const void *input,
    size_t input_len, char *output, size_t capacity, size_t *output_len)
{
    (void)user_data; (void)profile;
    *output_len = input_len;
    if (capacity < input_len) return OBSCURA64_BUFFER_TOO_SMALL;
    if (input_len != 0) memcpy(output, input, input_len);
    return OBSCURA64_OK;
}
static obscura64_status identity_decode(void *user_data,
    const char profile[OBSCURA64_PROFILE_SIZE], const char *encoded,
    size_t encoded_len, void *output, size_t capacity, size_t *output_len)
{
    return identity_encode(user_data, profile, encoded, encoded_len,
        (char *)output, capacity, output_len);
}

static int custom_provider_boundary(void)
{
    const unsigned char payload[] = {0, 1, 255, 0};
    obscura64_provider provider = {sizeof(provider), OBSCURA64_PROVIDER_ABI_VERSION,
        NULL, identity_encoded_size, identity_decoded_size,
        identity_encode, identity_decode};
    obscura64_context *context = NULL;
    char *encoded = NULL;
    void *decoded = NULL;
    size_t encoded_len = 0, decoded_len = 0;
    CHECK("custom V1 context", obscura64_context_create_from_profile_with_provider(
        (const char *)obscura64_profiles_v1[0], OBSCURA64_PROFILE_SIZE,
        &provider, &context) == OBSCURA64_OK);
    CHECK("custom V1 Managed output", obscura64_managed_encode_alloc(context,
        payload, sizeof(payload), &encoded, &encoded_len) == OBSCURA64_OK);
    CHECK("custom V1 API reads it", obscura64_managed_decode_alloc(context,
        encoded, encoded_len, &decoded, &decoded_len) == OBSCURA64_OK &&
        decoded_len == sizeof(payload) && memcmp(decoded, payload, sizeof(payload)) == 0);
    obscura64_free(decoded); decoded = NULL;
    CHECK("ordinary V2 does not claim custom Provider", obscura64_unprotect_alloc(
        encoded, encoded_len, &decoded, &decoded_len) == OBSCURA64_UNRECOGNIZED_DATA &&
        decoded == NULL && decoded_len == 0);
    obscura64_free(encoded);
    obscura64_context_destroy(context);
    return 0;
}

static int legacy_profile(uint16_t id, const void *input, size_t input_len)
{
    obscura64_context *context = NULL;
    char *legacy = NULL;
    size_t legacy_len = 0;
    void *decoded = NULL, *migrated = NULL, *again = NULL;
    size_t decoded_len = 0, migrated_len = 0, again_len = 0;
    obscura64_data_format format = OBSCURA64_FORMAT_UNKNOWN;
    const obscura64_protection targets[] = {
        OBSCURA64_PROTECTION_NONE,
        OBSCURA64_PROTECTION_CASUAL,
        OBSCURA64_PROTECTION_CURRENT_USER
    };
    size_t i;
    CHECK("V1 builtin context", obscura64_context_create_from_profile(
        (const char *)obscura64_profiles_v1[id], OBSCURA64_PROFILE_SIZE,
        &context) == OBSCURA64_OK);
    CHECK("V1 Managed encode", obscura64_managed_encode_alloc(context,
        input, input_len, &legacy, &legacy_len) == OBSCURA64_OK);
    CHECK("automatic legacy unprotect", obscura64_unprotect_alloc_ex(legacy,
        legacy_len, &decoded, &decoded_len, &format) == OBSCURA64_OK &&
        format == OBSCURA64_FORMAT_V1_LEGACY_UPGRADE_RECOMMENDED &&
        decoded_len == input_len &&
        (input_len == 0 || memcmp(decoded, input, input_len) == 0));
    obscura64_free(decoded); decoded = NULL;
    for (i = 0; i < sizeof(targets)/sizeof(targets[0]); ++i) {
        CHECK("V1 to V2 migration", obscura64_migrate_v1_managed_alloc(
            targets[i], legacy, legacy_len, &migrated, &migrated_len) == OBSCURA64_OK &&
            migrated != NULL && migrated_len >= 24 &&
            ((unsigned char *)migrated)[12] == (unsigned char)targets[i]);
        CHECK("migrated V2 read", obscura64_unprotect_alloc_ex(migrated,
            migrated_len, &again, &again_len, &format) == OBSCURA64_OK &&
            format == OBSCURA64_FORMAT_V2 && again_len == input_len &&
            (input_len == 0 || memcmp(again, input, input_len) == 0));
        obscura64_free(again); again = NULL;
        obscura64_free(migrated); migrated = NULL;
    }
    obscura64_free(legacy);
    obscura64_context_destroy(context);
    return 0;
}

int main(void)
{
    const unsigned char payload[] = {0, 255, 0, 'L', 'G'};
    void *out = (void *)1;
    size_t length = 99;
    obscura64_data_format format = OBSCURA64_FORMAT_V2;
    CHECK("Profile 0", legacy_profile(0, payload, sizeof(payload)) == 0);
    CHECK("nontrivial Profile", legacy_profile(137, payload, sizeof(payload)) == 0);
    CHECK("Profile 4095", legacy_profile(4095, payload, sizeof(payload)) == 0);
    CHECK("empty legacy payload", legacy_profile(0, NULL, 0) == 0);
    CHECK("not legacy", obscura64_unprotect_alloc_ex("AAAA", 4,
        &out, &length, &format) == OBSCURA64_UNRECOGNIZED_DATA &&
        out == NULL && length == 0 && format == OBSCURA64_FORMAT_UNKNOWN);
    out = (void *)1; length = 99;
    CHECK("V2 magic no legacy fallback", obscura64_unprotect_alloc(
        "OB64ENV2", 8, &out, &length) == OBSCURA64_ENVELOPE_CORRUPT &&
        out == NULL && length == 0);
    out = (void *)1; length = 99;
    CHECK("custom Provider boundary", custom_provider_boundary() == 0);
    printf("V2 legacy: %u checks passed\n", checks);
    return 0;
}
