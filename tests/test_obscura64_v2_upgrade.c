#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "obscura64.h"
#include "obscura64_internal.h"
#include "obscura64_profiles_v1.h"
#include "obscura64_v2_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { \
    fprintf(stderr, "upgrade failure at line %d\n", __LINE__); return 1; \
} } while (0)

static int to_utf8(const wchar_t *wide, char *utf8, int capacity)
{
    return WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
        wide, -1, utf8, capacity, NULL, NULL) != 0;
}

static int put(const wchar_t *path, const void *bytes, size_t size)
{
    HANDLE file;
    DWORD written;
    int okay;
    if (size > MAXDWORD) return 0;
    file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return 0;
    okay = WriteFile(file, bytes, (DWORD)size, &written, NULL) && written == size;
    return CloseHandle(file) && okay;
}

static int same_file(const wchar_t *path, const void *bytes, size_t size)
{
    unsigned char *actual = NULL;
    size_t length = 0;
    int okay = obscura64_file_read_all(path, &actual, &length) == OBSCURA64_OK &&
        length == size && (size == 0 || memcmp(actual, bytes, size) == 0);
    free(actual);
    return okay;
}

static int expected(const char *path, const void *bytes, size_t size)
{
    void *actual = NULL;
    size_t length = 0;
    int okay = obscura64_read_file_alloc(path, &actual, &length) == OBSCURA64_OK &&
        length == size && (size == 0 || memcmp(actual, bytes, size) == 0);
    obscura64_free(actual);
    return okay;
}

