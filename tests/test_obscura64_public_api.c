#include "obscura64.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static size_t checks;
static size_t failures;

static void check(int condition, const char *name)
{
    ++checks;
    if (!condition) {
        ++failures;
        fprintf(stderr, "FAIL: %s\n", name);
    }
}

static int create_test_context(obscura64_context **context)
{
    static const char profile[] =
        "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnpqrstuvwxyz23456789@#$%&!?~";
    return obscura64_context_create_from_profile(profile, sizeof(profile) - 1, context) == OBSCURA64_OK;
}

static void check_vector(obscura64_context *context, const uint8_t *input, size_t input_len,
                         const char *expected, const char *name)
{
    char encoded[32];
    uint8_t decoded[32];
    size_t encoded_len = 99;
    size_t decoded_len = 99;
    const size_t expected_len = strlen(expected);

    check(obscura64_encode(context, input, input_len, encoded, sizeof(encoded), &encoded_len) == OBSCURA64_OK,
          name);
    check(encoded_len == expected_len && memcmp(encoded, expected, expected_len) == 0,
          "encoded vector matches expected bytes");
    check(obscura64_decode(context, expected, expected_len, decoded, sizeof(decoded), &decoded_len) == OBSCURA64_OK,
          "decode vector succeeds");
    check(decoded_len == input_len &&
              (input_len == 0 || memcmp(decoded, input, input_len) == 0),
          "decoded vector matches input");
}

