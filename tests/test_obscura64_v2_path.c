#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "obscura64.h"
#include <stdio.h>
#include <string.h>
#include <wchar.h>

static unsigned checks, short_skips, symlink_skips, parent_skips, case_skips;
#define CHECK(x) do { ++checks; if (!(x)) { \
    fprintf(stderr, "path check failed at line %d\n", __LINE__); return 1; \
} } while (0)

static int utf8(const wchar_t *input, char *output, int capacity)
{
    return WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
        input, -1, output, capacity, NULL, NULL) != 0;
}

static int read_matches(const char *path, const char *expected)
{
    void *data = NULL;
    size_t length = 0;
    int okay = obscura64_read_file_alloc(path, &data, &length) == OBSCURA64_OK &&
        length == strlen(expected) && memcmp(data, expected, length) == 0;
    obscura64_free(data);
    return okay;
}

int main(void)
{
    wchar_t temp[MAX_PATH], dir[MAX_PATH], target[MAX_PATH], hard[MAX_PATH];
    wchar_t alias[MAX_PATH], backup[MAX_PATH], lock[MAX_PATH];
    wchar_t dotted[MAX_PATH], unicode[MAX_PATH], short_dir[MAX_PATH];
    wchar_t parent_alias[MAX_PATH], parent_target[MAX_PATH], case_alias[MAX_PATH];
    char target8[MAX_PATH * 4], alias8[MAX_PATH * 4];
    char dotted8[MAX_PATH * 4], unicode8[MAX_PATH * 4];
    void *rejected_output = NULL;
    size_t rejected_length = 0;
    DWORD got;
    static const char payload[] = "path-identity";
    got = GetTempPathW(MAX_PATH, temp);
    CHECK(got != 0 && got < MAX_PATH);
    CHECK(GetTempFileNameW(temp, L"o64", 0, dir) != 0);
    CHECK(DeleteFileW(dir));
    CHECK(CreateDirectoryW(dir, NULL));
    swprintf(target, MAX_PATH, L"%ls\\payload.ob64", dir);
    swprintf(hard, MAX_PATH, L"%ls\\hardlink.ob64", dir);
    swprintf(dotted, MAX_PATH, L"%ls\\.\\payload.ob64", dir);
    swprintf(unicode, MAX_PATH, L"%ls\\\u6d4b\u8bd5.ob64", dir);
    swprintf(parent_alias, MAX_PATH, L"%ls-alias", dir);
    swprintf(backup, MAX_PATH, L"%ls.ob64.bak", target);
    swprintf(lock, MAX_PATH, L"%ls.ob64.lock", target);
    CHECK(utf8(target, target8, sizeof(target8)) &&
        utf8(dotted, dotted8, sizeof(dotted8)) &&
        utf8(unicode, unicode8, sizeof(unicode8)));
    CHECK(obscura64_write_file("relative.ob64", OBSCURA64_PROTECTION_NONE,
        payload, sizeof(payload) - 1U) == OBSCURA64_INVALID_ARGUMENT);
    CHECK(obscura64_write_file("\\\\?\\GLOBALROOT\\Device\\HarddiskVolume1\\x",
        OBSCURA64_PROTECTION_NONE, payload, sizeof(payload) - 1U) ==
        OBSCURA64_INVALID_ARGUMENT);
    CHECK(obscura64_write_file(target8, OBSCURA64_PROTECTION_NONE,
        payload, sizeof(payload) - 1U) == OBSCURA64_OK);
    CHECK(read_matches(target8, payload) && read_matches(dotted8, payload));
    CHECK(obscura64_write_file(dotted8, OBSCURA64_PROTECTION_NONE,
        payload, sizeof(payload) - 1U) == OBSCURA64_OK);
    CHECK(GetFileAttributesW(backup) != INVALID_FILE_ATTRIBUTES);
    swprintf(alias, MAX_PATH, L"%ls\\child\\..\\payload.ob64", dir);
    CHECK(utf8(alias, alias8, sizeof(alias8)) && read_matches(alias8, payload));
    wcscpy_s(case_alias, MAX_PATH, target);
    CharUpperBuffW(case_alias, (DWORD)wcslen(case_alias));
    if (utf8(case_alias, alias8, sizeof(alias8)) &&
        GetFileAttributesW(case_alias) != INVALID_FILE_ATTRIBUTES)
        CHECK(read_matches(alias8, payload));
    else ++case_skips;
    CHECK(GetFileAttributesW(lock) != INVALID_FILE_ATTRIBUTES);
    CHECK(obscura64_write_file(unicode8, OBSCURA64_PROTECTION_NONE,
        payload, sizeof(payload) - 1U) == OBSCURA64_OK);
    CHECK(read_matches(unicode8, payload));
    CHECK(CreateHardLinkW(hard, target, NULL));
    CHECK(utf8(hard, alias8, sizeof(alias8)));
    CHECK(obscura64_read_file_alloc(alias8, &rejected_output,
        &rejected_length) == OBSCURA64_INVALID_ARGUMENT &&
        rejected_output == NULL && rejected_length == 0);
    CHECK(obscura64_write_file(alias8, OBSCURA64_PROTECTION_NONE,
        payload, sizeof(payload) - 1U) == OBSCURA64_INVALID_ARGUMENT);
    CHECK(obscura64_write_file(target8, OBSCURA64_PROTECTION_NONE,
        payload, sizeof(payload) - 1U) == OBSCURA64_INVALID_ARGUMENT);
    CHECK(DeleteFileW(hard));
    CHECK(read_matches(target8, payload));
    got = GetShortPathNameW(dir, short_dir, MAX_PATH);
    if (got != 0 && got < MAX_PATH && _wcsicmp(short_dir, dir) != 0) {
        swprintf(alias, MAX_PATH, L"%ls\\payload.ob64", short_dir);
        CHECK(utf8(alias, alias8, sizeof(alias8)));
        CHECK(read_matches(alias8, payload));
    } else ++short_skips;
    if (CreateSymbolicLinkW(parent_alias, dir, SYMBOLIC_LINK_FLAG_DIRECTORY)) {
        swprintf(parent_target, MAX_PATH, L"%ls\\payload.ob64", parent_alias);
        CHECK(utf8(parent_target, alias8, sizeof(alias8)));
        CHECK(read_matches(alias8, payload));
        CHECK(RemoveDirectoryW(parent_alias));
    } else ++parent_skips;
    swprintf(alias, MAX_PATH, L"%ls\\link.ob64", dir);
    if (CreateSymbolicLinkW(alias, target, 0)) {
        CHECK(utf8(alias, alias8, sizeof(alias8)));
        CHECK(obscura64_write_file(alias8, OBSCURA64_PROTECTION_NONE,
            payload, sizeof(payload) - 1U) == OBSCURA64_INVALID_ARGUMENT);
        CHECK(DeleteFileW(alias));
    } else ++symlink_skips;
    DeleteFileW(target); DeleteFileW(backup); DeleteFileW(lock);
    swprintf(backup, MAX_PATH, L"%ls.ob64.bak", unicode);
    swprintf(lock, MAX_PATH, L"%ls.ob64.lock", unicode);
    DeleteFileW(unicode); DeleteFileW(backup); DeleteFileW(lock);
    CHECK(RemoveDirectoryW(dir));
    printf("path checks=%u short-name skips=%u target-symlink skips=%u "
        "parent-symlink skips=%u case skips=%u\n",
        checks, short_skips, symlink_skips, parent_skips, case_skips);
    return 0;
}
