#include "obscura64.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

typedef struct provider_counts {
    size_t encoded_size;
    size_t decoded_size;
    size_t encode;
    size_t decode;
} provider_counts;

static size_t checks;
static size_t failures;
static const obscura64_provider *builtin;

static void check(int condition, const char *name)
{
    ++checks;
    if (!condition) {
        ++failures;
        fprintf(stderr, "FAIL: %s\n", name);
    }
}

static obscura64_status count_encoded_size(
    void *user_data, const char profile[OBSCURA64_PROFILE_SIZE],
    size_t input_len, size_t *output_len)
{
    provider_counts *counts = (provider_counts *)user_data;
    ++counts->encoded_size;
    return builtin->encoded_size(builtin->user_data, profile, input_len, output_len);
}

static obscura64_status count_decoded_size(
    void *user_data, const char profile[OBSCURA64_PROFILE_SIZE],
    const char *encoded, size_t encoded_len, size_t *output_len)
{
    provider_counts *counts = (provider_counts *)user_data;
    ++counts->decoded_size;
    return builtin->decoded_size(builtin->user_data, profile, encoded,
                                 encoded_len, output_len);
}

static obscura64_status count_encode(
    void *user_data, const char profile[OBSCURA64_PROFILE_SIZE],
    const void *input, size_t input_len, char *output, size_t output_capacity,
    size_t *output_len)
{
    provider_counts *counts = (provider_counts *)user_data;
    ++counts->encode;
    return builtin->encode(builtin->user_data, profile, input, input_len,
                           output, output_capacity, output_len);
}

static obscura64_status count_decode(
    void *user_data, const char profile[OBSCURA64_PROFILE_SIZE],
    const char *encoded, size_t encoded_len, void *output,
    size_t output_capacity, size_t *output_len)
{
    provider_counts *counts = (provider_counts *)user_data;
    ++counts->decode;
    return builtin->decode(builtin->user_data, profile, encoded, encoded_len,
                           output, output_capacity, output_len);
}

static void expect_invalid_provider(
    const obscura64_provider *provider, const char *name)
{
    static const char profile[] =
        "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnpqrstuvwxyz23456789@#$%&!?~";
    obscura64_context *context = (obscura64_context *)1;
    const obscura64_status status = obscura64_context_create_from_profile_with_provider(
        profile, sizeof(profile) - 1, provider, &context);

    check(status == OBSCURA64_INVALID_PROVIDER && context == NULL, name);
}

