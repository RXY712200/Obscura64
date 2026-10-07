#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "obscura64.h"
#include "obscura64_internal.h"
#include <stdio.h>
#include <string.h>
#include <wchar.h>

static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { \
    fprintf(stderr, "FAIL line %d\n", __LINE__); return 1; \
} } while (0)

static int utf8(const wchar_t *wide, char *out, int capacity)
{
    return WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
        wide, -1, out, capacity, NULL, NULL) != 0;
}

typedef struct thread_call {
    const char *path;
    int write;
    obscura64_status status;
} thread_call;

static DWORD WINAPI call_in_thread(LPVOID parameter)
{
    thread_call *call = (thread_call *)parameter;
    if (call->write) call->status = obscura64_write_file(call->path,
        OBSCURA64_PROTECTION_NONE, "thread", 6);
    else {
        void *plain = NULL;
        size_t size = 0;
        call->status = obscura64_read_file_alloc(call->path, &plain, &size);
        obscura64_free(plain);
    }
    return 0;
}

int main(int argc, char **argv)
{
    wchar_t dir[MAX_PATH], temp[MAX_PATH], target[MAX_PATH], lockpath[MAX_PATH];
    wchar_t exe[MAX_PATH], command[MAX_PATH * 5];
    char path[MAX_PATH * 4];
    obscura64_lock *first = NULL, *second = NULL;
    void *plain = NULL;
    size_t length = 0;
    if (argc == 5 && strcmp(argv[1], "--hold") == 0) {
        wchar_t wide[MAX_PATH], event_name[128];
        HANDLE ready;
        if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
            argv[2], -1, wide, MAX_PATH) ||
            !MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
            argv[3], -1, event_name, 128)) return 2;
        ready = OpenEventW(EVENT_MODIFY_STATE, FALSE, event_name);
        if (ready == NULL) return 3;
        if ((argv[4][0] == 'S' ?
            obscura64_lock_acquire_shared(wide, &first) :
            obscura64_lock_acquire_exclusive(wide, &first)) != OBSCURA64_OK) return 4;
        SetEvent(ready);
        CloseHandle(ready);
        Sleep(INFINITE);
    }
    if (!GetTempPathW(MAX_PATH, temp) ||
        !GetTempFileNameW(temp, L"ol3", 0, dir) ||
        !DeleteFileW(dir) || !CreateDirectoryW(dir, NULL)) return 1;
    swprintf(target, MAX_PATH, L"%ls\\file", dir);
    swprintf(lockpath, MAX_PATH, L"%ls.ob64.lock", target);
    CHECK(utf8(target, path, sizeof(path)));
    CHECK(obscura64_write_file(path, OBSCURA64_PROTECTION_NONE,
        "old", 3) == OBSCURA64_OK);
    CHECK(obscura64_lock_acquire_shared(lockpath, &first) == OBSCURA64_OK);
    CHECK(obscura64_lock_acquire_shared(lockpath, &second) == OBSCURA64_OK);
    {
        thread_call call = {path, 0, OBSCURA64_IO_ERROR};
        HANDLE thread = CreateThread(NULL, 0, call_in_thread, &call, 0, NULL);
        CHECK(thread != NULL && WaitForSingleObject(thread, 10000) == WAIT_OBJECT_0 &&
            call.status == OBSCURA64_OK);
        CloseHandle(thread);
        call.write = 1; call.status = OBSCURA64_IO_ERROR;
        thread = CreateThread(NULL, 0, call_in_thread, &call, 0, NULL);
        CHECK(thread != NULL && WaitForSingleObject(thread, 10000) == WAIT_OBJECT_0 &&
            call.status == OBSCURA64_BUSY);
        CloseHandle(thread);
    }
    CHECK(obscura64_read_file_alloc(path, &plain, &length) == OBSCURA64_OK &&
        length == 3 && memcmp(plain, "old", 3) == 0);
    obscura64_free(plain); plain = NULL;
    CHECK(obscura64_write_file(path, OBSCURA64_PROTECTION_NONE,
        "new", 3) == OBSCURA64_BUSY);
    CHECK(obscura64_lock_release(second) == OBSCURA64_OK);
    CHECK(obscura64_lock_release(first) == OBSCURA64_OK);
    CHECK(obscura64_lock_acquire_exclusive(lockpath, &first) == OBSCURA64_OK);
    {
        thread_call call = {path, 0, OBSCURA64_IO_ERROR};
        HANDLE thread = CreateThread(NULL, 0, call_in_thread, &call, 0, NULL);
        CHECK(thread != NULL && WaitForSingleObject(thread, 10000) == WAIT_OBJECT_0 &&
            call.status == OBSCURA64_BUSY);
        CloseHandle(thread);
    }
    CHECK(obscura64_read_file_alloc(path, &plain, &length) == OBSCURA64_BUSY);
    CHECK(obscura64_write_file(path, OBSCURA64_PROTECTION_NONE,
        "new", 3) == OBSCURA64_BUSY);
    CHECK(obscura64_lock_release(first) == OBSCURA64_OK);
    CHECK(GetModuleFileNameW(NULL, exe, MAX_PATH) != 0);
    {
        int mode;
        for (mode = 0; mode < 2; ++mode) {
        HANDLE ready = CreateEventW(NULL, TRUE, FALSE, L"Local\\obscura64-preview3-lock-test");
        STARTUPINFOW startup;
        PROCESS_INFORMATION child;
        DWORD code;
        char event_utf8[128];
        CHECK(ready != NULL);
        CHECK(utf8(L"Local\\obscura64-preview3-lock-test", event_utf8, sizeof(event_utf8)));
        swprintf(command, MAX_PATH * 5, L"\"%ls\" --hold \"%ls\" \"Local\\obscura64-preview3-lock-test\" %ls",
            exe, lockpath, mode == 0 ? L"S" : L"X");
        memset(&startup, 0, sizeof(startup)); startup.cb = sizeof(startup);
        memset(&child, 0, sizeof(child));
        CHECK(CreateProcessW(exe, command, NULL, NULL, FALSE, 0,
            NULL, NULL, &startup, &child) != 0);
        CHECK(WaitForSingleObject(ready, 10000) == WAIT_OBJECT_0);
        CHECK(obscura64_read_file_alloc(path, &plain, &length) ==
            (mode == 0 ? OBSCURA64_OK : OBSCURA64_BUSY));
        obscura64_free(plain); plain = NULL;
        CHECK(obscura64_write_file(path, OBSCURA64_PROTECTION_NONE,
            "new", 3) == OBSCURA64_BUSY);
        CHECK(TerminateProcess(child.hProcess, 74));
        CHECK(WaitForSingleObject(child.hProcess, 10000) == WAIT_OBJECT_0);
        CHECK(GetExitCodeProcess(child.hProcess, &code) && code == 74);
        CloseHandle(child.hThread); CloseHandle(child.hProcess); CloseHandle(ready);
        }
    }
    CHECK(obscura64_read_file_alloc(path, &plain, &length) == OBSCURA64_OK &&
        length == 3 && memcmp(plain, "old", 3) == 0);
    obscura64_free(plain);
    CHECK(obscura64_write_file(path, OBSCURA64_PROTECTION_NONE,
        "new", 3) == OBSCURA64_OK);
    CHECK(obscura64_read_file_alloc(path, &plain, &length) == OBSCURA64_OK &&
        length == 3 && memcmp(plain, "new", 3) == 0);
    obscura64_free(plain);
    swprintf(target, MAX_PATH, L"%ls\\file.ob64.bak", dir); DeleteFileW(target);
    swprintf(target, MAX_PATH, L"%ls\\file.ob64.lock", dir); DeleteFileW(target);
    swprintf(target, MAX_PATH, L"%ls\\file", dir); DeleteFileW(target);
    CHECK(RemoveDirectoryW(dir));
    printf("V2 file locking checks: %u\n", checks);
    return 0;
}
