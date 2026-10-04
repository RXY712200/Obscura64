#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>

#include "obscura64_internal.h"
#include "obscura64_profile_generator_internal.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

obscura64_profile_generator_status obscura64_profile_generator_random_uniform(
    uint32_t bound,
    uint32_t *value)
{
    const uint64_t range = UINT64_C(1) << 32;
    uint64_t limit;

    if (bound == 0 || value == NULL) {
        return OBSCURA64_PROFILE_GENERATOR_INVALID_ARGUMENT;
    }

    limit = (range / bound) * bound;
    for (;;) {
        uint32_t random_value;
        const NTSTATUS status = BCryptGenRandom(
            NULL,
            (PUCHAR)&random_value,
            (ULONG)sizeof(random_value),
            BCRYPT_USE_SYSTEM_PREFERRED_RNG);

        if (status != 0) {
            return OBSCURA64_PROFILE_GENERATOR_RANDOM_FAILURE;
        }
        if ((uint64_t)random_value < limit) {
            *value = random_value % bound;
            return OBSCURA64_PROFILE_GENERATOR_SUCCESS;
        }
    }
}

obscura64_profile_generator_status obscura64_profile_generator_generate(char profile[65])
{
    int i;

    if (profile == NULL) {
        return OBSCURA64_PROFILE_GENERATOR_INVALID_ARGUMENT;
    }

    memcpy(profile, obscura64_v1_character_pool, OBSCURA64_ALPHABET_SIZE);
    for (i = OBSCURA64_ALPHABET_SIZE - 1; i > 0; --i) {
        uint32_t selected;
        char temporary;
        const obscura64_profile_generator_status status =
            obscura64_profile_generator_random_uniform((uint32_t)i + 1, &selected);

        if (status != OBSCURA64_PROFILE_GENERATOR_SUCCESS) {
            return status;
        }
        temporary = profile[i];
        profile[i] = profile[selected];
        profile[selected] = temporary;
    }
    profile[OBSCURA64_ALPHABET_SIZE] = '\0';

    if (!obscura64_profile_is_valid(profile, OBSCURA64_ALPHABET_SIZE)) {
        return OBSCURA64_PROFILE_GENERATOR_VALIDATION_FAILURE;
    }
    return OBSCURA64_PROFILE_GENERATOR_SUCCESS;
}

obscura64_profile_generator_status obscura64_profile_generator_generate_unique_set(
    uint8_t *profiles,
    size_t count,
    obscura64_profile_generator_stats *stats)
{
    size_t accepted;

    if (profiles == NULL || stats == NULL || count == 0 ||
        count > SIZE_MAX / OBSCURA64_PROFILE_BYTES ||
        count > SIZE_MAX / OBSCURA64_PROFILE_GENERATOR_RETRY_LIMIT) {
        return OBSCURA64_PROFILE_GENERATOR_INVALID_ARGUMENT;
    }
    memset(stats, 0, sizeof(*stats));

    for (accepted = 0; accepted < count; ++accepted) {
        size_t attempt;
        int inserted = 0;

        for (attempt = 0; attempt < OBSCURA64_PROFILE_GENERATOR_RETRY_LIMIT; ++attempt) {
            char candidate[OBSCURA64_PROFILE_BYTES + 1];
            size_t previous;
            int duplicate = 0;
            obscura64_profile_generator_status status;

            ++stats->generation_attempts;
            status = obscura64_profile_generator_generate(candidate);
            if (status != OBSCURA64_PROFILE_GENERATOR_SUCCESS) {
                stats->accepted_count = accepted;
                return status;
            }
            if (!obscura64_profile_is_valid(candidate, OBSCURA64_PROFILE_BYTES)) {
                stats->accepted_count = accepted;
                return OBSCURA64_PROFILE_GENERATOR_VALIDATION_FAILURE;
            }

            for (previous = 0; previous < accepted; ++previous) {
                if (memcmp(profiles + previous * OBSCURA64_PROFILE_BYTES,
                           candidate, OBSCURA64_PROFILE_BYTES) == 0) {
                    duplicate = 1;
                    ++stats->collisions;
                    break;
                }
            }
            if (duplicate) {
                continue;
            }

            memcpy(profiles + accepted * OBSCURA64_PROFILE_BYTES,
                   candidate, OBSCURA64_PROFILE_BYTES);
            inserted = 1;
            break;
        }

        if (!inserted) {
            stats->accepted_count = accepted;
            return OBSCURA64_PROFILE_GENERATOR_RETRIES_EXHAUSTED;
        }
        stats->accepted_count = accepted + 1;
    }
    return OBSCURA64_PROFILE_GENERATOR_SUCCESS;
}