int main(void)
{
    static const char profile[] =
        "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnpqrstuvwxyz23456789@#$%&!?~";
    static const char input[] = "foobar";
    const char *expected = "bq!zaqF4";
    provider_counts counts = {0, 0, 0, 0};
    obscura64_provider custom;
    obscura64_provider invalid;
    obscura64_context *context = NULL;
    obscura64_context *normal_context = NULL;
    obscura64_context *builtin_context = NULL;
    size_t encoded_len = 0;
    size_t decoded_len = 0;
    char encoded[16];
    char encoded_normal[16];
    char encoded_builtin[16];
    char *allocated_encoded = NULL;
    void *allocated_decoded = NULL;
    unsigned int callback_total;

    builtin = obscura64_builtin_provider();
    check(builtin != NULL && builtin->struct_size == sizeof(*builtin) &&
              builtin->abi_version == OBSCURA64_PROVIDER_ABI_VERSION,
          "builtin Provider accessor returns current static Provider");
    check(builtin != NULL && builtin->user_data == NULL,
          "builtin Provider user_data is NULL");

    memset(&custom, 0, sizeof(custom));
    custom.struct_size = sizeof(custom);
    custom.abi_version = OBSCURA64_PROVIDER_ABI_VERSION;
    custom.user_data = &counts;
    custom.encoded_size = count_encoded_size;
    custom.decoded_size = count_decoded_size;
    custom.encode = count_encode;
    custom.decode = count_decode;
    check(obscura64_context_create_from_profile_with_provider(
              profile, sizeof(profile) - 1, &custom, &context) == OBSCURA64_OK,
          "valid custom Provider context creates");

    if (context != NULL) {
        /* Prove the context copied the callbacks before mutating the source. */
        custom.encode = NULL;
        check(obscura64_encoded_size(context, sizeof(input) - 1,
                                     &encoded_len) == OBSCURA64_OK && encoded_len == 8,
              "custom encoded_size callback is dispatched");
        check(obscura64_encode(context, input, sizeof(input) - 1, encoded,
                               sizeof(encoded), &encoded_len) == OBSCURA64_OK &&
                  encoded_len == 8 && memcmp(encoded, expected, 8) == 0,
              "copied custom encode callback returns builtin bytes");
        check(obscura64_encode_alloc(context, input, sizeof(input) - 1,
                                     &allocated_encoded, &encoded_len) == OBSCURA64_OK &&
                  encoded_len == 8 && memcmp(allocated_encoded, expected, 8) == 0,
              "custom encode_alloc dispatches through Provider callbacks");
        check(obscura64_decoded_size(context, expected, 8, &decoded_len) ==
                  OBSCURA64_OK && decoded_len == sizeof(input) - 1,
              "custom decoded_size callback is dispatched");
        check(obscura64_decode(context, expected, 8, encoded, sizeof(encoded),
                               &decoded_len) == OBSCURA64_OK && decoded_len == 6 &&
                  memcmp(encoded, input, 6) == 0,
              "custom decode callback returns builtin bytes");
        check(obscura64_decode_alloc(context, expected, 8, &allocated_decoded,
                                     &decoded_len) == OBSCURA64_OK && decoded_len == 6 &&
                  memcmp(allocated_decoded, input, 6) == 0,
              "custom decode_alloc dispatches through Provider callbacks");
        obscura64_free(allocated_encoded);
        obscura64_free(allocated_decoded);
    }

    check(counts.encoded_size >= 2 && counts.decoded_size >= 2 &&
              counts.encode >= 2 && counts.decode >= 2,
          "all four runtime callbacks ran and updated user_data counters");

    check(obscura64_context_create_from_profile(profile, sizeof(profile) - 1,
              &normal_context) == OBSCURA64_OK,
          "normal default context creates");
    check(obscura64_context_create_from_profile_with_provider(
              profile, sizeof(profile) - 1, builtin, &builtin_context) == OBSCURA64_OK,
          "explicit builtin context creates");
    if (normal_context != NULL && builtin_context != NULL) {
        check(obscura64_encode(normal_context, input, sizeof(input) - 1,
                  encoded_normal, sizeof(encoded_normal), &encoded_len) == OBSCURA64_OK &&
              obscura64_encode(builtin_context, input, sizeof(input) - 1,
                  encoded_builtin, sizeof(encoded_builtin), &decoded_len) == OBSCURA64_OK &&
              encoded_len == decoded_len && memcmp(encoded_normal, encoded_builtin,
                  encoded_len) == 0,
              "normal and explicit builtin Provider output are identical");
    }

    expect_invalid_provider(NULL, "NULL Provider rejected");
    invalid = *builtin;
    invalid.abi_version++;
    expect_invalid_provider(&invalid, "wrong Provider ABI rejected");
    invalid = *builtin;
    invalid.struct_size--;
    expect_invalid_provider(&invalid, "undersized Provider rejected");
    invalid = *builtin; invalid.encoded_size = NULL;
    expect_invalid_provider(&invalid, "missing encoded_size callback rejected");
    invalid = *builtin; invalid.decoded_size = NULL;
    expect_invalid_provider(&invalid, "missing decoded_size callback rejected");
    invalid = *builtin; invalid.encode = NULL;
    expect_invalid_provider(&invalid, "missing encode callback rejected");
    invalid = *builtin; invalid.decode = NULL;
    expect_invalid_provider(&invalid, "missing decode callback rejected");

    check(strcmp(obscura64_status_string(OBSCURA64_INVALID_PROVIDER),
                 "invalid provider") == 0,
          "invalid Provider has public status string");

    obscura64_context_destroy(context);
    obscura64_context_destroy(normal_context);
    obscura64_context_destroy(builtin_context);
    callback_total = (unsigned int)(counts.encoded_size + counts.decoded_size +
                                    counts.encode + counts.decode);
    if (failures != 0) {
        fprintf(stderr, "%zu of %zu Provider checks failed\n", failures, checks);
        return 1;
    }
    printf("Provider checks passed: %zu; callback calls: %u\n", checks, callback_total);
    return 0;
}
