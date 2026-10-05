#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "obscura64_internal.h"
#include "obscura64_test_project_helpers.h"

static unsigned int checks;
static int selected = -1, target_history, crash_mode, injected;
static const char *ready_name;
#define CHECK(c, label) do { ++checks; if (!(c)) { fprintf(stderr, "FAIL: %s line %d\n", label, __LINE__); return 1; } } while (0)

int obscura64_test_fault(obscura64_test_fault_point point, const wchar_t *target, const wchar_t *temporary)
{
    int history = wcsstr(target, L"history.state") != NULL;
    (void)temporary;
    if (selected != (int)point || history != target_history) return 0;
    selected = -1; /* One shot: rollback remains a real verified operation. */
    injected = 1;
    if (crash_mode) {
        if (!test_child_ready(ready_name)) ExitProcess(77);
        Sleep(INFINITE); /* Parent kills this child at the chosen exact window. */
    }
    return 1;
}

static int child_mode(const char *mode, const char *event)
{
    WCHAR path[MAX_PATH * 2];
    test_project p;
    obscura64_context *c = NULL;
    if (!GetEnvironmentVariableW(L"OBSCURA64_TEST_PROJECT", path, MAX_PATH * 2) || !test_bind(&p, path)) return 2;
    crash_mode = 1; ready_name = event;
    target_history = mode[0] == 'A';
    selected = mode[0] == 'A' ? OBSCURA64_FAULT_REPLACE :
               mode[0] == 'B' ? OBSCURA64_FAULT_TEMP_CREATE : OBSCURA64_FAULT_AFTER_REPLACE;
    (void)obscura64_force_reinitialize(p.utf8, &c);
    obscura64_context_destroy(c);
    return 3; /* Chosen crash point must actually be reached. */
}

static int remove_owned_crash_temps(test_project *p)
{
    WCHAR pattern[MAX_PATH * 2], path[MAX_PATH * 2];
    WIN32_FIND_DATAW entry;
    HANDLE find;
    int ok = 1;
    wcscpy(pattern, p->directory); wcscat(pattern, L"\\*.tmp.*");
    find = FindFirstFileW(pattern, &entry);
    if (find == INVALID_HANDLE_VALUE) return GetLastError() == ERROR_FILE_NOT_FOUND;
    do {
        wcscpy(path, p->directory); wcscat(path, L"\\"); wcscat(path, entry.cFileName);
        /* This directory and every artifact in it were created by this test/child. */
        if (!DeleteFileW(path)) ok = 0;
    } while (FindNextFileW(find, &entry));
    FindClose(find);
    return ok;
}