obscura64_profile_generator_status obscura64_profile_generator_validate_unique_set(
    const uint8_t *profiles,
    size_t count,
    size_t *pair_comparisons)
{
    size_t i;
    size_t pairs = 0;
    size_t factor_a;
    size_t factor_b;

    if (pair_comparisons == NULL) {
        return OBSCURA64_PROFILE_GENERATOR_INVALID_ARGUMENT;
    }
    *pair_comparisons = 0;
    if (profiles == NULL || count == 0 ||
        count > SIZE_MAX / OBSCURA64_PROFILE_BYTES) {
        return OBSCURA64_PROFILE_GENERATOR_INVALID_ARGUMENT;
    }

    for (i = 0; i < count; ++i) {
        const uint8_t *profile = profiles + i * OBSCURA64_PROFILE_BYTES;
        if (!obscura64_profile_is_valid((const char *)profile, OBSCURA64_PROFILE_BYTES) ||
            memchr(profile, '=', OBSCURA64_PROFILE_BYTES) != NULL) {
            return OBSCURA64_PROFILE_GENERATOR_VALIDATION_FAILURE;
        }
    }

    if (count > 1) {
        factor_a = count;
        factor_b = count - 1;
        if ((factor_a & 1U) == 0) {
            factor_a /= 2;
        } else {
            factor_b /= 2;
        }
        if (factor_a > SIZE_MAX / factor_b) {
            return OBSCURA64_PROFILE_GENERATOR_INVALID_ARGUMENT;
        }
    }

    for (i = 0; i < count; ++i) {
        size_t j;
        for (j = i + 1; j < count; ++j) {
            ++pairs;
            if (memcmp(profiles + i * OBSCURA64_PROFILE_BYTES,
                       profiles + j * OBSCURA64_PROFILE_BYTES,
                       OBSCURA64_PROFILE_BYTES) == 0) {
                *pair_comparisons = pairs;
                return OBSCURA64_PROFILE_GENERATOR_DUPLICATE;
            }
        }
    }
    *pair_comparisons = pairs;
    return OBSCURA64_PROFILE_GENERATOR_SUCCESS;
}

static int read_exact(FILE *file, void *buffer, size_t length)
{
    return fread(buffer, 1, length, file) == length;
}

#ifndef OBSCURA64_PROFILE_GENERATOR_NO_MAIN
static int path_exists(const char *path)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return 0;
    }
    fclose(file);
    return 1;
}
#endif

static obscura64_profile_generator_status load_candidate_profiles(
    const char *path,
    uint8_t **profiles_out,
    obscura64_profile_generator_stats *stats)
{
    static const char magic[] = "RB64_PROFILE_LIBRARY_CANDIDATE_V1\n";
    static const char count_line[] = "COUNT 4096\n";
    FILE *file;
    uint8_t *profiles;
    size_t i;
    int trailing;
    obscura64_profile_generator_status status;

    if (path == NULL || profiles_out == NULL || stats == NULL) {
        return OBSCURA64_PROFILE_GENERATOR_INVALID_ARGUMENT;
    }
    *profiles_out = NULL;
    memset(stats, 0, sizeof(*stats));
    file = fopen(path, "rb");
    if (file == NULL) {
        return OBSCURA64_PROFILE_GENERATOR_IO_FAILURE;
    }
    profiles = (uint8_t *)malloc((size_t)OBSCURA64_PROFILE_LIBRARY_COUNT *
                                 OBSCURA64_PROFILE_BYTES);
    if (profiles == NULL) {
        fclose(file);
        return OBSCURA64_PROFILE_GENERATOR_OUT_OF_MEMORY;
    }

    {
        char header[sizeof(magic) - 1];
        char count_header[sizeof(count_line) - 1];
        if (!read_exact(file, header, sizeof(header)) ||
            memcmp(header, magic, sizeof(header)) != 0 ||
            !read_exact(file, count_header, sizeof(count_header)) ||
            memcmp(count_header, count_line, sizeof(count_header)) != 0) {
            status = OBSCURA64_PROFILE_GENERATOR_INVALID_CANDIDATE;
            goto done;
        }
    }

    for (i = 0; i < OBSCURA64_PROFILE_LIBRARY_COUNT; ++i) {
        char entry[70];
        char expected_id[6];
        int id_length = snprintf(expected_id, sizeof(expected_id), "%04u ",
                                 (unsigned int)i);
        if (id_length != 5 || !read_exact(file, entry, sizeof(entry)) ||
            memcmp(entry, expected_id, 5) != 0 || entry[69] != '\n') {
            status = OBSCURA64_PROFILE_GENERATOR_INVALID_CANDIDATE;
            goto done;
        }
        memcpy(profiles + i * OBSCURA64_PROFILE_BYTES, entry + 5, OBSCURA64_PROFILE_BYTES);
        if (!obscura64_profile_is_valid((const char *)(entry + 5), OBSCURA64_PROFILE_BYTES)) {
            status = OBSCURA64_PROFILE_GENERATOR_INVALID_CANDIDATE;
            goto done;
        }
    }

    trailing = fgetc(file);
    if (trailing != EOF || ferror(file)) {
        status = OBSCURA64_PROFILE_GENERATOR_INVALID_CANDIDATE;
        goto done;
    }
    if (fclose(file) != 0) {
        file = NULL;
        status = OBSCURA64_PROFILE_GENERATOR_IO_FAILURE;
        goto done;
    }
    file = NULL;

    status = obscura64_profile_generator_validate_unique_set(
        profiles, OBSCURA64_PROFILE_LIBRARY_COUNT, &stats->pair_comparisons);
    if (status == OBSCURA64_PROFILE_GENERATOR_SUCCESS) {
        stats->accepted_count = OBSCURA64_PROFILE_LIBRARY_COUNT;
        *profiles_out = profiles;
        profiles = NULL;
    }

done:
    if (file != NULL) {
        fclose(file);
    }
    free(profiles);
    return status;
}

