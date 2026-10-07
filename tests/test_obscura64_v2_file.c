#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "obscura64.h"
#include "obscura64_internal.h"
#include "obscura64_profiles_v1.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { \
    fprintf(stderr, "FAIL line %d\n", __LINE__); return 1; \
} } while (0)

static obscura64_status identity_size(void *user, const char profile[64],
    size_t input_len, size_t *out)
{
    (void)user; (void)profile; *out = input_len; return OBSCURA64_OK;
}
static obscura64_status identity_decoded_size(void *user, const char profile[64],
    const char *input, size_t input_len, size_t *out)
{
    (void)input; return identity_size(user, profile, input_len, out);
}
static obscura64_status identity_copy(void *user, const char profile[64],
    const void *input, size_t input_len, char *out, size_t capacity, size_t *out_len)
{
    (void)user; (void)profile; *out_len = input_len;
    if (capacity < input_len) return OBSCURA64_BUFFER_TOO_SMALL;
    if (input_len != 0) memcpy(out, input, input_len);
    return OBSCURA64_OK;
}
static obscura64_status identity_decode(void *user, const char profile[64],
    const char *input, size_t input_len, void *out, size_t capacity, size_t *out_len)
{
    return identity_copy(user, profile, input, input_len,
        (char *)out, capacity, out_len);
}

static int utf8(const wchar_t *wide, char *out, int capacity)
{
    return WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
        wide, -1, out, capacity, NULL, NULL) != 0;
}

static int put(const wchar_t *path, const void *data, DWORD size)
{
    HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD written = 0;
    int okay;
    if (h == INVALID_HANDLE_VALUE) return 0;
    okay = WriteFile(h, data, size, &written, NULL) && written == size;
    return CloseHandle(h) && okay;
}

static int equals_read(const char *path, const void *expected, size_t length,
    obscura64_file_source expected_source)
{
    void *plain = NULL;
    size_t size = 0;
    obscura64_file_source source = OBSCURA64_FILE_SOURCE_UNKNOWN;
    obscura64_data_format format = OBSCURA64_FORMAT_UNKNOWN;
    int okay = obscura64_read_file_alloc_ex(path, &plain, &size,
        &source, &format) == OBSCURA64_OK && size == length &&
        source == expected_source && format == OBSCURA64_FORMAT_V2 &&
        (size == 0 || memcmp(plain, expected, size) == 0);
    obscura64_free(plain);
    return okay;
}

