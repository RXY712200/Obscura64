#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "obscura64_internal.h"
#include "obscura64_test_project_helpers.h"

static unsigned int checks;
#define CHECK(c, label) do { ++checks; if (!(c)) { fprintf(stderr, "FAIL: %s line %d\n", label, __LINE__); return 1; } } while (0)

typedef struct call {
    HANDLE start;
    const char *path;
    int force;
    obscura64_status status;
    char profile[64];
} call;

static DWORD WINAPI invoke(LPVOID argument)
{
    call *c = (call *)argument;
    obscura64_context *context = NULL;
    WaitForSingleObject(c->start, 10000);
    c->status = c->force ? obscura64_force_reinitialize(c->path, &context) : obscura64_open(c->path, &context);
    if (context != NULL) memcpy(c->profile, context->profile, 64);
    obscura64_context_destroy(context);
    return 0;
}

static int child_mode(const char *mode, const char *event)
{
    WCHAR project[MAX_PATH * 2];
    test_project p;
    obscura64_lock *lock = NULL;
    if (!GetEnvironmentVariableW(L"OBSCURA64_TEST_PROJECT", project, MAX_PATH * 2) || !test_bind(&p, project)) return 2;
    if ((mode[0] == 'S' ? obscura64_lock_acquire_shared(p.lock, &lock) :
         obscura64_lock_acquire_exclusive(p.lock, &lock)) != OBSCURA64_OK) return 3;
    if (!test_child_ready(event)) return 4;
    Sleep(INFINITE); /* Parent terminates this owned child without explicit unlock. */
    return 5;
}