obscura64_profile_generator_status obscura64_profile_generator_validate_candidate(
    const char *path,
    obscura64_profile_generator_stats *stats)
{
    uint8_t *profiles = NULL;
    obscura64_profile_generator_status status =
        load_candidate_profiles(path, &profiles, stats);
    free(profiles);
    return status;
}

obscura64_profile_generator_status obscura64_profile_generator_write_candidate(
    const char *path,
    const uint8_t *profiles,
    size_t count)
{
    static const char magic[] = "RB64_PROFILE_LIBRARY_CANDIDATE_V1\n";
    static const char count_line[] = "COUNT 4096\n";
    size_t path_len;
    char *temporary_path;
    FILE *file;
    size_t i;
    int failed = 0;
    obscura64_profile_generator_status status;
    obscura64_profile_generator_stats validation_stats;

    if (path == NULL || profiles == NULL || count != OBSCURA64_PROFILE_LIBRARY_COUNT) {
        return OBSCURA64_PROFILE_GENERATOR_INVALID_ARGUMENT;
    }
    path_len = strlen(path);
    if (path_len > SIZE_MAX - 5) {
        return OBSCURA64_PROFILE_GENERATOR_INVALID_ARGUMENT;
    }
    temporary_path = (char *)malloc(path_len + 5);
    if (temporary_path == NULL) {
        return OBSCURA64_PROFILE_GENERATOR_OUT_OF_MEMORY;
    }
    memcpy(temporary_path, path, path_len);
    memcpy(temporary_path + path_len, ".tmp", 5);

    file = fopen(temporary_path, "wb");
    if (file == NULL) {
        free(temporary_path);
        return OBSCURA64_PROFILE_GENERATOR_IO_FAILURE;
    }
    if (fwrite(magic, 1, sizeof(magic) - 1, file) != sizeof(magic) - 1 ||
        fwrite(count_line, 1, sizeof(count_line) - 1, file) != sizeof(count_line) - 1) {
        failed = 1;
    }
    for (i = 0; !failed && i < count; ++i) {
        char id[6];
        const int id_length = snprintf(id, sizeof(id), "%04u ", (unsigned int)i);
        const uint8_t *profile = profiles + i * OBSCURA64_PROFILE_BYTES;
        if (id_length != 5 || fwrite(id, 1, 5, file) != 5 ||
            fwrite(profile, 1, OBSCURA64_PROFILE_BYTES, file) != OBSCURA64_PROFILE_BYTES ||
            fputc('\n', file) == EOF) {
            failed = 1;
        }
    }
    if (fclose(file) != 0) {
        failed = 1;
    }
    if (failed) {
        remove(temporary_path);
        free(temporary_path);
        return OBSCURA64_PROFILE_GENERATOR_IO_FAILURE;
    }

    if (rename(temporary_path, path) != 0) {
        remove(temporary_path);
        free(temporary_path);
        return OBSCURA64_PROFILE_GENERATOR_IO_FAILURE;
    }
    free(temporary_path);

    status = obscura64_profile_generator_validate_candidate(path, &validation_stats);
    if (status != OBSCURA64_PROFILE_GENERATOR_SUCCESS) {
        remove(path);
    }
    return status;
}

#ifndef OBSCURA64_PROFILE_GENERATOR_NO_MAIN
static obscura64_profile_generator_status sha256_bytes(
    const uint8_t *bytes,
    size_t length,
    uint8_t digest[32])
{
    BCRYPT_ALG_HANDLE algorithm = NULL;
    BCRYPT_HASH_HANDLE hash = NULL;
    PUCHAR hash_object = NULL;
    ULONG object_length = 0;
    ULONG returned = 0;
    NTSTATUS status;

    status = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM,
                                         NULL, 0);
    if (!BCRYPT_SUCCESS(status)) {
        return OBSCURA64_PROFILE_GENERATOR_IO_FAILURE;
    }
    status = BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
                               (PUCHAR)&object_length, sizeof(object_length),
                               &returned, 0);
    if (!BCRYPT_SUCCESS(status) || object_length == 0) {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return OBSCURA64_PROFILE_GENERATOR_IO_FAILURE;
    }
    hash_object = (PUCHAR)malloc(object_length);
    if (hash_object == NULL) {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return OBSCURA64_PROFILE_GENERATOR_OUT_OF_MEMORY;
    }
    status = BCryptCreateHash(algorithm, &hash, hash_object, object_length,
                              NULL, 0, 0);
    if (BCRYPT_SUCCESS(status)) {
        status = BCryptHashData(hash, (PUCHAR)bytes, (ULONG)length, 0);
    }
    if (BCRYPT_SUCCESS(status)) {
        status = BCryptFinishHash(hash, digest, 32, 0);
    }
    if (hash != NULL) {
        BCryptDestroyHash(hash);
    }
    free(hash_object);
    BCryptCloseAlgorithmProvider(algorithm, 0);
    return BCRYPT_SUCCESS(status) ? OBSCURA64_PROFILE_GENERATOR_SUCCESS
                                  : OBSCURA64_PROFILE_GENERATOR_IO_FAILURE;
}

