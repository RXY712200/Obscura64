#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "obscura64_internal.h"
#include <stdlib.h>

struct obscura64_lock { HANDLE file; OVERLAPPED range; DWORD length; };

static obscura64_status acquire(const wchar_t *path, int exclusive, DWORD offset,
                                DWORD length, obscura64_lock **out)
{
    obscura64_lock *lock;
    DWORD error;
    if (out == NULL) return OBSCURA64_INVALID_ARGUMENT;
    *out = NULL;
    if (path == NULL || path[0] == L'\0') return OBSCURA64_INVALID_ARGUMENT;
    lock = (obscura64_lock *)calloc(1, sizeof(*lock));
    if (lock == NULL) return OBSCURA64_OUT_OF_MEMORY;
    lock->range.Offset = offset;
    lock->length = length;
    /* Deny deletion while handles exist so a second lock object cannot replace it. */
    lock->file = CreateFileW(path, GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (lock->file == INVALID_HANDLE_VALUE) { free(lock); return OBSCURA64_IO_ERROR; }
    if (!LockFileEx(lock->file, LOCKFILE_FAIL_IMMEDIATELY |
        (exclusive ? LOCKFILE_EXCLUSIVE_LOCK : 0), 0, length, 0, &lock->range)) {
        error = GetLastError();
        CloseHandle(lock->file); free(lock);
        return error == ERROR_LOCK_VIOLATION ? OBSCURA64_BUSY : OBSCURA64_IO_ERROR;
    }
    *out = lock;
    return OBSCURA64_OK;
}

obscura64_status obscura64_lock_acquire_shared(const wchar_t *path, obscura64_lock **lock)
{ return acquire(path, 0, 0, 1, lock); }

obscura64_status obscura64_lock_acquire_exclusive(const wchar_t *path, obscura64_lock **lock)
{ return acquire(path, 1, 0, 1, lock); }

obscura64_status obscura64_lock_acquire_initial(const wchar_t *path, obscura64_lock **lock)
{ return acquire(path, 1, 0, 2, lock); }

obscura64_status obscura64_lock_initialization_active(const wchar_t *path, int *active)
{
    obscura64_lock *probe = NULL;
    obscura64_status status;
    if (active == NULL) return OBSCURA64_INVALID_ARGUMENT;
    status = acquire(path, 1, 1, 1, &probe);
    if (status == OBSCURA64_BUSY) { *active = 1; return OBSCURA64_OK; }
    if (status != OBSCURA64_OK) return status;
    status = obscura64_lock_release(probe);
    if (status == OBSCURA64_OK) *active = 0;
    return status;
}

obscura64_status obscura64_lock_release(obscura64_lock *lock)
{
    int ok;
    if (lock == NULL) return OBSCURA64_INVALID_ARGUMENT;
    ok = UnlockFileEx(lock->file, 0, lock->length, 0, &lock->range) != 0;
    if (!CloseHandle(lock->file)) ok = 0;
    free(lock);
    return ok ? OBSCURA64_OK : OBSCURA64_IO_ERROR;
}
