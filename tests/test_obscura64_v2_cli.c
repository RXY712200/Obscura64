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
    fprintf(stderr, "CLI failure at line %d\n", __LINE__); return 1; \
} } while (0)

static int run(const wchar_t *exe, const wchar_t *args, DWORD *code, char output[2048])
{
    SECURITY_ATTRIBUTES attributes = {sizeof(attributes), NULL, TRUE};
    STARTUPINFOW startup = {0};
    PROCESS_INFORMATION process = {0};
    HANDLE read_pipe = NULL, write_pipe = NULL;
    wchar_t command[MAX_PATH * 10];
    DWORD got = 0;
    int okay;
    if (!CreatePipe(&read_pipe, &write_pipe, &attributes, 0)) return 0;
    if (!SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0)) {
        CloseHandle(read_pipe); CloseHandle(write_pipe); return 0;
    }
    swprintf(command, MAX_PATH * 10, L"\"%ls\" %ls", exe, args);
    startup.cb = sizeof(startup); startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = write_pipe; startup.hStdError = write_pipe;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    okay = CreateProcessW(NULL, command, NULL, NULL, TRUE,
        CREATE_NO_WINDOW, NULL, NULL, &startup, &process) != 0;
    CloseHandle(write_pipe);
    if (!okay) { CloseHandle(read_pipe); return 0; }
    okay = WaitForSingleObject(process.hProcess, 60000) == WAIT_OBJECT_0;
    if (!okay) { TerminateProcess(process.hProcess, 99);
        WaitForSingleObject(process.hProcess, 10000); }
    if (okay) okay = GetExitCodeProcess(process.hProcess, code) != 0;
    if (!ReadFile(read_pipe, output, 2047, &got, NULL)) got = 0;
    output[got] = '\0';
    CloseHandle(read_pipe); CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return okay;
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