static void digest_to_hex(const uint8_t digest[32], char hex[65])
{
    static const char digits[] = "0123456789abcdef";
    size_t i;
    for (i = 0; i < 32; ++i) {
        hex[i * 2] = digits[digest[i] >> 4];
        hex[i * 2 + 1] = digits[digest[i] & 0x0F];
    }
    hex[64] = '\0';
}

static char *join_path(const char *directory, const char *name)
{
    size_t directory_length;
    size_t name_length;
    int separator;
    char *path;

    if (directory == NULL || name == NULL) {
        return NULL;
    }
    directory_length = strlen(directory);
    name_length = strlen(name);
    separator = directory_length != 0 &&
                directory[directory_length - 1] != '/' &&
                directory[directory_length - 1] != '\\';
    if (directory_length > SIZE_MAX - name_length - (size_t)separator - 1) {
        return NULL;
    }
    path = (char *)malloc(directory_length + name_length + (size_t)separator + 1);
    if (path == NULL) {
        return NULL;
    }
    memcpy(path, directory, directory_length);
    if (separator) {
        path[directory_length++] = '\\';
    }
    memcpy(path + directory_length, name, name_length + 1);
    return path;
}

static int emit_embedded_source(FILE *file, const uint8_t *profiles)
{
    size_t i;
    if (fprintf(file,
                "#include \"obscura64_profiles_v1.h\"\n\n"
                "const unsigned char obscura64_profiles_v1[OBSCURA64_PROFILE_COUNT][OBSCURA64_PROFILE_SIZE] = {\n") < 0) {
        return 0;
    }
    for (i = 0; i < OBSCURA64_PROFILE_LIBRARY_COUNT; ++i) {
        size_t j;
        const uint8_t *profile = profiles + i * OBSCURA64_PROFILE_BYTES;
        if (fputs("    {", file) == EOF) {
            return 0;
        }
        for (j = 0; j < OBSCURA64_PROFILE_BYTES; ++j) {
            if (fprintf(file, "%s0x%02X", j == 0 ? "" : ", ", profile[j]) < 0) {
                return 0;
            }
        }
        if (fprintf(file, "}%s\n", i + 1 == OBSCURA64_PROFILE_LIBRARY_COUNT ? "" : ",") < 0) {
            return 0;
        }
    }
    return fprintf(file,
                   "};\n\n"
                   "_Static_assert(sizeof(obscura64_profiles_v1) == OBSCURA64_PROFILE_COUNT * OBSCURA64_PROFILE_SIZE,\n"
                   "               \"Profile Library V1 payload size mismatch\");\n") >= 0;
}

static int emit_embedded_header(FILE *file, const char *hex)
{
    return fprintf(file,
                   "#ifndef OBSCURA64_PROFILES_V1_H\n"
                   "#define OBSCURA64_PROFILES_V1_H\n\n"
                   "#include \"obscura64_internal.h\"\n\n"
                   "#define OBSCURA64_PROFILE_COUNT 4096U\n"
                    "#define OBSCURA64_PROFILE_LIBRARY_V1_SHA256 \"%s\"\n\n"
                   "extern const unsigned char obscura64_profiles_v1[OBSCURA64_PROFILE_COUNT][OBSCURA64_PROFILE_SIZE];\n\n"
                   "#endif /* OBSCURA64_PROFILES_V1_H */\n", hex) >= 0;
}

static int emit_library_document(FILE *file, const char *hex)
{
    return fprintf(file,
                    "# Obscura64 Profile Library V1\n\n"
                   "- Status: Frozen\n"
                   "- Profile count: 4096\n"
                   "- Profile size: 64 bytes\n"
                   "- Canonical payload size: 262144 bytes\n"
                   "- Profile IDs: 0~4095\n"
                   "- Canonical representation: concatenation of Profile 0 through Profile 4095\n"
                   "- Canonical SHA-256: `%s`\n"
                   "- Source candidate: `generated/obscura64_profiles_v1_candidate.txt`\n"
                   "- Canonical binary: `generated/obscura64_profiles_v1.bin`\n"
                   "- The candidate's first-line magic `RB64_PROFILE_LIBRARY_CANDIDATE_V1` is retained byte-for-byte from the pre-rename Stage 2 freeze artifact format. It is historical artifact metadata and does not denote the current project name, Obscura64.\n"
                   "- Runtime representation: embedded internal C array\n"
                   "- The Profile Library is public and non-secret.\n"
                   "- SHA-256 identifies and verifies the library version/integrity; it does not provide encryption or security strength.\n"
                   "- V1 Profile order and content must never change after freeze.\n", hex) >= 0;
}

