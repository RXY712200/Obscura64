#include "obscura64_internal.h"

#include <stdio.h>
#include <string.h>

static size_t checks_run;
static size_t failures;

static void check(int condition, const char *name)
{
    ++checks_run;
    if (!condition) {
        ++failures;
        fprintf(stderr, "FAIL: %s\n", name);
    }
}

static void check_vector(
    const uint8_t *input,
    size_t input_len,
    const char *expected,
    const char *name)
{
    char encoded[32];
    uint8_t decoded[32];
    size_t encoded_len = 999;
    size_t decoded_len = 999;
    const size_t expected_len = strlen(expected);
    obscura64_core_status status;

    status = obscura64_codec_encode(input, input_len, obscura64_v1_character_pool,
                         OBSCURA64_ALPHABET_SIZE, encoded, sizeof(encoded), &encoded_len);
    check(status == OBSCURA64_CORE_STATUS_SUCCESS, name);
    check(encoded_len == expected_len, "vector encoded length");
    check(encoded_len == expected_len && memcmp(encoded, expected, expected_len) == 0,
          "vector encoded bytes");
    status = obscura64_codec_decode(expected, expected_len, obscura64_v1_character_pool,
                         OBSCURA64_ALPHABET_SIZE, decoded, sizeof(decoded), &decoded_len);
    check(status == OBSCURA64_CORE_STATUS_SUCCESS, "vector decode succeeds");
    check(decoded_len == input_len, "vector decoded length");
    check(decoded_len == input_len &&
              (input_len == 0 || memcmp(decoded, input, input_len) == 0),
          "vector decoded bytes");
}

static void check_invalid_data(const char *encoded, const char *name)
{
    uint8_t output[16];
    uint8_t before[sizeof(output)];
    size_t output_len = 123;
    obscura64_core_status status;

    memset(output, 0xA5, sizeof(output));
    memcpy(before, output, sizeof(output));
    status = obscura64_codec_decode(encoded, strlen(encoded), obscura64_v1_character_pool,
                         OBSCURA64_ALPHABET_SIZE, output, sizeof(output), &output_len);
    check(status == OBSCURA64_CORE_STATUS_INVALID_DATA, name);
    check(output_len == 0, "invalid data sets output length to zero");
    check(memcmp(output, before, sizeof(output)) == 0,
          "invalid data leaves output unchanged");
}

static int round_trip_with_profile(
    const char *profile,
    const uint8_t *input,
    size_t input_len)
{
    char encoded[1400];
    uint8_t decoded[1024];
    size_t encoded_len = 0;
    size_t decoded_len = 0;
    obscura64_core_status status;

    status = obscura64_codec_encode(input, input_len, profile, OBSCURA64_ALPHABET_SIZE,
                               encoded, sizeof(encoded), &encoded_len);
    if (status != OBSCURA64_CORE_STATUS_SUCCESS) {
        return 0;
    }
    status = obscura64_codec_decode(encoded, encoded_len, profile, OBSCURA64_ALPHABET_SIZE,
                               decoded, sizeof(decoded), &decoded_len);
    return status == OBSCURA64_CORE_STATUS_SUCCESS && decoded_len == input_len &&
           (input_len == 0 || memcmp(decoded, input, input_len) == 0);
}