int main(void)
{
    static const uint8_t f[] = {'f'};
    static const uint8_t fo[] = {'f', 'o'};
    static const uint8_t foo[] = {'f', 'o', 'o'};
    static const uint8_t foob[] = {'f', 'o', 'o', 'b'};
    static const uint8_t fooba[] = {'f', 'o', 'o', 'b', 'a'};
    static const uint8_t foobar[] = {'f', 'o', 'o', 'b', 'a', 'r'};
    static const uint8_t binary[] = {0x00, 0x01, 0x7F, 0x80, 0xFF, 0x00};
    static const uint8_t multi_nul[] = {0x00, 0x00, 0x41, 0x00, 0xFF, 0x00, 0x00};
    static const char profile[] =
        "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnpqrstuvwxyz23456789@#$%&!?~";
    obscura64_context *context = NULL;
    obscura64_context *unchanged;
    char encoded[32];
    char encoded_before[sizeof(encoded)];
    uint8_t decoded[16];
    uint8_t before[sizeof(decoded)];
    size_t length;
    size_t i;

    check(create_test_context(&context), "valid profile creates context");
    check(context != NULL, "successful context is non-NULL");
    check(obscura64_context_create_from_profile(NULL, 64, &unchanged) == OBSCURA64_INVALID_ARGUMENT,
          "NULL Profile rejected");
    unchanged = context;
    check(obscura64_context_create_from_profile(profile, 63, &unchanged) == OBSCURA64_INVALID_PROFILE &&
              unchanged == NULL,
          "short Profile rejected and output reset");
    check(obscura64_context_create_from_profile(profile, 65, &unchanged) == OBSCURA64_INVALID_PROFILE,
          "long Profile rejected");
    {
        char bad_profile[sizeof(profile) - 1];
        memcpy(bad_profile, profile, sizeof(bad_profile));
        bad_profile[0] = bad_profile[1];
        check(obscura64_context_create_from_profile(bad_profile, sizeof(bad_profile), &unchanged) ==
                  OBSCURA64_INVALID_PROFILE,
              "duplicate Profile character rejected");
        memcpy(bad_profile, profile, sizeof(bad_profile));
        bad_profile[0] = '0';
        check(obscura64_context_create_from_profile(bad_profile, sizeof(bad_profile), &unchanged) ==
                  OBSCURA64_INVALID_PROFILE,
              "illegal Profile character rejected");
    }
    check(obscura64_context_create_from_profile(profile, 64, NULL) == OBSCURA64_INVALID_ARGUMENT,
          "NULL context output rejected");
    obscura64_context_destroy(NULL);
    check(1, "destroy accepts NULL");

    if (context != NULL) {
        check_vector(context, NULL, 0, "", "empty vector encode succeeds");
        check_vector(context, f, sizeof(f), "bi==", "f vector encode succeeds");
        check_vector(context, fo, sizeof(fo), "bq&=", "fo vector encode succeeds");
        check_vector(context, foo, sizeof(foo), "bq!z", "foo vector encode succeeds");
        check_vector(context, foob, sizeof(foob), "bq!zai==", "foob vector encode succeeds");
        check_vector(context, fooba, sizeof(fooba), "bq!zaqE=", "fooba vector encode succeeds");
        check_vector(context, foobar, sizeof(foobar), "bq!zaqF4", "foobar vector encode succeeds");

        length = 0;
        check(obscura64_encoded_size(context, 5, &length) == OBSCURA64_OK && length == 8,
              "encoded size returns padded length");
        check(obscura64_encoded_size(context, SIZE_MAX, &length) == OBSCURA64_SIZE_OVERFLOW && length == 0,
              "encoded size overflow reported");
        length = 0;
        check(obscura64_decoded_size(context, "bq!zaqE=", 8, &length) == OBSCURA64_OK && length == 5,
              "decoded size returns byte length");
        check(obscura64_decoded_size(context, "bj==", 4, &length) == OBSCURA64_INVALID_DATA && length == 0,
              "malformed input rejected by size query");

        memset(encoded, 0xA5, sizeof(encoded));
        memcpy(encoded_before, encoded, sizeof(encoded));
        length = 7;
        check(obscura64_encode(context, binary, sizeof(binary), encoded, 4, &length) == OBSCURA64_BUFFER_TOO_SMALL &&
                  length == 8,
              "encode reports required capacity");
        check(memcmp(encoded, encoded_before, sizeof(encoded)) == 0,
              "encode output unchanged when too small");
        memset(decoded, 0xA5, sizeof(decoded));
        memcpy(before, decoded, sizeof(decoded));
        length = 7;
        check(obscura64_decode(context, "bq!zaqE=", 8, decoded, 4, &length) == OBSCURA64_BUFFER_TOO_SMALL &&
                  length == 5,
              "decode reports required capacity");
        check(memcmp(decoded, before, sizeof(decoded)) == 0,
              "decode output unchanged when too small");

        length = 0;
        check(obscura64_encode(context, binary, sizeof(binary), encoded, sizeof(encoded), &length) == OBSCURA64_OK,
              "binary encode succeeds");
        {
            size_t decoded_len = 0;
        check(obscura64_decode(context, encoded, length, decoded, sizeof(decoded), &decoded_len) == OBSCURA64_OK &&
                      decoded_len == sizeof(binary) && memcmp(decoded, binary, sizeof(binary)) == 0,
                  "binary round trip preserves NUL and high bytes");
            length = 0;
            check(obscura64_encode(context, multi_nul, sizeof(multi_nul), encoded,
                              sizeof(encoded), &length) == OBSCURA64_OK,
                  "multi-NUL binary encode succeeds");
            decoded_len = 0;
            check(obscura64_decode(context, encoded, length, decoded, sizeof(decoded),
                              &decoded_len) == OBSCURA64_OK &&
                      decoded_len == sizeof(multi_nul) &&
                      memcmp(decoded, multi_nul, sizeof(multi_nul)) == 0,
                  "multi-NUL binary round trip succeeds");
        }
        check(obscura64_decode(context, "a", 1, decoded, sizeof(decoded), &length) == OBSCURA64_INVALID_DATA,
              "invalid encoded data status exposed");
        check(obscura64_encode(NULL, binary, sizeof(binary), encoded, sizeof(encoded), &length) ==
                  OBSCURA64_INVALID_ARGUMENT,
              "NULL context rejected");

        {
            char *allocated = NULL;
            size_t allocated_len = 0;
            check(obscura64_encode_alloc(context, foobar, sizeof(foobar), &allocated,
                                    &allocated_len) == OBSCURA64_OK,
                  "encode_alloc succeeds");
            check(allocated != NULL && allocated_len == 8 &&
                      memcmp(allocated, "bq!zaqF4", 8) == 0,
                  "encode_alloc returns exact foobar bytes without terminator");
            obscura64_free(allocated);
        }
        {
            void *allocated = NULL;
            size_t allocated_len = 0;
            check(obscura64_decode_alloc(context, "bq!zaqF4", 8, &allocated,
                                    &allocated_len) == OBSCURA64_OK,
                  "decode_alloc succeeds");
            check(allocated != NULL && allocated_len == sizeof(foobar) &&
                      memcmp(allocated, foobar, sizeof(foobar)) == 0,
                  "decode_alloc returns foobar bytes");
            obscura64_free(allocated);
        }
        {
            static const uint8_t allocation_binary[] =
                {0x00, 0x01, 0x00, 0x7F, 0x80, 0xFF, 0x00};
            char *allocated_encoded = NULL;
            void *allocated_decoded = NULL;
            size_t allocated_encoded_len = 0;
            size_t allocated_decoded_len = 0;
            obscura64_status status = obscura64_encode_alloc(context, allocation_binary,
                sizeof(allocation_binary), &allocated_encoded, &allocated_encoded_len);
            check(status == OBSCURA64_OK, "binary encode_alloc succeeds");
            status = obscura64_decode_alloc(context, allocated_encoded,
                allocated_encoded_len, &allocated_decoded, &allocated_decoded_len);
            check(status == OBSCURA64_OK && allocated_decoded_len == sizeof(allocation_binary) &&
                      memcmp(allocated_decoded, allocation_binary,
                             sizeof(allocation_binary)) == 0,
                  "binary allocating round trip preserves all bytes");
            obscura64_free(allocated_encoded);
            obscura64_free(allocated_decoded);
        }
        {
            char *allocated_encoded = (char *)1;
            void *allocated_decoded = (void *)1;
            size_t allocated_encoded_len = 77;
            size_t allocated_decoded_len = 77;
            check(obscura64_encode_alloc(context, NULL, 0, &allocated_encoded,
                                    &allocated_encoded_len) == OBSCURA64_OK &&
                      allocated_encoded == NULL && allocated_encoded_len == 0,
                  "empty encode_alloc succeeds as NULL/0");
            check(obscura64_decode_alloc(context, NULL, 0, &allocated_decoded,
                                    &allocated_decoded_len) == OBSCURA64_OK &&
                      allocated_decoded == NULL && allocated_decoded_len == 0,
                  "empty decode_alloc succeeds as NULL/0");
        }
        {
            char *failed_encoded = (char *)1;
            void *failed_decoded = (void *)1;
            size_t failed_encoded_len = 55;
            size_t failed_decoded_len = 55;
            check(obscura64_encode_alloc(NULL, foobar, sizeof(foobar), &failed_encoded,
                                    &failed_encoded_len) == OBSCURA64_INVALID_ARGUMENT &&
                      failed_encoded == NULL && failed_encoded_len == 0,
                  "encode_alloc NULL context failure resets outputs");
            check(obscura64_decode_alloc(NULL, "bq!zaqF4", 8, &failed_decoded,
                                    &failed_decoded_len) == OBSCURA64_INVALID_ARGUMENT &&
                      failed_decoded == NULL && failed_decoded_len == 0,
                  "decode_alloc NULL context failure resets outputs");
            failed_encoded = (char *)1;
            failed_encoded_len = 55;
            check(obscura64_encode_alloc(context, foobar, sizeof(foobar), NULL,
                                    &failed_encoded_len) == OBSCURA64_INVALID_ARGUMENT &&
                      failed_encoded_len == 0,
                  "encode_alloc NULL output slot rejected");
            check(obscura64_encode_alloc(context, foobar, sizeof(foobar), &failed_encoded,
                                    NULL) == OBSCURA64_INVALID_ARGUMENT && failed_encoded == NULL,
                  "encode_alloc NULL length slot rejected");
            failed_decoded = (void *)1;
            failed_decoded_len = 55;
            check(obscura64_decode_alloc(context, "bq!zaqF4", 8, NULL,
                                    &failed_decoded_len) == OBSCURA64_INVALID_ARGUMENT &&
                      failed_decoded_len == 0,
                  "decode_alloc NULL output slot rejected");
            check(obscura64_decode_alloc(context, "bq!zaqF4", 8, &failed_decoded,
                                    NULL) == OBSCURA64_INVALID_ARGUMENT && failed_decoded == NULL,
                  "decode_alloc NULL length slot rejected");
            failed_decoded = (void *)1;
            failed_decoded_len = 55;
            check(obscura64_decode_alloc(context, "bj==", 4, &failed_decoded,
                                    &failed_decoded_len) == OBSCURA64_INVALID_DATA &&
                      failed_decoded == NULL && failed_decoded_len == 0,
                  "decode_alloc invalid canonical padding resets outputs");
            failed_decoded = (void *)1;
            failed_decoded_len = 55;
            check(obscura64_decode_alloc(context, "b=!=", 4, &failed_decoded,
                                    &failed_decoded_len) == OBSCURA64_INVALID_DATA &&
                      failed_decoded == NULL && failed_decoded_len == 0,
                  "decode_alloc malformed padding resets outputs");
        }
        obscura64_free(NULL);
        check(1, "obscura64_free accepts NULL");

        {
            obscura64_context *reverse_context = NULL;
            char reverse_profile[sizeof(profile) - 1];
            char forward_encoded[16];
            char reverse_encoded[16];
            uint8_t round_trip[sizeof(foobar)];
            size_t forward_len = 0;
            size_t reverse_len = 0;
            size_t round_trip_len = 0;
            for (i = 0; i < sizeof(reverse_profile); ++i) {
                reverse_profile[i] = profile[sizeof(reverse_profile) - 1 - i];
            }
            check(obscura64_context_create_from_profile(reverse_profile,
                      sizeof(reverse_profile), &reverse_context) == OBSCURA64_OK,
                  "reverse Profile context creates");
            if (reverse_context != NULL) {
                check(obscura64_encode(context, foobar, sizeof(foobar), forward_encoded,
                                  sizeof(forward_encoded), &forward_len) == OBSCURA64_OK &&
                          obscura64_encode(reverse_context, foobar, sizeof(foobar), reverse_encoded,
                                      sizeof(reverse_encoded), &reverse_len) == OBSCURA64_OK &&
                          (forward_len != reverse_len ||
                           memcmp(forward_encoded, reverse_encoded, forward_len) != 0),
                      "different contexts produce different encoding");
                check(obscura64_decode(context, forward_encoded, forward_len, round_trip,
                                  sizeof(round_trip), &round_trip_len) == OBSCURA64_OK &&
                          round_trip_len == sizeof(foobar) &&
                          memcmp(round_trip, foobar, sizeof(foobar)) == 0,
                      "context A decodes its own bytes");
                check(obscura64_decode(reverse_context, reverse_encoded, reverse_len, round_trip,
                                  sizeof(round_trip), &round_trip_len) == OBSCURA64_OK &&
                          round_trip_len == sizeof(foobar) &&
                          memcmp(round_trip, foobar, sizeof(foobar)) == 0,
                      "context B decodes its own bytes");
            }
            obscura64_context_destroy(reverse_context);
        }

        {
            char writable_profile[sizeof(profile) - 1];
            char copied_context_output[16];
            size_t copied_context_len = 0;
            obscura64_context *copied_context = NULL;
            memcpy(writable_profile, profile, sizeof(writable_profile));
            check(obscura64_context_create_from_profile(writable_profile,
                      sizeof(writable_profile), &copied_context) == OBSCURA64_OK,
                  "writable Profile context creates");
            memset(writable_profile, '0', sizeof(writable_profile));
            if (copied_context != NULL) {
                check(obscura64_encode(copied_context, foobar, sizeof(foobar),
                                  copied_context_output, sizeof(copied_context_output),
                                  &copied_context_len) == OBSCURA64_OK && copied_context_len == 8 &&
                          memcmp(copied_context_output, "bq!zaqF4", 8) == 0,
                      "context uses copied Profile after caller buffer changes");
            }
            obscura64_context_destroy(copied_context);
        }

        {
            int statuses_nonempty = 1;
            obscura64_status status;
            for (status = OBSCURA64_OK; status <= OBSCURA64_UNRECOVERABLE;
                 status = (obscura64_status)(status + 1)) {
                const char *description = obscura64_status_string(status);
                if (description == NULL || description[0] == '\0') {
                    statuses_nonempty = 0;
                }
            }
            check(statuses_nonempty, "every public status has a nonempty string");
        }
        check(strcmp(obscura64_status_string(OBSCURA64_OK), "success") == 0,
              "status string for success");
        check(strcmp(obscura64_status_string(OBSCURA64_UNRECOVERABLE),
                     "managed project state cannot be recovered") == 0,
              "stable UNRECOVERABLE status string");
        check(strcmp(obscura64_status_string((obscura64_status)999), "unknown status") == 0,
              "unknown status has fallback string");
    }

    obscura64_context_destroy(context);
    if (failures != 0) {
        fprintf(stderr, "%zu of %zu public API checks failed\n", failures, checks);
        return 1;
    }
    printf("public API checks passed: %zu\n", checks);
    return 0;
}