static obscura64_profile_generator_status write_generated_file(
    const char *path,
    int (*emit)(FILE *, const void *),
    const void *context)
{
    FILE *file = fopen(path, "wb");
    int ok;
    if (file == NULL) {
        return OBSCURA64_PROFILE_GENERATOR_IO_FAILURE;
    }
    ok = emit(file, context);
    if (fclose(file) != 0) {
        ok = 0;
    }
    return ok ? OBSCURA64_PROFILE_GENERATOR_SUCCESS : OBSCURA64_PROFILE_GENERATOR_IO_FAILURE;
}

static int emit_source_adapter(FILE *file, const void *context)
{
    return emit_embedded_source(file, (const uint8_t *)context);
}

static int emit_header_adapter(FILE *file, const void *context)
{
    return emit_embedded_header(file, (const char *)context);
}

static int emit_document_adapter(FILE *file, const void *context)
{
    return emit_library_document(file, (const char *)context);
}

static int files_equal(const char *first_path, const char *second_path)
{
    unsigned char first[4096];
    unsigned char second[4096];
    FILE *first_file = fopen(first_path, "rb");
    FILE *second_file = fopen(second_path, "rb");
    int equal = 1;

    if (first_file == NULL || second_file == NULL) {
        equal = 0;
        goto done;
    }
    for (;;) {
        const size_t first_count = fread(first, 1, sizeof(first), first_file);
        const size_t second_count = fread(second, 1, sizeof(second), second_file);
        if (first_count != second_count || memcmp(first, second, first_count) != 0) {
            equal = 0;
            break;
        }
        if (first_count == 0) {
            if (ferror(first_file) || ferror(second_file)) {
                equal = 0;
            }
            break;
        }
    }

done:
    if (first_file != NULL) {
        fclose(first_file);
    }
    if (second_file != NULL) {
        fclose(second_file);
    }
    return equal;
}

static obscura64_profile_generator_status read_canonical_file(
    const char *path,
    uint8_t *bytes)
{
    FILE *file = fopen(path, "rb");
    int trailing;
    int close_status;
    if (file == NULL) {
        return OBSCURA64_PROFILE_GENERATOR_IO_FAILURE;
    }
    if (!read_exact(file, bytes, (size_t)OBSCURA64_PROFILE_LIBRARY_COUNT * OBSCURA64_PROFILE_BYTES)) {
        fclose(file);
        return OBSCURA64_PROFILE_GENERATOR_INVALID_CANDIDATE;
    }
    trailing = fgetc(file);
    if (trailing != EOF || ferror(file)) {
        fclose(file);
        return OBSCURA64_PROFILE_GENERATOR_INVALID_CANDIDATE;
    }
    close_status = fclose(file);
    return close_status == 0 ? OBSCURA64_PROFILE_GENERATOR_SUCCESS
                             : OBSCURA64_PROFILE_GENERATOR_IO_FAILURE;
}

