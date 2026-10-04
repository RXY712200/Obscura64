#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "obscura64_internal.h"
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <wchar.h>

static unsigned int checks;
#define CHECK(c, name) do { ++checks; if (!(c)) { \
    fprintf(stderr, "FAIL: %s\n", name); return 1; } } while (0)

typedef struct verifier_data {
    unsigned int calls;
    unsigned int reject_call;
    WCHAR directory[MAX_PATH * 2];
    WCHAR last_temp[MAX_PATH];
    WCHAR previous_temp[MAX_PATH];
    int names_valid;
} verifier_data;

static int join(WCHAR *result, const WCHAR *base, const WCHAR *suffix)
{
    if (wcslen(base) + wcslen(suffix) >= MAX_PATH * 2) return 0;
    wcscpy(result, base);
    wcscat(result, suffix);
    return 1;
}

static unsigned int temp_count(const WCHAR *directory)
{
    WCHAR pattern[MAX_PATH * 2];
    WIN32_FIND_DATAW entry;
    HANDLE search;
    unsigned int count = 0;
    if (!join(pattern, directory, L"\\target.tmp.*")) return UINT_MAX;
    search = FindFirstFileW(pattern, &entry);
    if (search == INVALID_HANDLE_VALUE) return 0;
    do { if (wcscmp(entry.cFileName, L"target.tmp.sentinel") != 0 &&
             wcscmp(entry.cFileName, L"target.tmp") != 0) ++count; }
    while (FindNextFileW(search, &entry));
    FindClose(search);
    return count;
}

static obscura64_status verify(const unsigned char *data, size_t size, void *user)
{
    verifier_data *v = (verifier_data *)user;
    if (v != NULL) {
        WCHAR pattern[MAX_PATH * 2];
        WIN32_FIND_DATAW entry;
        HANDLE search;
        ++v->calls;
        join(pattern, v->directory, L"\\target.tmp.*");
        search = FindFirstFileW(pattern, &entry);
        if (search != INVALID_HANDLE_VALUE) {
            do {
                if (wcscmp(entry.cFileName, L"target.tmp.sentinel") != 0 &&
                    wcscmp(entry.cFileName, L"target.tmp") != 0) {
                    size_t i;
                    if (wcslen(entry.cFileName) != 27U) v->names_valid = 0;
                    for (i = 11; i < wcslen(entry.cFileName); ++i)
                        if (wcschr(L"0123456789abcdef", entry.cFileName[i]) == NULL) v->names_valid = 0;
                    wcscpy(v->last_temp, entry.cFileName);
                }
            } while (FindNextFileW(search, &entry));
            FindClose(search);
        }
        if (v->calls == v->reject_call) return OBSCURA64_STATE_CORRUPT;
    }
    return size == 4 && memcmp(data, "ABC", 3) == 0 ? OBSCURA64_OK : OBSCURA64_STATE_CORRUPT;
}

static int write_fixture(const WCHAR *path, const unsigned char *data, DWORD size)
{
    HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD written;
    int ok;
    if (file == INVALID_HANDLE_VALUE) return 0;
    ok = WriteFile(file, data, size, &written, NULL) && written == size;
    if (!CloseHandle(file)) ok = 0;
    return ok;
}

static int only_project_files(const WCHAR *project, unsigned int expected)
{
    WCHAR pattern[MAX_PATH * 2];
    WIN32_FIND_DATAW entry;
    HANDLE search;
    unsigned int count = 0;
    int ok = 1;
    if (!join(pattern, project, L"\\.obscura64\\*")) return 0;
    search = FindFirstFileW(pattern, &entry);
    if (search == INVALID_HANDLE_VALUE) return 0;
    do {
        if (wcscmp(entry.cFileName, L".") == 0 || wcscmp(entry.cFileName, L"..") == 0) continue;
        ++count;
        if (wcscmp(entry.cFileName, L"current.state") != 0 &&
            wcscmp(entry.cFileName, L"history.state") != 0) ok = 0;
    } while (FindNextFileW(search, &entry));
    FindClose(search);
    return ok && count == expected;
}