int main(void)
{
    wchar_t temp[MAX_PATH], dir[MAX_PATH], target[MAX_PATH], backup[MAX_PATH];
    wchar_t other[MAX_PATH], missing[MAX_PATH], reserved[MAX_PATH];
    char path[MAX_PATH * 4], other_utf8[MAX_PATH * 4];
    char missing_utf8[MAX_PATH * 4], reserved_utf8[MAX_PATH * 4];
    const unsigned char first[] = {0, 255, 7, 'A', 0};
    const unsigned char second[] = {3, 4, 5};
    unsigned char *original = NULL, *current = NULL, *old_backup = NULL;
    size_t original_len = 0, current_len = 0, old_backup_len = 0;
    void *plain = NULL;
    size_t plain_len = 0;
    obscura64_file_source source;
    obscura64_status status;
    if (!GetTempPathW(MAX_PATH, temp) ||
        !GetTempFileNameW(temp, L"ob3", 0, dir)) return 1;
    if (!DeleteFileW(dir) || !CreateDirectoryW(dir, NULL)) return 1;
    swprintf(target, MAX_PATH, L"%ls\\file-\u6d4b\u8bd5.bin", dir);
    swprintf(backup, MAX_PATH, L"%ls.ob64.bak", target);
    swprintf(other, MAX_PATH, L"%ls\\other.bin", dir);
    swprintf(missing, MAX_PATH, L"%ls\\absent.bin", dir);
    swprintf(reserved, MAX_PATH, L"%ls.ob64.bak", other);
    CHECK(utf8(target, path, sizeof(path)) &&
          utf8(other, other_utf8, sizeof(other_utf8)) &&
          utf8(missing, missing_utf8, sizeof(missing_utf8)) &&
          utf8(reserved, reserved_utf8, sizeof(reserved_utf8)));
    CHECK(obscura64_read_file_alloc(missing_utf8, &plain, &plain_len) ==
          OBSCURA64_FILE_NOT_FOUND);
    CHECK(obscura64_write_file(path, OBSCURA64_PROTECTION_NONE,
          first, sizeof(first)) == OBSCURA64_OK);
    CHECK(equals_read(path, first, sizeof(first), OBSCURA64_FILE_SOURCE_PRIMARY));
    CHECK(obscura64_file_read_all(target, &original, &original_len) == OBSCURA64_OK);
    CHECK(obscura64_file_read_all(backup, &old_backup, &old_backup_len) == OBSCURA64_OK &&
          old_backup_len == original_len && memcmp(old_backup, original, original_len) == 0);
    free(old_backup); old_backup = NULL;
    CHECK(obscura64_write_file(path, OBSCURA64_PROTECTION_CASUAL,
          second, sizeof(second)) == OBSCURA64_OK);
    CHECK(equals_read(path, second, sizeof(second), OBSCURA64_FILE_SOURCE_PRIMARY));
    CHECK(obscura64_file_read_all(backup, &old_backup, &old_backup_len) == OBSCURA64_OK &&
          old_backup_len == original_len && memcmp(old_backup, original, original_len) == 0);
    CHECK(obscura64_file_read_all(target, &current, &current_len) == OBSCURA64_OK);
    CHECK(put(target, "bad", 3));
    CHECK(equals_read(path, first, sizeof(first), OBSCURA64_FILE_SOURCE_BACKUP));
    CHECK(obscura64_write_file(path, OBSCURA64_PROTECTION_CURRENT_USER,
          first, sizeof(first)) == OBSCURA64_OK);
    CHECK(equals_read(path, first, sizeof(first), OBSCURA64_FILE_SOURCE_PRIMARY));
    free(current); current = NULL;
    CHECK(obscura64_file_read_all(backup, &current, &current_len) == OBSCURA64_OK);
    CHECK(current_len == old_backup_len && memcmp(current, old_backup, current_len) == 0);
    free(current); current = NULL;
    CHECK(DeleteFileW(target));
    CHECK(equals_read(path, first, sizeof(first), OBSCURA64_FILE_SOURCE_BACKUP));
    CHECK(obscura64_write_file(path, OBSCURA64_PROTECTION_CASUAL, NULL, 0) == OBSCURA64_OK);
    CHECK(equals_read(path, NULL, 0, OBSCURA64_FILE_SOURCE_PRIMARY));
    CHECK(obscura64_write_file(other_utf8, OBSCURA64_PROTECTION_NONE,
          second, sizeof(second)) == OBSCURA64_OK);
    {
        unsigned char *large = (unsigned char *)malloc(1024U * 1024U);
        size_t i;
        CHECK(large != NULL);
        for (i = 0; i < 1024U * 1024U; ++i) large[i] = (unsigned char)i;
        CHECK(obscura64_write_file(other_utf8, OBSCURA64_PROTECTION_CASUAL,
            large, 1024U * 1024U) == OBSCURA64_OK);
        CHECK(equals_read(other_utf8, large, 1024U * 1024U,
            OBSCURA64_FILE_SOURCE_PRIMARY));
        free(large);
    }
    CHECK(obscura64_write_file(reserved_utf8, OBSCURA64_PROTECTION_NONE,
          second, sizeof(second)) == OBSCURA64_INVALID_ARGUMENT);
    CHECK(obscura64_read_file_alloc(reserved_utf8, &plain, &plain_len) ==
          OBSCURA64_INVALID_ARGUMENT);
    swprintf(reserved, MAX_PATH, L"%ls.OB64.LOCK", other);
    CHECK(utf8(reserved, reserved_utf8, sizeof(reserved_utf8)) &&
        obscura64_write_file(reserved_utf8, OBSCURA64_PROTECTION_NONE,
            first, sizeof(first)) == OBSCURA64_INVALID_ARGUMENT);
    swprintf(reserved, MAX_PATH, L"%ls.TMP.0123456789abcdef", other);
    CHECK(utf8(reserved, reserved_utf8, sizeof(reserved_utf8)) &&
        obscura64_write_file(reserved_utf8, OBSCURA64_PROTECTION_NONE,
            first, sizeof(first)) == OBSCURA64_INVALID_ARGUMENT);
    CHECK(obscura64_write_file("relative.bin", OBSCURA64_PROTECTION_NONE,
        first, sizeof(first)) == OBSCURA64_INVALID_ARGUMENT);
    swprintf(reserved, MAX_PATH, L"%ls\\NUL.bin", dir);
    CHECK(utf8(reserved, reserved_utf8, sizeof(reserved_utf8)) &&
        obscura64_write_file(reserved_utf8, OBSCURA64_PROTECTION_NONE,
            first, sizeof(first)) == OBSCURA64_INVALID_ARGUMENT);
    {
        obscura64_provider provider = {sizeof(provider), OBSCURA64_PROVIDER_ABI_VERSION,
            NULL, identity_size, identity_decoded_size, identity_copy, identity_decode};
        obscura64_context *context = NULL;
        wchar_t custom_path[MAX_PATH];
        char custom_utf8[MAX_PATH * 4], *encoded = NULL;
        size_t encoded_len = 0;
        swprintf(custom_path, MAX_PATH, L"%ls\\custom-v1", dir);
        CHECK(utf8(custom_path, custom_utf8, sizeof(custom_utf8)));
        CHECK(obscura64_context_create_from_profile_with_provider(
            (const char *)obscura64_profiles_v1[0], 64,
            &provider, &context) == OBSCURA64_OK);
        CHECK(obscura64_managed_encode_alloc(context, first, sizeof(first),
            &encoded, &encoded_len) == OBSCURA64_OK);
        CHECK(put(custom_path, encoded, (DWORD)encoded_len));
        CHECK(obscura64_read_file_alloc(custom_utf8, &plain, &plain_len) ==
            OBSCURA64_UNRECOGNIZED_DATA && plain == NULL);
        obscura64_free(encoded);
        obscura64_context_destroy(context);
        DeleteFileW(custom_path);
        swprintf(reserved, MAX_PATH, L"%ls.ob64.lock", custom_path);
        DeleteFileW(reserved);
    }
    {
        wchar_t absent_parent[MAX_PATH], dir_target[MAX_PATH];
        char absent_utf8[MAX_PATH * 4], dir_utf8[MAX_PATH * 4];
        swprintf(absent_parent, MAX_PATH, L"%ls\\absent-dir\\file", dir);
        swprintf(dir_target, MAX_PATH, L"%ls", dir);
        CHECK(utf8(absent_parent, absent_utf8, sizeof(absent_utf8)) &&
            utf8(dir_target, dir_utf8, sizeof(dir_utf8)));
        CHECK(obscura64_write_file(absent_utf8, OBSCURA64_PROTECTION_NONE,
            first, sizeof(first)) == OBSCURA64_IO_ERROR);
        CHECK(obscura64_read_file_alloc(absent_utf8, &plain, &plain_len) ==
            OBSCURA64_FILE_NOT_FOUND);
        CHECK(obscura64_write_file(dir_utf8, OBSCURA64_PROTECTION_NONE,
            first, sizeof(first)) == OBSCURA64_IO_ERROR);
        CHECK(obscura64_read_file_alloc(dir_utf8, &plain, &plain_len) ==
            OBSCURA64_IO_ERROR);
    }
    CHECK(obscura64_file_read_all(target, &current, &current_len) == OBSCURA64_OK);
    current[8] = 99; /* Clearly identified future V2 version. */
    CHECK(put(target, current, (DWORD)current_len));
    status = obscura64_read_file_alloc_ex(path, &plain, &plain_len, &source, NULL);
    CHECK(status == OBSCURA64_UNSUPPORTED_VERSION && plain == NULL &&
          source == OBSCURA64_FILE_SOURCE_UNKNOWN);
    CHECK(obscura64_write_file(path, OBSCURA64_PROTECTION_NONE,
          first, sizeof(first)) == OBSCURA64_UNSUPPORTED_VERSION);
    current[8] = 2; current[12] = 99;
    CHECK(put(target, current, (DWORD)current_len));
    CHECK(obscura64_read_file_alloc(path, &plain, &plain_len) ==
          OBSCURA64_UNSUPPORTED_PROTECTION);
    CHECK(obscura64_write_file(path, OBSCURA64_PROTECTION_NONE,
          first, sizeof(first)) == OBSCURA64_UNSUPPORTED_PROTECTION);
    CHECK(obscura64_file_read_all(target, &old_backup, &old_backup_len) == OBSCURA64_OK &&
          old_backup_len == current_len && memcmp(old_backup, current, current_len) == 0);
    free(old_backup); old_backup = NULL;
    free(current); current = NULL;
    CHECK(put(target, "bad", 3) && put(backup, "bad", 3));
    CHECK(obscura64_read_file_alloc(path, &plain, &plain_len) != OBSCURA64_OK);
    CHECK(obscura64_write_file(path, OBSCURA64_PROTECTION_NONE,
          first, sizeof(first)) == OBSCURA64_OK);
    CHECK(equals_read(path, first, sizeof(first), OBSCURA64_FILE_SOURCE_PRIMARY));
    {
        obscura64_context *legacy_context = NULL;
        char *legacy = NULL;
        size_t legacy_len = 0;
        obscura64_data_format format = OBSCURA64_FORMAT_UNKNOWN;
        CHECK(obscura64_context_create_from_profile(
            (const char *)obscura64_profiles_v1[0], 64,
            &legacy_context) == OBSCURA64_OK);
        CHECK(obscura64_managed_encode_alloc(legacy_context, first,
            sizeof(first), &legacy, &legacy_len) == OBSCURA64_OK);
        CHECK(put(target, legacy, (DWORD)legacy_len));
        CHECK(obscura64_read_file_alloc_ex(path, &plain, &plain_len,
            &source, &format) == OBSCURA64_OK &&
            source == OBSCURA64_FILE_SOURCE_PRIMARY &&
            format == OBSCURA64_FORMAT_V1_LEGACY_UPGRADE_RECOMMENDED &&
            plain_len == sizeof(first) && memcmp(plain, first, plain_len) == 0);
        obscura64_free(plain); plain = NULL;
        CHECK(obscura64_write_file(path, OBSCURA64_PROTECTION_CASUAL,
            second, sizeof(second)) == OBSCURA64_OK);
        CHECK(equals_read(path, second, sizeof(second),
            OBSCURA64_FILE_SOURCE_PRIMARY));
        free(old_backup); old_backup = NULL;
        CHECK(obscura64_file_read_all(backup, &old_backup, &old_backup_len) ==
            OBSCURA64_OK && old_backup_len == legacy_len &&
            memcmp(old_backup, legacy, legacy_len) == 0);
        obscura64_free(legacy);
        obscura64_context_destroy(legacy_context);
        free(old_backup); old_backup = NULL;
    }
    free(original);
    DeleteFileW(target); DeleteFileW(backup);
    swprintf(reserved, MAX_PATH, L"%ls.ob64.lock", target); DeleteFileW(reserved);
    DeleteFileW(other);
    swprintf(reserved, MAX_PATH, L"%ls.ob64.bak", other); DeleteFileW(reserved);
    swprintf(reserved, MAX_PATH, L"%ls.ob64.lock", other); DeleteFileW(reserved);
    swprintf(reserved, MAX_PATH, L"%ls.ob64.lock", missing); DeleteFileW(reserved);
    swprintf(reserved, MAX_PATH, L"%ls.ob64.lock", dir); DeleteFileW(reserved);
    RemoveDirectoryW(dir);
    printf("V2 file checks: %u\n", checks);
    return 0;
}