static obscura64_profile_generator_status verify_frozen_files(
    const char *candidate,
    const char *root,
    char hex_out[65],
    obscura64_profile_generator_stats *stats)
{
    uint8_t *profiles = NULL;
    uint8_t *canonical = NULL;
    uint8_t digest[32];
    char *bin_path = NULL;
    char *source_path = NULL;
    char *header_path = NULL;
    char *doc_path = NULL;
    char *expected_source_path = NULL;
    char *expected_header_path = NULL;
    char *expected_doc_path = NULL;
    size_t pair_count = 0;
    obscura64_profile_generator_status status;

    status = load_candidate_profiles(candidate, &profiles, stats);
    if (status != OBSCURA64_PROFILE_GENERATOR_SUCCESS) {
        goto done;
    }
    canonical = (uint8_t *)malloc((size_t)OBSCURA64_PROFILE_LIBRARY_COUNT * OBSCURA64_PROFILE_BYTES);
    if (canonical == NULL) {
        status = OBSCURA64_PROFILE_GENERATOR_OUT_OF_MEMORY;
        goto done;
    }
    bin_path = join_path(root, "generated\\obscura64_profiles_v1.bin");
    source_path = join_path(root, "src\\obscura64_profiles_v1.c");
    header_path = join_path(root, "src\\obscura64_profiles_v1.h");
    doc_path = join_path(root, "docs\\PROFILE_LIBRARY_V1.md");
    expected_source_path = join_path(root, "generated\\obscura64_profiles_v1.verify-source.tmp");
    expected_header_path = join_path(root, "generated\\obscura64_profiles_v1.verify-header.tmp");
    expected_doc_path = join_path(root, "generated\\obscura64_profiles_v1.verify-doc.tmp");
    if (bin_path == NULL || source_path == NULL || header_path == NULL || doc_path == NULL ||
        expected_source_path == NULL || expected_header_path == NULL || expected_doc_path == NULL) {
        status = OBSCURA64_PROFILE_GENERATOR_OUT_OF_MEMORY;
        goto done;
    }
    status = read_canonical_file(bin_path, canonical);
    if (status != OBSCURA64_PROFILE_GENERATOR_SUCCESS) {
        goto done;
    }
    if (memcmp(canonical, profiles,
               (size_t)OBSCURA64_PROFILE_LIBRARY_COUNT * OBSCURA64_PROFILE_BYTES) != 0) {
        fputs("verify mismatch: canonical binary differs from candidate\n", stderr);
        status = OBSCURA64_PROFILE_GENERATOR_INVALID_CANDIDATE;
        goto done;
    }
    status = sha256_bytes(canonical,
                          (size_t)OBSCURA64_PROFILE_LIBRARY_COUNT * OBSCURA64_PROFILE_BYTES,
                          digest);
    if (status != OBSCURA64_PROFILE_GENERATOR_SUCCESS) {
        goto done;
    }
    digest_to_hex(digest, hex_out);

    status = write_generated_file(expected_source_path, emit_source_adapter, profiles);
    if (status != OBSCURA64_PROFILE_GENERATOR_SUCCESS) {
        goto done;
    }
    status = write_generated_file(expected_header_path, emit_header_adapter, hex_out);
    if (status != OBSCURA64_PROFILE_GENERATOR_SUCCESS) {
        goto done;
    }
    status = write_generated_file(expected_doc_path, emit_document_adapter, hex_out);
    if (status != OBSCURA64_PROFILE_GENERATOR_SUCCESS) {
        goto done;
    }
    if (!files_equal(source_path, expected_source_path)) {
        fputs("verify mismatch: embedded C source differs from deterministic candidate output\n",
              stderr);
        status = OBSCURA64_PROFILE_GENERATOR_INVALID_CANDIDATE;
        goto done;
    }
    if (!files_equal(header_path, expected_header_path)) {
        fputs("verify mismatch: embedded header differs from canonical hash metadata\n",
              stderr);
        status = OBSCURA64_PROFILE_GENERATOR_INVALID_CANDIDATE;
        goto done;
    }
    if (!files_equal(doc_path, expected_doc_path)) {
        fputs("verify mismatch: Profile Library document differs from frozen metadata\n",
              stderr);
        status = OBSCURA64_PROFILE_GENERATOR_INVALID_CANDIDATE;
        goto done;
    }
    status = obscura64_profile_generator_validate_unique_set(
        profiles, OBSCURA64_PROFILE_LIBRARY_COUNT, &pair_count);
    if (status == OBSCURA64_PROFILE_GENERATOR_SUCCESS) {
        stats->accepted_count = OBSCURA64_PROFILE_LIBRARY_COUNT;
        stats->pair_comparisons = pair_count;
    }

done:
    if (expected_source_path != NULL) remove(expected_source_path);
    if (expected_header_path != NULL) remove(expected_header_path);
    if (expected_doc_path != NULL) remove(expected_doc_path);
    free(profiles);
    free(canonical);
    free(bin_path);
    free(source_path);
    free(header_path);
    free(doc_path);
    free(expected_source_path);
    free(expected_header_path);
    free(expected_doc_path);
    return status;
}

