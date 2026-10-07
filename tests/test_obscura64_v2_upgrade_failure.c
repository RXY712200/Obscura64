#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "obscura64.h"
#include "obscura64_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

static unsigned checks;
static int selected = -1, on_backup, crash;
#define CHECK(x) do { ++checks; if (!(x)) { \
    fprintf(stderr, "upgrade fault failure at line %d\n", __LINE__); return 1; \
} } while (0)

int obscura64_test_fault(obscura64_test_fault_point point,
    const wchar_t *target, const wchar_t *temporary)
{
    (void)temporary;
    if (selected != (int)point ||
        (wcsstr(target, L".ob64.bak") != NULL) != on_backup) return 0;
    selected = -1;
    if (crash) ExitProcess(73);
    return 1;
}

static int same(const wchar_t *path, const unsigned char *expected, size_t size)
{
    unsigned char *actual = NULL;
    size_t count = 0;
    int okay = obscura64_file_read_all(path, &actual, &count) == OBSCURA64_OK &&
        count == size && memcmp(actual, expected, size) == 0;
    free(actual);
    return okay;
}

static int valid(const wchar_t *path)
{
    unsigned char *bytes = NULL;
    void *plain = NULL;
    size_t size = 0, plain_size = 0;
    int okay = obscura64_file_read_all(path, &bytes, &size) == OBSCURA64_OK &&
        obscura64_unprotect_alloc(bytes, size, &plain, &plain_size) == OBSCURA64_OK;
    free(bytes); obscura64_free(plain);
    return okay;
}

static int remove_test_temps(const wchar_t *directory)
{
    wchar_t pattern[MAX_PATH], path[MAX_PATH];
    WIN32_FIND_DATAW found;
    HANDLE search;
    swprintf(pattern, MAX_PATH, L"%ls\\*.tmp.*", directory);
    search = FindFirstFileW(pattern, &found);
    if (search == INVALID_HANDLE_VALUE) return GetLastError() == ERROR_FILE_NOT_FOUND;
    do {
        swprintf(path, MAX_PATH, L"%ls\\%ls", directory, found.cFileName);
        if (!DeleteFileW(path)) { FindClose(search); return 0; }
    } while (FindNextFileW(search, &found));
    { DWORD end = GetLastError(); FindClose(search);
      return end == ERROR_NO_MORE_FILES; }
}

int main(int argc, char **argv)
{
    wchar_t temp[MAX_PATH], dir[MAX_PATH], target[MAX_PATH], backup[MAX_PATH];
    wchar_t lock_path[MAX_PATH], exe[MAX_PATH], command[MAX_PATH * 6];
    char path[MAX_PATH * 4];
    unsigned char *old = NULL;
    size_t old_size = 0;
    obscura64_status status;
    int point;
    if (argc == 4 && strcmp(argv[1], "--child") == 0) {
        selected = atoi(argv[3]); on_backup = 0; crash = 1;
        return obscura64_upgrade_file(argv[2], OBSCURA64_PROTECTION_CURRENT_USER)
            == OBSCURA64_OK ? 0 : 8;
    }
    CHECK(GetTempPathW(MAX_PATH, temp) &&
        GetTempFileNameW(temp, L"ouF", 0, dir) &&
        DeleteFileW(dir) && CreateDirectoryW(dir, NULL));
    swprintf(target, MAX_PATH, L"%ls\\fault.bin", dir);
    swprintf(backup, MAX_PATH, L"%ls.ob64.bak", target);
    swprintf(lock_path, MAX_PATH, L"%ls.ob64.lock", target);
    CHECK(WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
        target, -1, path, sizeof(path), NULL, NULL) != 0);
    for (point = OBSCURA64_FAULT_TEMP_CREATE;
         point <= OBSCURA64_FAULT_AFTER_REPLACE; ++point) {
        CHECK(obscura64_write_file(path, OBSCURA64_PROTECTION_CASUAL,
            "application-secret", 18) == OBSCURA64_OK);
        CHECK(obscura64_file_read_all(target, &old, &old_size) == OBSCURA64_OK);
        on_backup = 1; selected = point;
        status = obscura64_upgrade_file(path, OBSCURA64_PROTECTION_CURRENT_USER);
        CHECK(status != OBSCURA64_OK && selected == -1 &&
            same(target, old, old_size) && valid(backup));
        on_backup = 0; selected = point;
        status = obscura64_upgrade_file(path, OBSCURA64_PROTECTION_CURRENT_USER);
        CHECK(status != OBSCURA64_OK && selected == -1 &&
            same(target, old, old_size) && same(backup, old, old_size));
        free(old); old = NULL;
    }
    CHECK(obscura64_write_file(path, OBSCURA64_PROTECTION_CASUAL,
        "application-secret", 18) == OBSCURA64_OK);
    CHECK(obscura64_file_read_all(target, &old, &old_size) == OBSCURA64_OK);
    CHECK(GetModuleFileNameW(NULL, exe, MAX_PATH));
    CHECK(MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        path, -1, target, MAX_PATH));
    for (point = OBSCURA64_FAULT_REPLACE;
         point <= OBSCURA64_FAULT_AFTER_REPLACE; point +=
         OBSCURA64_FAULT_AFTER_REPLACE - OBSCURA64_FAULT_REPLACE) {
        STARTUPINFOW startup = {0};
        PROCESS_INFORMATION process = {0};
        DWORD code;
        CHECK(obscura64_write_file(path, OBSCURA64_PROTECTION_CASUAL,
            "application-secret", 18) == OBSCURA64_OK);
        free(old); old = NULL;
        CHECK(obscura64_file_read_all(target, &old, &old_size) == OBSCURA64_OK);
        startup.cb = sizeof(startup);
        swprintf(command, MAX_PATH * 6, L"\"%ls\" --child \"%ls\" %d",
            exe, target, point);
        CHECK(CreateProcessW(NULL, command, NULL, NULL, FALSE,
            CREATE_NO_WINDOW, NULL, NULL, &startup, &process));
        CHECK(WaitForSingleObject(process.hProcess, 60000) == WAIT_OBJECT_0 &&
            GetExitCodeProcess(process.hProcess, &code) && code == 73);
        CloseHandle(process.hThread); CloseHandle(process.hProcess);
        CHECK(valid(backup));
        CHECK(point == OBSCURA64_FAULT_REPLACE ? same(target, old, old_size) :
            valid(target));
        CHECK(obscura64_upgrade_file(path, OBSCURA64_PROTECTION_CURRENT_USER) ==
            OBSCURA64_OK);
    }
    free(old);
    CHECK(remove_test_temps(dir) && DeleteFileW(target) &&
        DeleteFileW(backup) && DeleteFileW(lock_path) &&
        RemoveDirectoryW(dir));
    printf("PASS: %u upgrade fault checks\n", checks);
    return 0;
}