int main(int argc, char **argv)
{
    test_project p, fresh;
    obscura64_context *context = NULL;
    obscura64_lock *shared = NULL, *second = NULL, *exclusive = NULL;
    obscura64_project_state state, next;
    obscura64_project_history history;
    unsigned char before[160], after[160];
    PROCESS_INFORMATION child;
    HANDLE ready, start, threads[2];
    call calls[2];
    unsigned int i, success;
    WCHAR fresh_path[MAX_PATH * 2];
    int initialization_active;
    if (argc == 4 && strcmp(argv[1], "--child") == 0) return child_mode(argv[2], argv[3]);
    CHECK(test_make(&p), "Unicode fixture initialized");
    CHECK(GetFileAttributesW(p.lock) != INVALID_FILE_ATTRIBUTES, "persistent unlocked file exists");
    CHECK(obscura64_lock_acquire_exclusive(p.lock, &exclusive) == OBSCURA64_OK &&
        obscura64_lock_initialization_active(p.lock, &initialization_active) == OBSCURA64_OK &&
        initialization_active == 0, "ordinary writer has no initialization marker");
    CHECK(obscura64_open(p.utf8, &context) == OBSCURA64_BUSY && context == NULL,
          "generation-one existing project does not wait for ordinary writer");
    CHECK(obscura64_lock_release(exclusive) == OBSCURA64_OK, "release generation-one writer");
    CHECK(obscura64_lock_acquire_initial(p.lock, &exclusive) == OBSCURA64_OK &&
        obscura64_lock_initialization_active(p.lock, &initialization_active) == OBSCURA64_OK &&
        initialization_active == 1, "initialization marker is a real kernel byte range");
    CHECK(obscura64_lock_release(exclusive) == OBSCURA64_OK &&
        obscura64_lock_initialization_active(p.lock, &initialization_active) == OBSCURA64_OK &&
        initialization_active == 0, "marker disappears on release with no metadata file writes");
    CHECK(obscura64_open(p.utf8, &context) == OBSCURA64_OK, "unlocked file permits open");
    obscura64_context_destroy(context); context = NULL;
    CHECK(obscura64_force_reinitialize(p.utf8, &context) == OBSCURA64_OK, "unlocked file permits Force");
    obscura64_context_destroy(context); context = NULL;
    CHECK(test_load(&p, &state, &history) && test_bytes(p.current, before, 160, 0), "valid fixture baseline");
    CHECK(obscura64_lock_acquire_shared(p.lock, &shared) == OBSCURA64_OK &&
        obscura64_lock_acquire_shared(p.lock, &second) == OBSCURA64_OK, "two simultaneous shared handles");
    CHECK(obscura64_lock_acquire_exclusive(p.lock, &exclusive) == OBSCURA64_BUSY && exclusive == NULL, "shared excludes writer");
    start = CreateEventW(NULL, TRUE, FALSE, NULL);
    CHECK(start != NULL, "reader start barrier");
    memset(calls, 0, sizeof(calls));
    for (i = 0; i < 2; ++i) {
        calls[i].start = start; calls[i].path = p.utf8;
        threads[i] = CreateThread(NULL, 0, invoke, &calls[i], 0, NULL);
        CHECK(threads[i] != NULL, "concurrent reader thread");
    }
    SetEvent(start);
    CHECK(WaitForMultipleObjects(2, threads, TRUE, 10000) == WAIT_OBJECT_0, "readers complete");
    for (i = 0; i < 2; ++i) {
        CHECK(calls[i].status == OBSCURA64_OK && memcmp(calls[i].profile, state.profile, 64) == 0, "reader Profile shared and unchanged");
        CloseHandle(threads[i]);
    }
    CloseHandle(start);
    CHECK(test_bytes(p.current, after, 160, 0) && memcmp(before, after, 160) == 0, "shared reads never change state");
    CHECK(obscura64_force_reinitialize(p.utf8, &context) == OBSCURA64_BUSY && context == NULL, "reader versus Force BUSY");
    CHECK(obscura64_lock_release(second) == OBSCURA64_OK && obscura64_lock_release(shared) == OBSCURA64_OK, "release shared handles");
    CHECK(obscura64_force_reinitialize(p.utf8, &context) == OBSCURA64_OK, "Force succeeds after reader release");
    obscura64_context_destroy(context); context = NULL;
    CHECK(obscura64_lock_acquire_exclusive(p.lock, &exclusive) == OBSCURA64_OK, "exclusive holder");
    CHECK(obscura64_open(p.utf8, &context) == OBSCURA64_BUSY && context == NULL, "writer versus reader BUSY");
    CHECK(obscura64_force_reinitialize(p.utf8, &context) == OBSCURA64_BUSY && context == NULL, "writer versus writer BUSY");
    CHECK(obscura64_lock_release(exclusive) == OBSCURA64_OK, "release exclusive");
    CHECK(test_spawn(&p, L"S", &child, &ready), "spawn separate shared holder");
    CHECK(WaitForSingleObject(ready, 10000) == WAIT_OBJECT_0, "child acquired shared kernel lock");
    CHECK(obscura64_open(p.utf8, &context) == OBSCURA64_OK, "other process shared reader permitted");
    obscura64_context_destroy(context); context = NULL;
    CHECK(obscura64_force_reinitialize(p.utf8, &context) == OBSCURA64_BUSY, "other process shared holder blocks mutator");
    CHECK(TerminateProcess(child.hProcess, 99) && WaitForSingleObject(child.hProcess, 10000) == WAIT_OBJECT_0, "terminate owned shared holder");
    CloseHandle(ready); CloseHandle(child.hThread); CloseHandle(child.hProcess);
    CHECK(test_spawn(&p, L"X", &child, &ready), "spawn separate exclusive holder");
    CHECK(WaitForSingleObject(ready, 10000) == WAIT_OBJECT_0, "child acquired exclusive kernel lock");
    CHECK(obscura64_open(p.utf8, &context) == OBSCURA64_BUSY && context == NULL, "child writer excludes parent open");
    CHECK(obscura64_force_reinitialize(p.utf8, &context) == OBSCURA64_BUSY, "child writer excludes parent Force");
    CHECK(TerminateProcess(child.hProcess, 99) && WaitForSingleObject(child.hProcess, 10000) == WAIT_OBJECT_0, "abrupt exclusive holder termination");
    CloseHandle(ready); CloseHandle(child.hThread); CloseHandle(child.hProcess);
    CHECK(GetFileAttributesW(p.lock) != INVALID_FILE_ATTRIBUTES, "crash leaves harmless coordination file");
    CHECK(obscura64_open(p.utf8, &context) == OBSCURA64_OK, "kernel authority released after crash");
    obscura64_context_destroy(context); context = NULL;
    CHECK(obscura64_force_reinitialize(p.utf8, &context) == OBSCURA64_OK, "Force works after crash");
    obscura64_context_destroy(context); context = NULL;
    CHECK(test_load(&p, &state, &history), "concurrent writers baseline");
    start = CreateEventW(NULL, TRUE, FALSE, NULL);
    CHECK(start != NULL, "writer barrier");
    memset(calls, 0, sizeof(calls));
    for (i = 0; i < 2; ++i) {
        calls[i].start = start; calls[i].path = p.utf8; calls[i].force = 1;
        threads[i] = CreateThread(NULL, 0, invoke, &calls[i], 0, NULL);
        CHECK(threads[i] != NULL, "concurrent writer thread");
    }
    SetEvent(start);
    CHECK(WaitForMultipleObjects(2, threads, TRUE, 10000) == WAIT_OBJECT_0, "writers finish");
    success = 0;
    for (i = 0; i < 2; ++i) {
        CHECK(calls[i].status == OBSCURA64_OK || calls[i].status == OBSCURA64_BUSY, "writer serialized or BUSY");
        success += calls[i].status == OBSCURA64_OK;
        CloseHandle(threads[i]);
    }
    CloseHandle(start);
    CHECK(success >= 1 && test_load(&p, &next, &history) && next.generation == state.generation + success &&
        memcmp(next.project_id, state.project_id, 16) == 0 &&
        obscura64_managed_state_validate(&next, &history, NULL) == OBSCURA64_OK, "no lost/duplicate concurrent generation or identity");
    /* A fresh directory has no coordination file yet. */
    wcscpy(fresh_path, p.project); wcscat(fresh_path, L"-fresh");
    CHECK(CreateDirectoryW(fresh_path, NULL) && test_bind(&fresh, fresh_path), "fresh first-init fixture");
    start = CreateEventW(NULL, TRUE, FALSE, NULL);
    CHECK(start != NULL, "first-init barrier");
    memset(calls, 0, sizeof(calls));
    for (i = 0; i < 2; ++i) {
        calls[i].start = start; calls[i].path = fresh.utf8;
        threads[i] = CreateThread(NULL, 0, invoke, &calls[i], 0, NULL);
        CHECK(threads[i] != NULL, "first-init callers");
    }
    SetEvent(start);
    CHECK(WaitForMultipleObjects(2, threads, TRUE, 10000) == WAIT_OBJECT_0, "first-init finishes");
    CHECK(calls[0].status == OBSCURA64_OK && calls[1].status == OBSCURA64_OK &&
        memcmp(calls[0].profile, calls[1].profile, 64) == 0, "first-init convergence");
    for (i = 0; i < 2; ++i) CloseHandle(threads[i]);
    CloseHandle(start);
    CHECK(test_bytes(fresh.current, after, 160, 0) &&
        obscura64_state_deserialize(after, 160, &next) == OBSCURA64_CORE_STATUS_SUCCESS && next.generation == 1,
        "one valid generation-one authoritative file");
    CHECK(test_no_temps(&p) && test_no_temps(&fresh), "no owned temp after concurrency");
    CHECK(test_cleanup(&p) && test_cleanup(&fresh), "cleanup owned fixtures");
    printf("PASS: %u locking checks\n", checks);
    return 0;
}