static obscura64_profile_generator_status freeze_v1(
    const char *candidate,
    const char *root,
    char hex_out[65],
    obscura64_profile_generator_stats *stats)
{
    uint8_t *profiles = NULL;
    uint8_t digest[32];
    char hex[65];
    char *bin_path = NULL;
    char *source_path = NULL;
    char *header_path = NULL;
    char *doc_path = NULL;
    char *bin_temp = NULL;
    char *source_temp = NULL;
    char *header_temp = NULL;
    char *doc_temp = NULL;
    FILE *file = NULL;
    obscura64_profile_generator_status status;
    int renamed_bin = 0;
    int renamed_source = 0;
    int renamed_header = 0;
    int renamed_doc = 0;
    size_t bytes_length = (size_t)OBSCURA64_PROFILE_LIBRARY_COUNT * OBSCURA64_PROFILE_BYTES;
    size_t pair_count = 0;

    status = load_candidate_profiles(candidate, &profiles, stats);
    if (status != OBSCURA64_PROFILE_GENERATOR_SUCCESS) {
        goto done;
    }
    status = obscura64_profile_generator_validate_unique_set(
        profiles, OBSCURA64_PROFILE_LIBRARY_COUNT, &pair_count);
    if (status != OBSCURA64_PROFILE_GENERATOR_SUCCESS) {
        goto done;
    }
    stats->pair_comparisons = pair_count;

    bin_path = join_path(root, "generated\\obscura64_profiles_v1.bin");
    source_path = join_path(root, "src\\obscura64_profiles_v1.c");
    header_path = join_path(root, "src\\obscura64_profiles_v1.h");
    doc_path = join_path(root, "docs\\PROFILE_LIBRARY_V1.md");
    if (bin_path == NULL || source_path == NULL || header_path == NULL || doc_path == NULL) {
        status = OBSCURA64_PROFILE_GENERATOR_OUT_OF_MEMORY;
        goto done;
    }
    if (path_exists(bin_path) || path_exists(source_path) ||
        path_exists(header_path) || path_exists(doc_path)) {
        status = OBSCURA64_PROFILE_GENERATOR_IO_FAILURE;
        goto done;
    }

    bin_temp = (char *)malloc(strlen(bin_path) + 5);
    source_temp = (char *)malloc(strlen(source_path) + 5);
    header_temp = (char *)malloc(strlen(header_path) + 5);
    doc_temp = (char *)malloc(strlen(doc_path) + 5);
    if (bin_temp == NULL || source_temp == NULL || header_temp == NULL || doc_temp == NULL) {
        status = OBSCURA64_PROFILE_GENERATOR_OUT_OF_MEMORY;
        goto done;
    }
    sprintf(bin_temp, "%s.tmp", bin_path);
    sprintf(source_temp, "%s.tmp", source_path);
    sprintf(header_temp, "%s.tmp", header_path);
    sprintf(doc_temp, "%s.tmp", doc_path);

    file = fopen(bin_temp, "wb");
    if (file == NULL) {
        status = OBSCURA64_PROFILE_GENERATOR_IO_FAILURE;
        goto done;
    }
    if (fwrite(profiles, 1, bytes_length, file) != bytes_length) {
        fclose(file);
        file = NULL;
        status = OBSCURA64_PROFILE_GENERATOR_IO_FAILURE;
        goto done;
    }
    if (fclose(file) != 0) {
        file = NULL;
        status = OBSCURA64_PROFILE_GENERATOR_IO_FAILURE;
        goto done;
    }
    file = NULL;

    {
        uint8_t *readback = (uint8_t *)malloc(bytes_length);
        if (readback == NULL) {
            status = OBSCURA64_PROFILE_GENERATOR_OUT_OF_MEMORY;
            goto done;
        }
        status = read_canonical_file(bin_temp, readback);
        if (status == OBSCURA64_PROFILE_GENERATOR_SUCCESS &&
            memcmp(readback, profiles, bytes_length) != 0) {
            status = OBSCURA64_PROFILE_GENERATOR_INVALID_CANDIDATE;
        }
        if (status == OBSCURA64_PROFILE_GENERATOR_SUCCESS) {
            status = sha256_bytes(readback, bytes_length, digest);
        }
        free(readback);
        if (status != OBSCURA64_PROFILE_GENERATOR_SUCCESS) {
            goto done;
        }
    }
    digest_to_hex(digest, hex);

    status = write_generated_file(source_temp, emit_source_adapter, profiles);
    if (status == OBSCURA64_PROFILE_GENERATOR_SUCCESS) {
        status = write_generated_file(header_temp, emit_header_adapter, hex);
    }
    if (status == OBSCURA64_PROFILE_GENERATOR_SUCCESS) {
        status = write_generated_file(doc_temp, emit_document_adapter, hex);
    }
    if (status != OBSCURA64_PROFILE_GENERATOR_SUCCESS) {
        goto done;
    }

    if (rename(bin_temp, bin_path) != 0) goto rename_failed;
    renamed_bin = 1;
    if (rename(source_temp, source_path) != 0) goto rename_failed;
    renamed_source = 1;
    if (rename(header_temp, header_path) != 0) goto rename_failed;
    renamed_header = 1;
    if (rename(doc_temp, doc_path) != 0) goto rename_failed;
    renamed_doc = 1;

    status = verify_frozen_files(candidate, root, hex_out, stats);
    if (status != OBSCURA64_PROFILE_GENERATOR_SUCCESS) {
        goto rename_failed;
    }
    memcpy(hex_out, hex, sizeof(hex));
    goto done;

rename_failed:
    status = OBSCURA64_PROFILE_GENERATOR_IO_FAILURE;
    if (renamed_bin) remove(bin_path);
    if (renamed_source) remove(source_path);
    if (renamed_header) remove(header_path);
    if (renamed_doc) remove(doc_path);

done:
    if (file != NULL) fclose(file);
    if (bin_temp != NULL) remove(bin_temp);
    if (source_temp != NULL) remove(source_temp);
    if (header_temp != NULL) remove(header_temp);
    if (doc_temp != NULL) remove(doc_temp);
    free(profiles);
    free(bin_path);
    free(source_path);
    free(header_path);
    free(doc_path);
    free(bin_temp);
    free(source_temp);
    free(header_temp);
    free(doc_temp);
    return status;
}

static int parse_count(int argc, char **argv, unsigned int *count)
{
    char *end;
    unsigned long parsed;

    if (argc == 1) {
        *count = 1;
        return 1;
    }
    if (argc != 3 || strcmp(argv[1], "--count") != 0 || argv[2][0] == '\0') {
        return 0;
    }

    parsed = strtoul(argv[2], &end, 10);
    if (*end != '\0' || parsed == 0 || parsed > 1024) {
        return 0;
    }
    *count = (unsigned int)parsed;
    return 1;
}

