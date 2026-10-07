#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "obscura64.h"
#include "obscura64_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

static unsigned checks;
static int selected = -1, select_backup, crash;
#define CHECK(x) do { ++checks; if (!(x)) { \
    fprintf(stderr, "FAIL line %d\n", __LINE__); return 1; \
} } while (0)

int obscura64_test_fault(obscura64_test_fault_point point,
    const wchar_t *target, const wchar_t *temporary)
{
    int backup = wcsstr(target, L".ob64.bak") != NULL;
    (void)temporary;
    if (selected != (int)point || backup != select_backup) return 0;
    selected = -1;
    if (crash) ExitProcess(73);
    return 1;
}

static int to_utf8(const wchar_t *w, char *s, int n)
{
    return WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
        w, -1, s, n, NULL, NULL) != 0;
}

static int validated(const wchar_t *path)
{
    unsigned char *bytes = NULL;
    size_t len = 0, plain_len = 0;
    void *plain = NULL;
    int okay;
    if (obscura64_file_read_all(path, &bytes, &len) != OBSCURA64_OK) return 0;
    okay = obscura64_unprotect_alloc(bytes, len, &plain, &plain_len) == OBSCURA64_OK;
    free(bytes); obscura64_free(plain);
    return okay;
}

static int no_temps(const wchar_t *path)
{
    wchar_t pattern[MAX_PATH];
    WIN32_FIND_DATAW data;
    HANDLE h;
    swprintf(pattern, MAX_PATH, L"%ls.tmp.*", path);
    h = FindFirstFileW(pattern, &data);
    if (h != INVALID_HANDLE_VALUE) { FindClose(h); return 0; }
    return GetLastError() == ERROR_FILE_NOT_FOUND;
}

static int read_old(const char *path)
{
    void *plain = NULL;
    size_t len = 0;
    int okay = obscura64_read_file_alloc(path, &plain, &len) == OBSCURA64_OK &&
        len == 3 && memcmp(plain, "old", 3) == 0;
    obscura64_free(plain);
    return okay;
}