int main(int argc, char **argv)
{
    wchar_t exe[MAX_PATH * 3], temp[MAX_PATH], dir[MAX_PATH];
    wchar_t target[MAX_PATH], backup[MAX_PATH], lock_path[MAX_PATH];
    wchar_t args[MAX_PATH * 5];
    char path[MAX_PATH * 4], log[2048];
    const unsigned char payload[] = {0, 'S', 'E', 'C', 'R', 'E', 'T', 0xff};
    unsigned char *protected_bytes = NULL, *after = NULL;
    size_t protected_len = 0, after_len = 0, legacy_len = 0;
    char *legacy = NULL;
    obscura64_context *context = NULL;
    DWORD code;
    int semantic;
    CHECK(argc == 2 && MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        argv[1], -1, exe, MAX_PATH * 3) != 0);
    CHECK(GetTempPathW(MAX_PATH, temp) &&
        GetTempFileNameW(temp, L"oc4", 0, dir) &&
        DeleteFileW(dir) && CreateDirectoryW(dir, NULL));
    swprintf(target, MAX_PATH, L"%ls\\tool-\u6d4b\u8bd5.bin", dir);
    swprintf(backup, MAX_PATH, L"%ls.ob64.bak", target);
    swprintf(lock_path, MAX_PATH, L"%ls.ob64.lock", target);
    CHECK(WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
        target, -1, path, sizeof(path), NULL, NULL) != 0);
    CHECK(run(exe, L"", &code, log) && code == 2);
    CHECK(run(exe, L"unknown", &code, log) && code == 2);
    swprintf(args, MAX_PATH * 5, L"upgrade \"%ls\"", target);
    CHECK(run(exe, args, &code, log) && code == 2);
    swprintf(args, MAX_PATH * 5, L"upgrade \"%ls\" --to secure", target);
    CHECK(run(exe, args, &code, log) && code == 2);
    CHECK(obscura64_context_create_from_profile(
        (const char *)obscura64_profiles_v1[1], 64, &context) == OBSCURA64_OK);
    CHECK(obscura64_managed_encode_alloc(context, payload, sizeof(payload),
        &legacy, &legacy_len) == OBSCURA64_OK && put(target, legacy, legacy_len));
    swprintf(args, MAX_PATH * 5, L"inspect \"%ls\"", target);
    CHECK(run(exe, args, &code, log) && code == 0 &&
        strstr(log, "V1 legacy") && strstr(log, "upgrade recommended: yes"));
    swprintf(args, MAX_PATH * 5, L"verify \"%ls\"", target);
    CHECK(run(exe, args, &code, log) && code == 0 &&
        strstr(log, "verification: V1 Managed format/digest valid") &&
        strstr(log, "authenticity: not cryptographically established") &&
        !strstr(log, "valid under available checks") &&
        !strstr(log, "SECRET"));
    swprintf(args, MAX_PATH * 5, L"upgrade \"%ls\" --to casual", target);
    CHECK(run(exe, args, &code, log) && code == 0 &&
        strstr(log, "source: PRIMARY") && strstr(log, "changed: yes"));
    CHECK(obscura64_file_read_all(backup, &after, &after_len) == OBSCURA64_OK &&
        after_len == legacy_len && memcmp(after, legacy, legacy_len) == 0);
    free(after); after = NULL;
    CHECK(run(exe, args, &code, log) && code == 0 && strstr(log, "changed: no"));
    swprintf(args, MAX_PATH * 5, L"upgrade \"%ls\" --to current-user", target);
    CHECK(run(exe, args, &code, log) && code == 0 &&
        strstr(log, "source protection: CASUAL") &&
        strstr(log, "target protection: CURRENT_USER") &&
        strstr(log, "changed: yes"));
    swprintf(args, MAX_PATH * 5, L"upgrade \"%ls\" --to casual", target);
    CHECK(run(exe, args, &code, log) && code == 0 &&
        strstr(log, "source protection: CURRENT_USER") &&
        strstr(log, "changed: yes"));
    CHECK(put(target, legacy, legacy_len));
    swprintf(args, MAX_PATH * 5, L"upgrade \"%ls\" --to current-user", target);
    CHECK(run(exe, args, &code, log) && code == 0 &&
        strstr(log, "format: V1 legacy") &&
        strstr(log, "target protection: CURRENT_USER"));
    CHECK(same_file(backup, legacy, legacy_len));
    for (semantic = OBSCURA64_PROTECTION_NONE;
         semantic <= OBSCURA64_PROTECTION_CURRENT_USER; ++semantic) {
        const char *verification = semantic == OBSCURA64_PROTECTION_NONE ?
            "verification: structurally valid V2 NONE (canonical envelope and exact length)" :
            semantic == OBSCURA64_PROTECTION_CASUAL ?
            "verification: Casual format/digest valid" :
            "verification: valid under current Windows DPAPI context";
        const char *limit = semantic == OBSCURA64_PROTECTION_NONE ?
            "integrity: not provided by NONE" :
            semantic == OBSCURA64_PROTECTION_CASUAL ?
            "authentication: not provided" : "inner record: valid";
        void *created = NULL;
        size_t created_len = 0;
        CHECK(obscura64_protect_alloc((obscura64_protection)semantic,
            payload, sizeof(payload), &created, &created_len) == OBSCURA64_OK &&
            put(target, created, created_len));
        swprintf(args, MAX_PATH * 5, L"inspect \"%ls\"", target);
        CHECK(run(exe, args, &code, log) && code == 0 &&
            strstr(log, "format: V2") &&
            strstr(log, "supported current representation: yes") &&
            !strstr(log, "SECRET"));
        swprintf(args, MAX_PATH * 5, L"verify \"%ls\"", target);
        CHECK(run(exe, args, &code, log) && code == 0 &&
            strstr(log, verification) && strstr(log, limit) &&
            !strstr(log, "valid under available checks") &&
            !strstr(log, "SECRET"));
        obscura64_free(created);
    }
    CHECK(obscura64_file_read_all(target, &protected_bytes, &protected_len) == OBSCURA64_OK);
    protected_bytes[8] = 99;
    CHECK(put(target, protected_bytes, protected_len));
    swprintf(args, MAX_PATH * 5, L"inspect \"%ls\"", target);
    CHECK(run(exe, args, &code, log) && code == 1 &&
        strstr(log, "format: V2") && strstr(log, "unsupported V2 envelope version"));
    swprintf(args, MAX_PATH * 5, L"upgrade \"%ls\" --to none", target);
    CHECK(run(exe, args, &code, log) && code == 1 &&
        strstr(log, "unsupported V2 envelope version"));
    protected_bytes[8] = 2; protected_bytes[12] = 99;
    CHECK(put(target, protected_bytes, protected_len));
    swprintf(args, MAX_PATH * 5, L"inspect \"%ls\"", target);
    CHECK(run(exe, args, &code, log) && code == 1 &&
        strstr(log, "unsupported V2 protection kind") &&
        strstr(log, "protection: unsupported/unknown"));
    CHECK(put(target, "unknown", 7));
    CHECK(run(exe, args, &code, log) && code == 1 &&
        strstr(log, "unrecognized"));
    swprintf(args, MAX_PATH * 5, L"verify \"%ls\"", target);
    CHECK(run(exe, args, &code, log) && code == 1 &&
        strstr(log, "not supported V2 or V1"));
    CHECK(obscura64_protect_alloc(OBSCURA64_PROTECTION_CASUAL,
        payload, sizeof(payload), (void **)&after, &after_len) == OBSCURA64_OK);
    after[after_len - 1] ^= 1;
    CHECK(put(target, after, after_len));
    CHECK(run(exe, args, &code, log) && code == 1 &&
        strstr(log, "invalid or corrupted V2 Casual body") &&
        same_file(target, after, after_len));
    obscura64_free(after); after = NULL;
    {
        const unsigned char invalid_dpapi[] = {0x13, 0x57, 0x9b, 0xdf};
        void *wire = NULL;
        size_t wire_len = 0;
        CHECK(obscura64_v2_envelope_build(OBSCURA64_PROTECTION_CURRENT_USER,
            invalid_dpapi, sizeof(invalid_dpapi), &wire, &wire_len) == OBSCURA64_OK);
        CHECK(put(target, wire, wire_len));
        swprintf(args, MAX_PATH * 5, L"inspect \"%ls\"", target);
        CHECK(run(exe, args, &code, log) && code == 0 &&
            strstr(log, "protection: CURRENT_USER") &&
            same_file(target, wire, wire_len));
        swprintf(args, MAX_PATH * 5, L"verify \"%ls\"", target);
        CHECK(run(exe, args, &code, log) && code == 1 &&
            strstr(log, "protection backend failed") &&
            same_file(target, wire, wire_len));
        obscura64_free(wire);
    }
    CHECK(put(target, "OB64ENV2", 8));
    swprintf(args, MAX_PATH * 5, L"inspect \"%ls\"", target);
    CHECK(run(exe, args, &code, log) && code == 1 &&
        strstr(log, "format: V2") && strstr(log, "malformed V2 envelope") &&
        !strstr(log, "protection: NONE") &&
        same_file(target, "OB64ENV2", 8));
    swprintf(args, MAX_PATH * 5, L"verify \"%ls\"", target);
    CHECK(run(exe, args, &code, log) && code == 1 &&
        strstr(log, "malformed V2 envelope"));
    CHECK(put(target, "bad", 3));
    swprintf(args, MAX_PATH * 5, L"upgrade \"%ls\" --to none", target);
    CHECK(run(exe, args, &code, log) && code == 0 &&
        strstr(log, "source: BACKUP") && strstr(log, "changed: yes") &&
        !strstr(log, "SECRET"));
    CHECK(DeleteFileW(target));
    CHECK(run(exe, args, &code, log) && code == 0 &&
        strstr(log, "source: BACKUP"));
    {
        wchar_t previous[MAX_PATH];
        CHECK(GetCurrentDirectoryW(MAX_PATH, previous) != 0 &&
            SetCurrentDirectoryW(dir));
        CHECK(run(exe, L"inspect \"tool-\u6d4b\u8bd5.bin\"", &code, log) &&
            code == 0 && strstr(log, "format: V2"));
        CHECK(SetCurrentDirectoryW(previous));
    }
    {
        HANDLE held = CreateFileW(lock_path, GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        OVERLAPPED overlap = {0};
        CHECK(held != INVALID_HANDLE_VALUE &&
            LockFileEx(held, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY,
                0, 1, 0, &overlap));
        swprintf(args, MAX_PATH * 5, L"upgrade \"%ls\" --to casual", target);
        CHECK(run(exe, args, &code, log) && code == 1 &&
            strstr(log, "busy"));
        CHECK(UnlockFileEx(held, 0, 1, 0, &overlap) && CloseHandle(held));
    }
    free(protected_bytes); obscura64_free(legacy);
    obscura64_context_destroy(context);
    CHECK(DeleteFileW(target) && DeleteFileW(backup) && DeleteFileW(lock_path) &&
        RemoveDirectoryW(dir));
    printf("PASS: %u CLI checks\n", checks);
    return 0;
}