int main(int argc, char **argv)
{
    unsigned int count;
    unsigned int i;

    if (argc == 4 && strcmp(argv[1], "--freeze-v1") == 0) {
        char hex[65];
        obscura64_profile_generator_stats stats;
        const obscura64_profile_generator_status status =
            freeze_v1(argv[2], argv[3], hex, &stats);
        if (status != OBSCURA64_PROFILE_GENERATOR_SUCCESS) {
            fprintf(stderr, "Profile Library V1 freeze failed (status %d)\n",
                    (int)status);
            return 10;
        }
        fprintf(stderr,
                "frozen profiles=%zu pair_comparisons=%zu raw_bytes=%zu sha256=%s\n",
                stats.accepted_count, stats.pair_comparisons,
                stats.accepted_count * OBSCURA64_PROFILE_BYTES, hex);
        return 0;
    }

    if (argc == 4 && strcmp(argv[1], "--verify-frozen-v1") == 0) {
        char hex[65];
        obscura64_profile_generator_stats stats;
        const obscura64_profile_generator_status status =
            verify_frozen_files(argv[2], argv[3], hex, &stats);
        if (status != OBSCURA64_PROFILE_GENERATOR_SUCCESS) {
            fprintf(stderr, "frozen Profile Library verification failed (status %d)\n",
                    (int)status);
            return 11;
        }
        fprintf(stderr,
                "verified profiles=%zu pair_comparisons=%zu raw_bytes=%zu sha256=%s\n",
                stats.accepted_count, stats.pair_comparisons,
                stats.accepted_count * OBSCURA64_PROFILE_BYTES, hex);
        return 0;
    }

    if (argc == 3 && strcmp(argv[1], "--validate-candidate") == 0) {
        obscura64_profile_generator_stats stats;
        const obscura64_profile_generator_status status =
            obscura64_profile_generator_validate_candidate(argv[2], &stats);
        if (status != OBSCURA64_PROFILE_GENERATOR_SUCCESS) {
            fprintf(stderr, "candidate validation failed (status %d)\n", (int)status);
            return 6;
        }
        fprintf(stderr, "validated profiles=%zu pair_comparisons=%zu raw_bytes=%zu\n",
                stats.accepted_count, stats.pair_comparisons,
                stats.accepted_count * OBSCURA64_PROFILE_BYTES);
        return 0;
    }

    if (argc == 3 && strcmp(argv[1], "--generate-candidate") == 0) {
        const size_t count_fixed = OBSCURA64_PROFILE_LIBRARY_COUNT;
        uint8_t *profiles = (uint8_t *)malloc(count_fixed * OBSCURA64_PROFILE_BYTES);
        obscura64_profile_generator_stats stats;
        size_t pair_comparisons = 0;
        obscura64_profile_generator_status status;

        if (profiles == NULL) {
            fputs("candidate allocation failed\n", stderr);
            return 7;
        }
        status = obscura64_profile_generator_generate_unique_set(profiles, count_fixed, &stats);
        if (status != OBSCURA64_PROFILE_GENERATOR_SUCCESS) {
            fprintf(stderr, "candidate generation failed (status %d)\n", (int)status);
            free(profiles);
            return 7;
        }
        status = obscura64_profile_generator_validate_unique_set(
            profiles, count_fixed, &pair_comparisons);
        if (status != OBSCURA64_PROFILE_GENERATOR_SUCCESS) {
            fprintf(stderr, "in-memory candidate validation failed (status %d)\n",
                    (int)status);
            free(profiles);
            return 8;
        }
        stats.pair_comparisons = pair_comparisons;
        status = obscura64_profile_generator_write_candidate(argv[2], profiles, count_fixed);
        free(profiles);
        if (status != OBSCURA64_PROFILE_GENERATOR_SUCCESS) {
            fprintf(stderr, "candidate write/readback validation failed (status %d)\n",
                    (int)status);
            return 9;
        }
        fprintf(stderr,
                "generated profiles=%zu attempts=%zu collisions=%zu pair_comparisons=%zu raw_bytes=%zu\n",
                stats.accepted_count, stats.generation_attempts, stats.collisions,
                stats.pair_comparisons, stats.accepted_count * OBSCURA64_PROFILE_BYTES);
        return 0;
    }

    if (!parse_count(argc, argv, &count)) {
        fputs("invalid arguments (usage: obscura64_profile_generator [--count N], 1 <= N <= 1024)\n",
              stderr);
        return 2;
    }

    for (i = 0; i < count; ++i) {
        char profile[OBSCURA64_ALPHABET_SIZE + 1];
        const obscura64_profile_generator_status status =
            obscura64_profile_generator_generate(profile);

        if (status == OBSCURA64_PROFILE_GENERATOR_RANDOM_FAILURE) {
            fputs("BCryptGenRandom failed\n", stderr);
            return 3;
        }
        if (status == OBSCURA64_PROFILE_GENERATOR_VALIDATION_FAILURE) {
            fputs("generated Profile failed validation\n", stderr);
            return 4;
        }
        if (status != OBSCURA64_PROFILE_GENERATOR_SUCCESS) {
            fputs("Profile generation failed\n", stderr);
            return 4;
        }
        if (printf("%04u %s\n", i, profile) < 0) {
            fputs("stdout write failed\n", stderr);
            return 5;
        }
    }

    if (fflush(stdout) == EOF || ferror(stdout)) {
        fputs("stdout write failed\n", stderr);
        return 5;
    }
    return 0;
}
#endif