int main(int argc, char **argv)
{
    wchar_t dir[MAX_PATH], temp[MAX_PATH], target[MAX_PATH], backup[MAX_PATH];
    wchar_t lock[MAX_PATH], command[MAX_PATH * 5], exe[MAX_PATH];
    char path[MAX_PATH * 4];
    unsigned char *before = NULL, *after = NULL;
    size_t before_len = 0, after_len = 0;
    int side, point;
    if (argc == 4 && strcmp(argv[1], "--child") == 0) {
        selected = atoi(argv[3]); select_backup = 0; crash = 1;
        return obscura64_write_file(argv[2], OBSCURA64_PROTECTION_CASUAL,
            "new", 3) == OBSCURA64_OK ? 0 : 8;
    }
    if (!GetTempPathW(MAX_PATH, temp) ||
        !GetTempFileNameW(temp, L"of3", 0, dir) ||
        !DeleteFileW(dir) || !CreateDirectoryW(dir, NULL)) return 1;
    for (side = 0; side < 2; ++side) {
        for (point = OBSCURA64_FAULT_TEMP_CREATE;
             point <= OBSCURA64_FAULT_AFTER_REPLACE; ++point) {
            obscura64_status status;
            swprintf(target, MAX_PATH, L"%ls\\f-%d-%d", dir, side, point);
            swprintf(backup, MAX_PATH, L"%ls.ob64.bak", target);
            swprintf(lock, MAX_PATH, L"%ls.ob64.lock", target);
            CHECK(to_utf8(target, path, sizeof(path)));
            CHECK(obscura64_write_file(path, OBSCURA64_PROTECTION_CASUAL,
                "old", 3) == OBSCURA64_OK);
            CHECK(obscura64_file_read_all(target, &before, &before_len) == OBSCURA64_OK);
            selected = point; select_backup = side == 0;
            status = obscura64_write_file(path, OBSCURA64_PROTECTION_CASUAL,
                "new", 3);
            CHECK(selected == -1 && status != OBSCURA64_OK);
            CHECK(obscura64_file_read_all(target, &after, &after_len) == OBSCURA64_OK &&
                before_len == after_len && memcmp(before, after, before_len) == 0);
            CHECK(read_old(path) && validated(backup));
            CHECK(no_temps(target) && no_temps(backup));
            free(before); free(after); before = after = NULL;
            DeleteFileW(target); DeleteFileW(backup); DeleteFileW(lock);
        }
    }
    /* With neither source valid, a failed backup step cannot advance primary. */
    for (point = OBSCURA64_FAULT_TEMP_CREATE;
         point <= OBSCURA64_FAULT_AFTER_REPLACE; ++point) {
        obscura64_status status;
        swprintf(target, MAX_PATH, L"%ls\\empty-%d", dir, point);
        swprintf(backup, MAX_PATH, L"%ls.ob64.bak", target);
        swprintf(lock, MAX_PATH, L"%ls.ob64.lock", target);
        CHECK(to_utf8(target, path, sizeof(path)));
        selected = point; select_backup = 1;
        status = obscura64_write_file(path, OBSCURA64_PROTECTION_NONE,
            "new", 3);
        CHECK(selected == -1 && status != OBSCURA64_OK);
        CHECK(obscura64_file_read_all(target, &after, &after_len) ==
            OBSCURA64_FILE_NOT_FOUND);
        CHECK(obscura64_file_read_all(backup, &after, &after_len) ==
            OBSCURA64_FILE_NOT_FOUND || validated(backup));
        free(after); after = NULL;
        CHECK(no_temps(target) && no_temps(backup));
        DeleteFileW(backup); DeleteFileW(lock);
    }
    /* Invalid primary plus a valid backup preserves exact backup bytes. */
    swprintf(target, MAX_PATH, L"%ls\\recover", dir);
    swprintf(backup, MAX_PATH, L"%ls.ob64.bak", target);
    CHECK(to_utf8(target, path, sizeof(path)));
    CHECK(obscura64_write_file(path, OBSCURA64_PROTECTION_CASUAL,
        "old", 3) == OBSCURA64_OK);
    CHECK(obscura64_file_read_all(backup, &before, &before_len) == OBSCURA64_OK);
    CHECK(DeleteFileW(target));
    selected = OBSCURA64_FAULT_REPLACE; select_backup = 0;
    CHECK(obscura64_write_file(path, OBSCURA64_PROTECTION_CASUAL,
        "new", 3) != OBSCURA64_OK && selected == -1);
    CHECK(obscura64_file_read_all(backup, &after, &after_len) == OBSCURA64_OK &&
        before_len == after_len && memcmp(before, after, before_len) == 0);
    free(before); free(after); before = after = NULL;
    CHECK(read_old(path));
    CHECK(GetModuleFileNameW(NULL, exe, MAX_PATH) != 0);
    for (point = 0; point < 3; ++point) {
        STARTUPINFOW startup;
        PROCESS_INFORMATION process;
        DWORD code = 0;
        int crash_point = point == 0 ? OBSCURA64_FAULT_TEMP_CREATE :
            point == 1 ? OBSCURA64_FAULT_PRE_VERIFY : OBSCURA64_FAULT_AFTER_REPLACE;
        swprintf(target, MAX_PATH, L"%ls\\crash-%d", dir, point);
        swprintf(backup, MAX_PATH, L"%ls.ob64.bak", target);
        swprintf(lock, MAX_PATH, L"%ls.ob64.lock", target);
        CHECK(to_utf8(target, path, sizeof(path)));
        CHECK(obscura64_write_file(path, OBSCURA64_PROTECTION_CASUAL,
            "old", 3) == OBSCURA64_OK);
        swprintf(command, MAX_PATH * 5, L"\"%ls\" --child \"%ls\" %d",
            exe, target, crash_point);
        memset(&startup, 0, sizeof(startup)); startup.cb = sizeof(startup);
        memset(&process, 0, sizeof(process));
        CHECK(CreateProcessW(exe, command, NULL, NULL, FALSE, 0, NULL, NULL,
            &startup, &process) != 0);
        CHECK(WaitForSingleObject(process.hProcess, 30000) == WAIT_OBJECT_0);
        CHECK(GetExitCodeProcess(process.hProcess, &code) && code == 73);
        CloseHandle(process.hThread); CloseHandle(process.hProcess);
        CHECK(validated(target) && validated(backup));
        CHECK(obscura64_write_file(path, OBSCURA64_PROTECTION_NONE,
            "after", 5) == OBSCURA64_OK);
        /* Test owns all files in this isolated directory, including crash temps. */
    }
    {
        WIN32_FIND_DATAW data;
        wchar_t pattern[MAX_PATH], item[MAX_PATH];
        HANDLE find;
        swprintf(pattern, MAX_PATH, L"%ls\\*", dir);
        find = FindFirstFileW(pattern, &data);
        if (find != INVALID_HANDLE_VALUE) {
            do {
                if (wcscmp(data.cFileName, L".") == 0 ||
                    wcscmp(data.cFileName, L"..") == 0) continue;
                swprintf(item, MAX_PATH, L"%ls\\%ls", dir, data.cFileName);
                DeleteFileW(item);
            } while (FindNextFileW(find, &data));
            FindClose(find);
        }
        CHECK(RemoveDirectoryW(dir));
    }
    printf("V2 file failure checks: %u\n", checks);
    return 0;
}