int main(void)
{
    wchar_t temp[MAX_PATH], dir[MAX_PATH], target[MAX_PATH], backup[MAX_PATH];
    wchar_t lock_path[MAX_PATH];
    char path[MAX_PATH * 4];
    const unsigned char payload[] = { 0, 1, 255, 'X', 0 };
    obscura64_context *context = NULL;
    char *legacy = NULL;
    size_t legacy_len = 0;
    unsigned char *old = NULL, *old_backup = NULL;
    size_t old_len = 0, old_backup_len = 0;
    obscura64_upgrade_result result;
    HANDLE held;
    int semantic;
    CHECK(GetTempPathW(MAX_PATH, temp) != 0 &&
        GetTempFileNameW(temp, L"ou4", 0, dir) != 0 &&
        DeleteFileW(dir) && CreateDirectoryW(dir, NULL));
    swprintf(target, MAX_PATH, L"%ls\\upgrade-\u6d4b\u8bd5.bin", dir);
    swprintf(backup, MAX_PATH, L"%ls.ob64.bak", target);
    swprintf(lock_path, MAX_PATH, L"%ls.ob64.lock", target);
    CHECK(to_utf8(target, path, sizeof(path)));
    CHECK(obscura64_context_create_from_profile(
        (const char *)obscura64_profiles_v1[1], 64, &context) == OBSCURA64_OK);
    CHECK(obscura64_managed_encode_alloc(context, payload, sizeof(payload),
        &legacy, &legacy_len) == OBSCURA64_OK);
    for (semantic = OBSCURA64_PROTECTION_NONE;
         semantic <= OBSCURA64_PROTECTION_CURRENT_USER; ++semantic) {
        obscura64_protection target_semantic = (obscura64_protection)semantic;
        CHECK(put(target, legacy, legacy_len));
        CHECK(obscura64_upgrade_file_ex(path, target_semantic, &result) == OBSCURA64_OK);
        CHECK(result.changed && result.source == OBSCURA64_FILE_SOURCE_PRIMARY &&
            result.format == OBSCURA64_FORMAT_V1_LEGACY_UPGRADE_RECOMMENDED &&
            result.target_protection == target_semantic);
        CHECK(same_file(backup, legacy, legacy_len) &&
            expected(path, payload, sizeof(payload)));
        CHECK(obscura64_file_read_all(target, &old, &old_len) == OBSCURA64_OK &&
            obscura64_file_read_all(backup, &old_backup, &old_backup_len) == OBSCURA64_OK);
        CHECK(obscura64_upgrade_file_ex(path, target_semantic, &result) == OBSCURA64_OK &&
            !result.changed && result.format == OBSCURA64_FORMAT_V2 &&
            same_file(target, old, old_len) &&
            same_file(backup, old_backup, old_backup_len));
        free(old); free(old_backup); old = old_backup = NULL;
    }
    CHECK(obscura64_upgrade_file_ex(path, OBSCURA64_PROTECTION_NONE, &result) == OBSCURA64_OK &&
        result.changed && result.source_protection == OBSCURA64_PROTECTION_CURRENT_USER &&
        expected(path, payload, sizeof(payload)));
    CHECK(obscura64_upgrade_file_ex(path, OBSCURA64_PROTECTION_CASUAL, &result) == OBSCURA64_OK &&
        result.changed && expected(path, payload, sizeof(payload)));
    CHECK(obscura64_upgrade_file_ex(path, OBSCURA64_PROTECTION_CURRENT_USER, &result) == OBSCURA64_OK &&
        result.changed && expected(path, payload, sizeof(payload)));
    CHECK(obscura64_upgrade_file_ex(path, OBSCURA64_PROTECTION_CASUAL, &result) == OBSCURA64_OK &&
        result.changed && expected(path, payload, sizeof(payload)));
    CHECK(obscura64_file_read_all(backup, &old_backup, &old_backup_len) == OBSCURA64_OK);
    CHECK(DeleteFileW(target));
    CHECK(obscura64_upgrade_file_ex(path, OBSCURA64_PROTECTION_NONE, &result) == OBSCURA64_OK &&
        result.source == OBSCURA64_FILE_SOURCE_BACKUP && result.changed &&
        same_file(backup, old_backup, old_backup_len) &&
        expected(path, payload, sizeof(payload)));
    free(old_backup); old_backup = NULL;
    CHECK(obscura64_file_read_all(backup, &old_backup, &old_backup_len) == OBSCURA64_OK);
    CHECK(put(target, "bad", 3));
    CHECK(obscura64_upgrade_file_ex(path, OBSCURA64_PROTECTION_CASUAL, &result) == OBSCURA64_OK &&
        result.source == OBSCURA64_FILE_SOURCE_BACKUP &&
        same_file(backup, old_backup, old_backup_len));
    free(old_backup); old_backup = NULL;
    CHECK(obscura64_file_read_all(target, &old, &old_len) == OBSCURA64_OK);
    old[8] = 99; CHECK(put(target, old, old_len));
    CHECK(obscura64_upgrade_file(path, OBSCURA64_PROTECTION_NONE) ==
        OBSCURA64_UNSUPPORTED_VERSION && same_file(target, old, old_len));
    old[8] = 2; old[12] = 99; CHECK(put(target, old, old_len));
    CHECK(obscura64_upgrade_file(path, OBSCURA64_PROTECTION_NONE) ==
        OBSCURA64_UNSUPPORTED_PROTECTION && same_file(target, old, old_len));
    free(old); old = NULL;
    held = CreateFileW(lock_path, GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    CHECK(held != INVALID_HANDLE_VALUE);
    { OVERLAPPED overlap = {0};
      CHECK(LockFileEx(held, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY,
          0, 1, 0, &overlap));
      CHECK(obscura64_upgrade_file(path, OBSCURA64_PROTECTION_NONE) == OBSCURA64_BUSY);
      CHECK(UnlockFileEx(held, 0, 1, 0, &overlap)); }
    CHECK(CloseHandle(held));
    CHECK(obscura64_upgrade_file(path, (obscura64_protection)88) ==
        OBSCURA64_UNSUPPORTED_PROTECTION);
    CHECK(put(target, legacy, legacy_len));
    CHECK(obscura64_upgrade_file_ex(path, OBSCURA64_PROTECTION_NONE, &result) == OBSCURA64_OK &&
        same_file(backup, legacy, legacy_len));
    {
        const unsigned char invalid_dpapi[] = {0x13, 0x57, 0x9b, 0xdf};
        void *wire = NULL;
        size_t wire_len = 0;
        CHECK(obscura64_v2_envelope_build(OBSCURA64_PROTECTION_CURRENT_USER,
            invalid_dpapi, sizeof(invalid_dpapi), &wire, &wire_len) == OBSCURA64_OK);
        CHECK(put(target, wire, wire_len));
        CHECK(obscura64_upgrade_file(path, OBSCURA64_PROTECTION_CASUAL) ==
            OBSCURA64_PROTECTION_FAILURE && same_file(target, wire, wire_len) &&
            same_file(backup, legacy, legacy_len));
        obscura64_free(wire);
    }
    CHECK(put(target, "custom-provider-data", 20));
    CHECK(obscura64_upgrade_file_ex(path, OBSCURA64_PROTECTION_CASUAL, &result) ==
        OBSCURA64_OK && result.source == OBSCURA64_FILE_SOURCE_BACKUP &&
        same_file(backup, legacy, legacy_len));
    CHECK(obscura64_write_file(path, OBSCURA64_PROTECTION_NONE,
        NULL, 0) == OBSCURA64_OK);
    CHECK(obscura64_upgrade_file(path, OBSCURA64_PROTECTION_CASUAL) ==
        OBSCURA64_OK && expected(path, NULL, 0));
    {
        size_t i;
        unsigned char *large = (unsigned char *)malloc(65537);
        CHECK(large != NULL);
        for (i = 0; i < 65537; ++i) large[i] = (unsigned char)i;
        CHECK(obscura64_write_file(path, OBSCURA64_PROTECTION_CASUAL,
            large, 65537) == OBSCURA64_OK);
        CHECK(obscura64_upgrade_file(path, OBSCURA64_PROTECTION_CURRENT_USER) ==
            OBSCURA64_OK && expected(path, large, 65537));
        free(large);
    }
    obscura64_free(legacy); obscura64_context_destroy(context);
    CHECK(DeleteFileW(target) && DeleteFileW(backup) && DeleteFileW(lock_path) &&
        RemoveDirectoryW(dir));
    printf("PASS: %u upgrade checks\n", checks);
    return 0;
}