int main(void)
{
    WCHAR temp[MAX_PATH], seed[MAX_PATH], directory[MAX_PATH * 2];
    WCHAR target[MAX_PATH * 2], sentinel[MAX_PATH * 2], fixed[MAX_PATH * 2];
    WCHAR short_path[MAX_PATH * 2], long_path[MAX_PATH * 2], missing[MAX_PATH * 2];
    WCHAR project[MAX_PATH * 2], path[MAX_PATH * 2];
    char utf8[MAX_PATH * 6];
    const unsigned char old_data[] = "ABC1", new_data[] = "ABC2";
    unsigned char output[4];
    unsigned char unchanged[4] = {9, 9, 9, 9};
    verifier_data v;
    obscura64_context *context = NULL;
    unsigned int i;
    CHECK(GetTempPathW(MAX_PATH, temp) != 0 && GetTempFileNameW(temp, L"ops", 0, seed) != 0,
          "temporary fixture location");
    CHECK(DeleteFileW(seed) && join(directory, seed, L"-\x6D4B\x8BD5") && CreateDirectoryW(directory, NULL),
          "Unicode fixture directory");
    CHECK(join(target, directory, L"\\target") && join(sentinel, directory, L"\\target.tmp.sentinel") &&
        join(fixed, directory, L"\\target.tmp") && join(short_path, directory, L"\\short") &&
        join(long_path, directory, L"\\long") && join(missing, directory, L"\\missing"), "fixture paths");
    memset(&v, 0, sizeof(v));
    wcscpy(v.directory, directory);
    v.names_valid = 1;
    CHECK(obscura64_persistence_create_new_verified(target, old_data, 4, verify, NULL) == OBSCURA64_OK,
          "create new verifies exact bytes");
    CHECK(obscura64_persistence_create_new_verified(target, new_data, 4, verify, NULL) == OBSCURA64_BUSY,
          "create new never overwrites winner");
    CHECK(obscura64_persistence_read_verified(target, output, 4, verify, NULL) == OBSCURA64_OK &&
        memcmp(output, old_data, 4) == 0, "create loser leaves old bytes unchanged");
    CHECK(write_fixture(sentinel, old_data, 4) && write_fixture(fixed, old_data, 4), "unrelated temp-like sentinels");
    CHECK(obscura64_persistence_replace_verified(target, new_data, 4, verify, &v, old_data) == OBSCURA64_OK,
          "Unicode verified replacement succeeds");
    CHECK(v.calls == 2 && v.names_valid && v.last_temp[0] != 0, "pre and post verifier and same-directory hex temp");
    CHECK(obscura64_persistence_read_verified(target, output, 4, verify, NULL) == OBSCURA64_OK &&
        memcmp(output, new_data, 4) == 0, "final exact bytes and size");
    CHECK(temp_count(directory) == 0, "success owns no remaining temp");
    for (i = 0; i < 12; ++i) {
        wcscpy(v.previous_temp, v.last_temp);
        CHECK(obscura64_persistence_replace_verified(target, (i & 1U) ? new_data : old_data,
            4, verify, &v, NULL) == OBSCURA64_OK, "repeated replacement");
        CHECK(v.names_valid && wcscmp(v.previous_temp, v.last_temp) != 0, "fresh unique hex temp name");
        CHECK(temp_count(directory) == 0, "repeated success temp cleanup");
    }
    v.reject_call = v.calls + 1;
    CHECK(obscura64_persistence_replace_verified(target, old_data, 4, verify, &v, new_data) == OBSCURA64_STATE_CORRUPT,
          "pre-commit verifier rejection");
    CHECK(obscura64_persistence_read_verified(target, output, 4, verify, NULL) == OBSCURA64_OK &&
        memcmp(output, new_data, 4) == 0, "pre-commit failure leaves target byte-for-byte unchanged");
    CHECK(temp_count(directory) == 0, "rejected owned temp removed");
    v.reject_call = v.calls + 2;
    CHECK(obscura64_persistence_replace_verified(target, old_data, 4, verify, &v, new_data) == OBSCURA64_STATE_CORRUPT,
          "post-replacement verifier failure reported");
    CHECK(obscura64_persistence_read_verified(target, output, 4, verify, NULL) == OBSCURA64_OK &&
        memcmp(output, new_data, 4) == 0, "post-verification rollback preserves prior bytes");
    CHECK(temp_count(directory) == 0, "rollback temp cleanup");
    CHECK(obscura64_persistence_replace_verified(target, old_data, 3, verify, NULL, NULL) == OBSCURA64_STATE_CORRUPT,
          "short temp rejected by format verifier");
    CHECK(obscura64_persistence_read_verified(target, output, 4, verify, NULL) == OBSCURA64_OK &&
        memcmp(output, new_data, 4) == 0 && temp_count(directory) == 0, "short temp preserves target and cleanup");
    CHECK(write_fixture(short_path, old_data, 3) && write_fixture(long_path, old_data, 5), "short and trailing-byte fixtures");
    memcpy(output, unchanged, 4);
    CHECK(obscura64_persistence_read_verified(short_path, output, 4, verify, NULL) == OBSCURA64_STATE_CORRUPT &&
        memcmp(output, unchanged, 4) == 0, "short read rejected atomically");
    CHECK(obscura64_persistence_read_verified(long_path, output, 4, verify, NULL) == OBSCURA64_STATE_CORRUPT &&
        memcmp(output, unchanged, 4) == 0, "oversized read rejected atomically");
    CHECK(obscura64_persistence_read_verified(missing, output, 4, verify, NULL) == OBSCURA64_STATE_MISSING,
          "missing read");
    CHECK(obscura64_persistence_read_verified(directory, output, 4, verify, NULL) == OBSCURA64_STATE_CORRUPT,
          "directory is not payload");
    v.reject_call = v.calls + 1;
    CHECK(obscura64_persistence_read_verified(target, output, 4, verify, &v) == OBSCURA64_STATE_CORRUPT &&
        memcmp(output, unchanged, 4) == 0, "verifier rejection leaves read output unchanged");
    CHECK(obscura64_persistence_read_verified(sentinel, output, 4, verify, NULL) == OBSCURA64_OK &&
        memcmp(output, old_data, 4) == 0, "temp-like sentinel preserved");
    CHECK(obscura64_persistence_read_verified(fixed, output, 4, verify, NULL) == OBSCURA64_OK &&
        memcmp(output, old_data, 4) == 0, "fixed stale temp preserved and not used");
    CHECK(obscura64_persistence_replace_verified(NULL, new_data, 4, verify, NULL, NULL) == OBSCURA64_INVALID_ARGUMENT,
          "null path rejected");
    CHECK(obscura64_persistence_replace_verified(target, NULL, 4, verify, NULL, NULL) == OBSCURA64_INVALID_ARGUMENT,
          "null data rejected");
    CHECK(obscura64_persistence_replace_verified(target, new_data, 0, verify, NULL, NULL) == OBSCURA64_INVALID_ARGUMENT,
          "zero size rejected");
    CHECK(obscura64_persistence_replace_verified(target, new_data, 4, NULL, NULL, NULL) == OBSCURA64_INVALID_ARGUMENT,
          "null verifier rejected");
    CHECK(obscura64_persistence_create_new_verified(missing, old_data, 3, verify, NULL) == OBSCURA64_STATE_CORRUPT &&
        GetFileAttributesW(missing) == INVALID_FILE_ATTRIBUTES, "failed owned initial file cleaned");
    CHECK(join(project, directory, L"\\project") && CreateDirectoryW(project, NULL) &&
        WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, project, -1, utf8, sizeof(utf8), NULL, NULL) != 0,
          "Unicode project path");
    CHECK(obscura64_open(utf8, &context) == OBSCURA64_OK, "first init regression");
    obscura64_context_destroy(context); context = NULL;
    CHECK(only_project_files(project, 1), "first init no orphan temp");
    CHECK(obscura64_open(utf8, &context) == OBSCURA64_OK, "normal reopen regression");
    obscura64_context_destroy(context); context = NULL;
    CHECK(only_project_files(project, 1), "normal reopen no orphan temp");
    CHECK(obscura64_force_reinitialize(utf8, &context) == OBSCURA64_OK, "Force regression");
    obscura64_context_destroy(context);
    CHECK(only_project_files(project, 2), "Force only current and history");
    CHECK(join(path, project, L"\\.obscura64\\current.state") && DeleteFileW(path), "cleanup owned current");
    CHECK(join(path, project, L"\\.obscura64\\history.state") && DeleteFileW(path), "cleanup owned history");
    CHECK(join(path, project, L"\\.obscura64") && RemoveDirectoryW(path) && RemoveDirectoryW(project), "cleanup project directories");
    CHECK(DeleteFileW(target) && DeleteFileW(sentinel) && DeleteFileW(fixed) && DeleteFileW(short_path) &&
        DeleteFileW(long_path) && RemoveDirectoryW(directory), "cleanup only owned fixture files");
    printf("PASS: %u persistence checks\n", checks);
    return 0;
}
