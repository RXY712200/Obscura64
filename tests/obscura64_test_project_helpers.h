#ifndef OBSCURA64_TEST_PROJECT_HELPERS_H
#define OBSCURA64_TEST_PROJECT_HELPERS_H
/* Local fixture helpers for the two Windows concurrency/failure executables. */
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <stdlib.h>

static HANDLE test_owned_children[16];
static unsigned int test_owned_child_count;
static void test_cleanup_children(void)
{
    unsigned int i;
    for (i = 0; i < test_owned_child_count; ++i) {
        DWORD code;
        if (GetExitCodeProcess(test_owned_children[i], &code) && code == STILL_ACTIVE) {
            TerminateProcess(test_owned_children[i], 88);
            WaitForSingleObject(test_owned_children[i], 10000);
        }
        CloseHandle(test_owned_children[i]);
    }
}

typedef struct test_project {
    WCHAR project[MAX_PATH * 2], directory[MAX_PATH * 2];
    WCHAR current[MAX_PATH * 2], history[MAX_PATH * 2], lock[MAX_PATH * 2];
    char utf8[MAX_PATH * 6];
} test_project;

static int test_bind(test_project *p, const WCHAR *path)
{
    if (wcslen(path) > MAX_PATH * 2 - 64U) return 0;
    wcscpy(p->project, path);
    wcscpy(p->directory, path); wcscat(p->directory, L"\\.obscura64");
    wcscpy(p->current, p->directory); wcscat(p->current, L"\\current.state");
    wcscpy(p->history, p->directory); wcscat(p->history, L"\\history.state");
    wcscpy(p->lock, p->directory); wcscat(p->lock, L"\\operation.lock");
    return WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, path, -1,
        p->utf8, sizeof(p->utf8), NULL, NULL) != 0;
}

static int test_make(test_project *p)
{
    WCHAR temp[MAX_PATH], seed[MAX_PATH], path[MAX_PATH * 2];
    obscura64_context *c = NULL;
    if (!GetTempPathW(MAX_PATH, temp) || !GetTempFileNameW(temp, L"o5", 0, seed) || !DeleteFileW(seed)) return 0;
    wcscpy(path, seed); wcscat(path, L"-\x6D4B\x8BD5");
    if (!CreateDirectoryW(path, NULL) || !test_bind(p, path)) return 0;
    if (obscura64_open(p->utf8, &c) != OBSCURA64_OK) return 0;
    obscura64_context_destroy(c);
    return 1;
}

static int test_bytes(const WCHAR *path, unsigned char *data, DWORD size, int write)
{
    HANDLE f = CreateFileW(path, write ? GENERIC_WRITE : GENERIC_READ,
        FILE_SHARE_READ, NULL, write ? CREATE_ALWAYS : OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    LARGE_INTEGER length;
    DWORD count = 0;
    int ok;
    if (f == INVALID_HANDLE_VALUE) return 0;
    ok = write ? WriteFile(f, data, size, &count, NULL) :
        (GetFileSizeEx(f, &length) && length.QuadPart == size && ReadFile(f, data, size, &count, NULL));
    if (write && ok) ok = FlushFileBuffers(f);
    if (!CloseHandle(f)) ok = 0;
    return ok && count == size;
}

static int test_load(test_project *p, obscura64_project_state *s, obscura64_project_history *h)
{
    unsigned char current[160], history[336];
    return test_bytes(p->current, current, 160, 0) && test_bytes(p->history, history, 336, 0) &&
        obscura64_state_deserialize(current, 160, s) == OBSCURA64_CORE_STATUS_SUCCESS &&
        obscura64_history_deserialize(history, 336, h) == OBSCURA64_CORE_STATUS_SUCCESS;
}

static int test_no_temps(test_project *p)
{
    WCHAR pattern[MAX_PATH * 2];
    WIN32_FIND_DATAW entry;
    HANDLE find;
    wcscpy(pattern, p->directory); wcscat(pattern, L"\\*.tmp.*");
    find = FindFirstFileW(pattern, &entry);
    if (find != INVALID_HANDLE_VALUE) { FindClose(find); return 0; }
    return GetLastError() == ERROR_FILE_NOT_FOUND;
}

static int test_remove_owned_directory(const WCHAR *path)
{
    unsigned int attempt;
    for (attempt = 0; attempt < 25U; ++attempt) {
        if (RemoveDirectoryW(path)) return 1;
        if (GetLastError() != ERROR_DIR_NOT_EMPTY) {
            fprintf(stderr, "Test directory removal failed: Windows error %lu\n", (unsigned long)GetLastError());
            return 0;
        }
        Sleep(10);
    }
    return 0; /* Persistent extra entries still fail; never delete them here. */
}

static int test_cleanup(test_project *p)
{
    int ok = DeleteFileW(p->current) != 0;
    if (GetFileAttributesW(p->history) != INVALID_FILE_ATTRIBUTES && !DeleteFileW(p->history)) ok = 0;
    if (!DeleteFileW(p->lock)) ok = 0;
    if (!test_remove_owned_directory(p->directory) || !test_remove_owned_directory(p->project)) ok = 0;
    return ok;
}

static int test_child_ready(const char *name)
{
    WCHAR wide[128];
    HANDLE event;
    int ok;
    if (!MultiByteToWideChar(CP_UTF8, 0, name, -1, wide, 128)) return 0;
    event = OpenEventW(EVENT_MODIFY_STATE, FALSE, wide);
    if (event == NULL) return 0;
    ok = SetEvent(event) != 0;
    CloseHandle(event);
    return ok;
}

static int test_spawn(test_project *p, const WCHAR *mode, PROCESS_INFORMATION *child, HANDLE *ready)
{
    static unsigned int serial;
    WCHAR exe[MAX_PATH * 2], event_name[128], command[MAX_PATH * 3];
    STARTUPINFOW startup;
    if (!GetModuleFileNameW(NULL, exe, MAX_PATH * 2)) return 0;
    swprintf(event_name, 128, L"Local\\Obscura64Test-%lu-%u", (unsigned long)GetCurrentProcessId(), ++serial);
    *ready = CreateEventW(NULL, TRUE, FALSE, event_name);
    if (*ready == NULL || !SetEnvironmentVariableW(L"OBSCURA64_TEST_PROJECT", p->project)) return 0;
    swprintf(command, MAX_PATH * 3, L"\"%ls\" --child %ls %ls", exe, mode, event_name);
    memset(&startup, 0, sizeof(startup)); startup.cb = sizeof(startup);
    memset(child, 0, sizeof(*child));
    if (!CreateProcessW(NULL, command, NULL, NULL, FALSE, CREATE_NO_WINDOW,
                        NULL, NULL, &startup, child)) { CloseHandle(*ready); return 0; }
    SetEnvironmentVariableW(L"OBSCURA64_TEST_PROJECT", NULL);
    if (test_owned_child_count == 0 && atexit(test_cleanup_children) != 0) return 0;
    if (test_owned_child_count >= 16 || !DuplicateHandle(GetCurrentProcess(), child->hProcess,
        GetCurrentProcess(), &test_owned_children[test_owned_child_count], 0, FALSE, DUPLICATE_SAME_ACCESS)) {
        TerminateProcess(child->hProcess, 88); return 0;
    }
    ++test_owned_child_count;
    return 1;
}

#endif