int main(int argc, char **argv)
{
    test_project p;
    obscura64_context *context = NULL;
    obscura64_project_state old, current;
    obscura64_project_history old_history, history;
    obscura64_managed_state_kind kind;
    unsigned char before[160], after[160], history_before[336], history_after[336];
    unsigned int i, which;
    WCHAR stale_current[MAX_PATH * 2], stale_history[MAX_PATH * 2], stale_valid[MAX_PATH * 2];
    unsigned char junk[] = {1, 9, 3};
    PROCESS_INFORMATION child;
    HANDLE ready;
    WCHAR orphan_path[MAX_PATH * 2];
    unsigned char orphan_bytes[336], orphan_after[336];
    if (argc == 4 && strcmp(argv[1], "--child") == 0) return child_mode(argv[2], argv[3]);
    CHECK(test_make(&p), "isolated Unicode project initialized");
    for (i = 0; i < 3; ++i) {
        CHECK(obscura64_force_reinitialize(p.utf8, &context) == OBSCURA64_OK, "reach generation four");
        obscura64_context_destroy(context); context = NULL;
    }
    CHECK(test_load(&p, &old, &old_history) && old.generation == 4 &&
        test_bytes(p.current, before, 160, 0) && test_bytes(p.history, history_before, 336, 0), "canonical full-window baseline");
    for (which = 0; which < 2; ++which) {
        for (i = OBSCURA64_FAULT_TEMP_CREATE; i <= OBSCURA64_FAULT_AFTER_REPLACE; ++i) {
            CHECK(test_bytes(p.current, before, 160, 1) && test_bytes(p.history, history_before, 336, 1), "reset only owned fixtures");
            selected = (int)i; target_history = which == 0; injected = 0;
            {
                obscura64_status status = obscura64_force_reinitialize(p.utf8, &context);
                CHECK(injected && context == NULL && (status == OBSCURA64_IO_ERROR || status == OBSCURA64_STATE_CORRUPT),
                      "deterministic failure point reached and reported");
            }
            CHECK(test_load(&p, &current, &history), "no partial authoritative file after failure");
            CHECK(test_bytes(p.current, after, 160, 0) && memcmp(before, after, 160) == 0,
                  "failed Force and rollback never expose incremented current");
            CHECK(memcmp(current.project_id, old.project_id, 16) == 0 && current.generation == old.generation,
                  "identity and generation stable");
            CHECK(obscura64_managed_state_validate(&current, &history, &kind) == OBSCURA64_OK,
                  "managed pair is steady or safe overlap");
            if (target_history) {
                CHECK(test_bytes(p.history, history_after, 336, 0) && memcmp(history_before, history_after, 336) == 0 &&
                    kind == OBSCURA64_MANAGED_STEADY, "history failure preserves both old authoritative blobs");
            } else {
                CHECK(kind == OBSCURA64_MANAGED_OVERLAP && history.entries[0].generation == old.generation &&
                    memcmp(history.entries[0].profile, old.profile, 64) == 0, "history-success current-failure retains exact old Profile");
                CHECK(history.count == 3 && history.entries[1].generation == 3 && history.entries[2].generation == 2,
                      "safe overlap preserves full retained window");
            }
            CHECK(test_no_temps(&p), "all owned failed-operation temps cleaned");
            CHECK(obscura64_open(p.utf8, &context) == OBSCURA64_OK, "failure released active kernel lock");
            obscura64_context_destroy(context); context = NULL;
        }
    }
    CHECK(test_bytes(p.current, before, 160, 1) && test_bytes(p.history, history_before, 336, 1), "retry sequence baseline");
    selected = OBSCURA64_FAULT_REPLACE; target_history = 0; injected = 0;
    CHECK(obscura64_force_reinitialize(p.utf8, &context) == OBSCURA64_IO_ERROR && injected, "attempt one history succeeds current fails");
    for (i = 0; i < 2; ++i) {
        CHECK(obscura64_force_reinitialize(p.utf8, &context) == OBSCURA64_OK, "retry normalization then next normal Force");
        obscura64_context_destroy(context); context = NULL;
    }
    CHECK(test_load(&p, &current, &history) && current.generation == 6 && history.count == 3 &&
        history.entries[0].generation == 5 && history.entries[1].generation == 4 && history.entries[2].generation == 3 &&
        memcmp(current.project_id, old.project_id, 16) == 0 &&
        memcmp(history.entries[1].profile, old.profile, 64) == 0,
        "retry sequence N+2 and N+1,N,N-1 with stable identity and old Profile");

    wcscpy(stale_current, p.current); wcscat(stale_current, L".tmp.1111111111111111");
    wcscpy(stale_history, p.history); wcscat(stale_history, L".tmp.2222222222222222");
    wcscpy(stale_valid, p.current); wcscat(stale_valid, L".tmp.3333333333333333");
    CHECK(test_bytes(stale_current, junk, sizeof(junk), 1) && test_bytes(stale_history, junk, sizeof(junk), 1) &&
        test_bytes(stale_valid, before, 160, 1), "truncated/random and valid-but-wrong stale artifacts");
    CHECK(obscura64_open(p.utf8, &context) == OBSCURA64_OK, "stale temps ignored by normal open");
    obscura64_context_destroy(context); context = NULL;
    CHECK(obscura64_force_reinitialize(p.utf8, &context) == OBSCURA64_OK, "stale temps do not block Force");
    obscura64_context_destroy(context); context = NULL;
    CHECK(test_bytes(stale_current, after, sizeof(junk), 0) && memcmp(after, junk, sizeof(junk)) == 0 &&
        test_bytes(stale_history, history_after, sizeof(junk), 0) && memcmp(history_after, junk, sizeof(junk)) == 0 &&
        test_bytes(stale_valid, after, 160, 0) && memcmp(before, after, 160) == 0, "unrelated stale artifacts byte-for-byte preserved");
    CHECK(test_bytes(p.current, after, 160, 0), "save valid current before corruption");
    after[80] ^= 1;
    CHECK(test_bytes(p.current, after, 160, 1), "corrupt owned authoritative current");
    CHECK(obscura64_open(p.utf8, &context) == OBSCURA64_STATE_CORRUPT && context == NULL &&
        obscura64_force_reinitialize(p.utf8, &context) == OBSCURA64_STATE_CORRUPT && context == NULL,
        "no fallback to valid stale temp or history for corrupt current");
    CHECK(test_bytes(p.current, before, 160, 1) && test_bytes(p.history, history_before, 336, 1), "restore owned canonical baseline");
    CHECK(DeleteFileW(stale_current) && DeleteFileW(stale_history) && DeleteFileW(stale_valid), "test deletes only its deliberate sentinels");

    for (i = 0; i < 3; ++i) {
        WCHAR mode[2] = {(WCHAR)(L'A' + i), 0};
        CHECK(test_bytes(p.current, before, 160, 1) && test_bytes(p.history, history_before, 336, 1), "reset crash fixture");
        CHECK(test_spawn(&p, mode, &child, &ready), "spawn isolated crash-window child");
        CHECK(WaitForSingleObject(ready, 10000) == WAIT_OBJECT_0, "child reached exact crash window under exclusive lock");
        CHECK(obscura64_open(p.utf8, &context) == OBSCURA64_BUSY && context == NULL, "paused transaction excludes normal reader");
        CHECK(obscura64_force_reinitialize(p.utf8, &context) == OBSCURA64_BUSY && context == NULL,
              "paused child Force excludes competing parent Force");
        CHECK(TerminateProcess(child.hProcess, 101) && WaitForSingleObject(child.hProcess, 10000) == WAIT_OBJECT_0,
              "abrupt termination without cleanup or unlock");
        CloseHandle(ready); CloseHandle(child.hThread); CloseHandle(child.hProcess);
        CHECK(GetFileAttributesW(p.lock) != INVALID_FILE_ATTRIBUTES && test_load(&p, &current, &history),
              "crash leaves harmless lock file and full valid authoritative files");
        CHECK(obscura64_managed_state_validate(&current, &history, &kind) == OBSCURA64_OK,
              "crash pair validates steady or safe overlap");
        CHECK(current.generation == (i == 2 ? 5U : 4U) && memcmp(current.project_id, old.project_id, 16) == 0,
              "crash generation old or fully committed new, identity unchanged");
        if (i == 0) {
            WIN32_FIND_DATAW entry;
            WCHAR pattern[MAX_PATH * 2];
            HANDLE find;
            CHECK(kind == OBSCURA64_MANAGED_STEADY && !test_no_temps(&p) &&
                memcmp(current.profile, old.profile, 64) == 0, "window A old authoritative state and orphan temp");
            wcscpy(pattern, p.history); wcscat(pattern, L".tmp.*");
            find = FindFirstFileW(pattern, &entry);
            CHECK(find != INVALID_HANDLE_VALUE, "locate only child-owned orphan");
            wcscpy(orphan_path, p.directory); wcscat(orphan_path, L"\\"); wcscat(orphan_path, entry.cFileName);
            FindClose(find);
            CHECK(test_bytes(orphan_path, orphan_bytes, 336, 0), "snapshot actual child orphan bytes");
        } else {
            CHECK(kind == (i == 1 ? OBSCURA64_MANAGED_OVERLAP : OBSCURA64_MANAGED_STEADY) &&
                history.entries[0].generation == 4 && memcmp(history.entries[0].profile, old.profile, 64) == 0,
                "windows B/C preserve sufficient old Profile in history");
        }
        CHECK(obscura64_open(p.utf8, &context) == OBSCURA64_OK, "process exit releases kernel lock, temps ignored");
        obscura64_context_destroy(context); context = NULL;
        CHECK(obscura64_force_reinitialize(p.utf8, &context) == OBSCURA64_OK, "next Force resumes from actual crash state");
        obscura64_context_destroy(context); context = NULL;
        CHECK(test_load(&p, &current, &history) && current.generation == (i == 2 ? 6U : 5U) &&
            obscura64_managed_state_validate(&current, &history, &kind) == OBSCURA64_OK && kind == OBSCURA64_MANAGED_STEADY,
            "crash retry increments once and restores canonical full window");
        if (i == 0) CHECK(!test_no_temps(&p) && test_bytes(orphan_path, orphan_after, 336, 0) &&
            memcmp(orphan_bytes, orphan_after, 336) == 0,
            "unrelated child orphan byte-for-byte untouched by reopen and Force");
        CHECK(remove_owned_crash_temps(&p), "test cleanup only artifacts in its owned crash fixture");
    }
    CHECK(test_cleanup(&p), "cleanup owned failure fixture");
    printf("PASS: %u failure/crash checks\n", checks);
    return 0;
}