int main(void)
{
    static const uint8_t f[] = {'f'};
    static const uint8_t fo[] = {'f', 'o'};
    static const uint8_t foo[] = {'f', 'o', 'o'};
    static const uint8_t foob[] = {'f', 'o', 'o', 'b'};
    static const uint8_t fooba[] = {'f', 'o', 'o', 'b', 'a'};
    static const uint8_t foobar[] = {'f', 'o', 'o', 'b', 'a', 'r'};
    static const uint8_t binary[] = {0x00, 0x01, 0x7F, 0x80, 0xFF};
    char reverse_profile[OBSCURA64_ALPHABET_SIZE];
    char rotated_profile[OBSCURA64_ALPHABET_SIZE];
    char encoded[32];
    uint8_t decoded[32];
    uint8_t output[16];
    uint8_t before[sizeof(output)];
    size_t output_len;
    size_t required;
    size_t i;
    obscura64_core_status status;

    check_vector(NULL, 0, "", "empty vector encode succeeds");
    check_vector(f, sizeof(f), "bi==", "f vector");
    check_vector(fo, sizeof(fo), "bq&=", "fo vector");
    check_vector(foo, sizeof(foo), "bq!z", "foo vector");
    check_vector(foob, sizeof(foob), "bq!zai==", "foob vector");
    check_vector(fooba, sizeof(fooba), "bq!zaqE=", "fooba vector");
    check_vector(foobar, sizeof(foobar), "bq!zaqF4", "foobar vector");

    status = obscura64_codec_encode(binary, sizeof(binary), obscura64_v1_character_pool,
                         OBSCURA64_ALPHABET_SIZE, encoded, sizeof(encoded), &output_len);
    check(status == OBSCURA64_CORE_STATUS_SUCCESS, "binary encode succeeds");
    status = obscura64_codec_decode(encoded, output_len, obscura64_v1_character_pool,
                         OBSCURA64_ALPHABET_SIZE, decoded, sizeof(decoded), &required);
    check(status == OBSCURA64_CORE_STATUS_SUCCESS, "binary decode succeeds");
    check(required == sizeof(binary) && memcmp(decoded, binary, sizeof(binary)) == 0,
          "binary round trip including NUL");

    for (i = 0; i < OBSCURA64_ALPHABET_SIZE; ++i) {
        reverse_profile[i] = obscura64_v1_character_pool[OBSCURA64_ALPHABET_SIZE - 1 - i];
    }
    status = obscura64_codec_encode(binary, sizeof(binary), reverse_profile,
                         sizeof(reverse_profile), encoded, sizeof(encoded), &output_len);
    check(status == OBSCURA64_CORE_STATUS_SUCCESS, "reverse profile encode succeeds");
    status = obscura64_codec_decode(encoded, output_len, reverse_profile,
                         sizeof(reverse_profile), decoded, sizeof(decoded), &required);
    check(status == OBSCURA64_CORE_STATUS_SUCCESS && required == sizeof(binary) &&
              memcmp(decoded, binary, sizeof(binary)) == 0,
          "reverse profile round trip");

    for (i = 0; i < OBSCURA64_ALPHABET_SIZE; ++i) {
        rotated_profile[i] = obscura64_v1_character_pool[(i + 17) % OBSCURA64_ALPHABET_SIZE];
    }
    check(obscura64_profile_is_valid(rotated_profile, sizeof(rotated_profile)),
          "rotated third profile is a valid permutation");
    check(round_trip_with_profile(obscura64_v1_character_pool, binary, sizeof(binary)),
          "default profile binary round trip");
    check(round_trip_with_profile(reverse_profile, binary, sizeof(binary)),
          "reverse profile binary round trip");
    check(round_trip_with_profile(rotated_profile, binary, sizeof(binary)),
          "rotated profile binary round trip");
    {
        static const uint8_t more_binary[] = {
            0x00, 0xFF, 0x10, 0x80, 0x7F, 0x55, 0xAA, 0x01
        };
        const char *profiles[] = {
            obscura64_v1_character_pool, reverse_profile, rotated_profile
        };
        static const size_t binary_lengths[] = {3, 5, 8};
        size_t profile_index;
        size_t length_index;
        for (profile_index = 0;
             profile_index < sizeof(profiles) / sizeof(profiles[0]);
             ++profile_index) {
            for (length_index = 0;
                 length_index < sizeof(binary_lengths) / sizeof(binary_lengths[0]);
                 ++length_index) {
                check(round_trip_with_profile(profiles[profile_index], more_binary,
                                              binary_lengths[length_index]),
                      "profile binary length round trip");
            }
        }
    }

    {
        uint8_t source[1024];
        static const size_t lengths[] = {
            0, 1, 2, 3, 4, 5, 6, 7, 8, 15, 16, 31, 32,
            63, 64, 65, 127, 128, 255, 256, 257, 511, 512, 1023, 1024
        };
        for (i = 0; i < sizeof(lengths) / sizeof(lengths[0]); ++i) {
            size_t j;
            const size_t len = lengths[i];
            for (j = 0; j < len; ++j) {
                source[j] = (uint8_t)((j * 37U + len * 13U) ^ (j >> 2));
            }
            check(round_trip_with_profile(obscura64_v1_character_pool, source, len),
                  "deterministic multi-length round trip");
        }
    }

    {
        int all_single_bytes_pass = 1;
        uint8_t source[1];
        char encoded_one[4];
        uint8_t decoded_one[1];
        for (i = 0; i < 256; ++i) {
            size_t encoded_len = 0;
            size_t decoded_len = 0;
            source[0] = (uint8_t)i;
            if (obscura64_codec_encode(source, 1, obscura64_v1_character_pool,
                                  OBSCURA64_ALPHABET_SIZE, encoded_one,
                                  sizeof(encoded_one), &encoded_len) != OBSCURA64_CORE_STATUS_SUCCESS ||
                obscura64_codec_decode(encoded_one, encoded_len, obscura64_v1_character_pool,
                                  OBSCURA64_ALPHABET_SIZE, decoded_one, sizeof(decoded_one),
                                  &decoded_len) != OBSCURA64_CORE_STATUS_SUCCESS ||
                decoded_len != 1 || decoded_one[0] != source[0]) {
                all_single_bytes_pass = 0;
            }
        }
        check(all_single_bytes_pass, "all 256 single-byte values round trip");
    }

    {
        int all_two_byte_pairs_pass = 1;
        uint8_t source[2];
        uint8_t decoded_pair[2];
        char encoded_pair[4];
        unsigned int first;
        unsigned int second;
        for (first = 0; first < 256; ++first) {
            for (second = 0; second < 256; ++second) {
                size_t encoded_len = 0;
                size_t decoded_len = 0;
                source[0] = (uint8_t)first;
                source[1] = (uint8_t)second;
                if (obscura64_codec_encode(source, 2, obscura64_v1_character_pool,
                                      OBSCURA64_ALPHABET_SIZE, encoded_pair,
                                      sizeof(encoded_pair), &encoded_len) != OBSCURA64_CORE_STATUS_SUCCESS ||
                    obscura64_codec_decode(encoded_pair, encoded_len, obscura64_v1_character_pool,
                                      OBSCURA64_ALPHABET_SIZE, decoded_pair,
                                      sizeof(decoded_pair), &decoded_len) != OBSCURA64_CORE_STATUS_SUCCESS ||
                    decoded_len != 2 || decoded_pair[0] != source[0] ||
                    decoded_pair[1] != source[1]) {
                    all_two_byte_pairs_pass = 0;
                }
            }
        }
        check(all_two_byte_pairs_pass, "all 65536 two-byte pairs round trip");
    }

    {
        /* Raw Codec cannot reliably detect a wrong Profile; a Managed Payload
         * Envelope will provide candidate validation in a later stage. */
        static const uint8_t source[] = {0x00, 0x01, 0xFF};
        char cross_encoded[8];
        uint8_t cross_decoded[8];
        size_t cross_encoded_len = 0;
        size_t cross_decoded_len = 0;
        status = obscura64_codec_encode(source, sizeof(source), obscura64_v1_character_pool,
                                   OBSCURA64_ALPHABET_SIZE, cross_encoded,
                                   sizeof(cross_encoded), &cross_encoded_len);
        check(status == OBSCURA64_CORE_STATUS_SUCCESS, "cross-profile source encode succeeds");
        status = obscura64_codec_decode(cross_encoded, cross_encoded_len, reverse_profile,
                                   OBSCURA64_ALPHABET_SIZE, cross_decoded,
                                   sizeof(cross_decoded), &cross_decoded_len);
        check(status == OBSCURA64_CORE_STATUS_SUCCESS, "cross-profile decode can succeed");
        check(cross_decoded_len != sizeof(source) ||
                  memcmp(cross_decoded, source, sizeof(source)) != 0,
              "cross-profile decode is not mistaken for original data");
    }

    {
        static const struct { size_t input; size_t encoded; } cases[] = {
            {0, 0}, {1, 4}, {2, 4}, {3, 4}, {4, 8}, {5, 8}, {6, 8}
        };
        for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
            required = 999;
            status = obscura64_codec_encoded_size(cases[i].input, &required);
            check(status == OBSCURA64_CORE_STATUS_SUCCESS && required == cases[i].encoded,
                  "encoded size boundary");
        }
    }
    required = 999;
    status = obscura64_codec_encoded_size(SIZE_MAX, &required);
    check(status == OBSCURA64_CORE_STATUS_SIZE_OVERFLOW && required == 0,
          "encoded size detects overflow");

    check_invalid_data("a", "length 1 rejected");
    check_invalid_data("aa", "length 2 rejected");
    check_invalid_data("aaa", "length 3 rejected");
    check_invalid_data("aaaaa", "length 5 rejected");
    check_invalid_data("0000", "'0' rejected");
    check_invalid_data("Iaaa", "'I' rejected");
    check_invalid_data(" aaA", "space rejected");
    check_invalid_data("aa\na", "LF rejected");
    check_invalid_data("aa\ra", "CR rejected");
    check_invalid_data("aa\ta", "tab rejected");
    check_invalid_data("bi==AAAA", "padding before final quantum rejected");
    check_invalid_data("=xxx", "leading padding rejected");
    check_invalid_data("b=!!", "padding in second position rejected");
    check_invalid_data("bq=&", "padding in third position rejected");
    check_invalid_data("b===", "three padding characters rejected");
    check_invalid_data("bi==A===", "data after padding rejected");
    check_invalid_data("bj==", "non-canonical xx== pad bits rejected");
    check_invalid_data("bq!=", "non-canonical xxx= pad bits rejected");

    memset(output, 0x5A, sizeof(output));
    memcpy(before, output, sizeof(output));
    output_len = 0;
    status = obscura64_codec_encode(binary, sizeof(binary), obscura64_v1_character_pool,
                         OBSCURA64_ALPHABET_SIZE, (char *)output, 4, &output_len);
    check(status == OBSCURA64_CORE_STATUS_BUFFER_TOO_SMALL && output_len == 8,
          "encode reports required capacity");
    check(memcmp(output, before, sizeof(output)) == 0,
          "encode buffer unchanged when too small");

    memset(output, 0x5A, sizeof(output));
    memcpy(before, output, sizeof(output));
    output_len = 0;
    status = obscura64_codec_decode("bq!zaqE=", 8, obscura64_v1_character_pool,
                         OBSCURA64_ALPHABET_SIZE, output, 4, &output_len);
    check(status == OBSCURA64_CORE_STATUS_BUFFER_TOO_SMALL && output_len == 5,
          "decode reports required capacity");
    check(memcmp(output, before, sizeof(output)) == 0,
          "decode buffer unchanged when too small");

    memset(output, 0x5A, sizeof(output));
    memcpy(before, output, sizeof(output));
    output_len = 123;
    status = obscura64_codec_decode("bj==", 4, obscura64_v1_character_pool,
                         OBSCURA64_ALPHABET_SIZE, output, sizeof(output), &output_len);
    check(status == OBSCURA64_CORE_STATUS_INVALID_DATA && output_len == 0,
          "invalid encoded input reports invalid data");
    check(memcmp(output, before, sizeof(output)) == 0,
          "invalid encoded input leaves output unchanged");

    memset(output, 0x5A, sizeof(output));
    memcpy(before, output, sizeof(output));
    output_len = 123;
    status = obscura64_codec_encode(binary, sizeof(binary), "invalid", 7,
                         (char *)output, sizeof(output), &output_len);
    check(status == OBSCURA64_CORE_STATUS_INVALID_PROFILE && output_len == 0,
          "invalid profile rejected by encode");
    check(memcmp(output, before, sizeof(output)) == 0,
          "invalid profile leaves encode output unchanged");
    status = obscura64_codec_decode("bq!zaqE=", 8, "invalid", 7,
                         output, sizeof(output), &output_len);
    check(status == OBSCURA64_CORE_STATUS_INVALID_PROFILE && output_len == 0,
          "invalid profile rejected by decode");
    check(memcmp(output, before, sizeof(output)) == 0,
          "invalid profile leaves decode output unchanged");

    if (failures != 0) {
        fprintf(stderr, "%zu of %zu codec checks failed\n", failures, checks_run);
        return 1;
    }
    printf("codec checks passed: %zu\n", checks_run);
    return 0;
}
