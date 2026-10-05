#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <bcrypt.h>
#include "obscura64_internal.h"
#include <stdlib.h>
#include <string.h>

static wchar_t *join(const wchar_t *base, const wchar_t *name)
{
    size_t a = wcslen(base), b = wcslen(name);
    wchar_t *p;
    if (a > SIZE_MAX / sizeof(*p) - b - 2U) return NULL;
    p = (wchar_t *)malloc((a + b + 2U) * sizeof(*p));
    if (p != NULL) { wcscpy(p, base); if (a && p[a-1] != L'\\') wcscat(p, L"\\"); wcscat(p, name); }
    return p;
}

void obscura64_snapshot_paths_free(obscura64_snapshot_paths *p)
{
    if (p == NULL) return;
    free(p->directory); free(p->current); free(p->history);
    memset(p, 0, sizeof(*p));
}

static int directory(const wchar_t *path)
{
    DWORD attr;
    if (CreateDirectoryW(path, NULL)) return 1;
    if (GetLastError() != ERROR_ALREADY_EXISTS) return 0;
    attr = GetFileAttributesW(path);
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

static obscura64_status locator(const wchar_t *project, wchar_t hex[65])
{
    HANDLE file;
    DWORD needed, length;
    wchar_t *resolved;
    BCRYPT_ALG_HANDLE alg = NULL;
    BCRYPT_HASH_HANDLE hash = NULL;
    unsigned char digest[32];
    static const wchar_t digits[] = L"0123456789abcdef";
    obscura64_status status = OBSCURA64_IO_ERROR;
    size_t i;
    file = CreateFileW(project, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (file == INVALID_HANDLE_VALUE) return status;
    needed = GetFinalPathNameByHandleW(file, NULL, 0, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    if (needed == 0) { CloseHandle(file); return status; }
    resolved = (wchar_t *)malloc(((size_t)needed + 1U) * sizeof(*resolved));
    if (resolved == NULL) { CloseHandle(file); return OBSCURA64_OUT_OF_MEMORY; }
    length = GetFinalPathNameByHandleW(file, resolved, needed + 1U, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    if (!CloseHandle(file) || length == 0 || length > needed) goto done;
    /* Handle resolution removes caller spelling and trailing separators. Preserve
       resolved casing: case-sensitive Windows directories must remain distinct. */
    if (length > MAXDWORD / sizeof(wchar_t)) goto done;
    if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, NULL, 0)) ||
        !BCRYPT_SUCCESS(BCryptCreateHash(alg, &hash, NULL, 0, NULL, 0, 0)) ||
        !BCRYPT_SUCCESS(BCryptHashData(hash, (PUCHAR)resolved, length * sizeof(wchar_t), 0)) ||
        !BCRYPT_SUCCESS(BCryptFinishHash(hash, digest, sizeof(digest), 0))) goto done;
    for (i = 0; i < 32; ++i) { hex[2*i] = digits[digest[i] >> 4]; hex[2*i+1] = digits[digest[i] & 15]; }
    hex[64] = 0; status = OBSCURA64_OK;
done:
    if (hash != NULL) BCryptDestroyHash(hash);
    if (alg != NULL) BCryptCloseAlgorithmProvider(alg, 0);
    free(resolved);
    return status;
}

obscura64_status obscura64_redundancy_paths(const wchar_t *project,
    int create, obscura64_snapshot_paths *out)
{
    wchar_t hex[65], *root = NULL, *brand = NULL, *projects = NULL;
    obscura64_snapshot_paths p = {0};
    obscura64_status status;
    if (project == NULL || out == NULL) return OBSCURA64_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    status = locator(project, hex);
    if (status != OBSCURA64_OK) return status;
#ifdef OBSCURA64_REDUNDANCY_TESTING
    {
        DWORD size = GetEnvironmentVariableW(L"OBSCURA64_TEST_BACKUP_ROOT", NULL, 0);
        DWORD copied;
        if (size == 0) return OBSCURA64_IO_ERROR; /* Never fall through to real user storage. */
        root = (wchar_t *)malloc((size_t)size * sizeof(*root));
        if (root == NULL) return OBSCURA64_OUT_OF_MEMORY;
        copied = GetEnvironmentVariableW(L"OBSCURA64_TEST_BACKUP_ROOT", root, size);
        if (copied == 0 || copied >= size) { free(root); return OBSCURA64_IO_ERROR; }
    }
#else
    {
        PWSTR known = NULL;
        if (FAILED(SHGetKnownFolderPath(&FOLDERID_LocalAppData, 0, NULL, &known))) return OBSCURA64_IO_ERROR;
        root = join(known, L"");
        CoTaskMemFree(known);
        if (root == NULL) return OBSCURA64_OUT_OF_MEMORY;
    }
#endif
    status = OBSCURA64_OUT_OF_MEMORY;
    brand = join(root, L"Obscura64");
    if (brand != NULL) projects = join(brand, L"Projects");
    if (projects != NULL) p.directory = join(projects, hex);
    if (p.directory != NULL) { p.current = join(p.directory, L"current.state"); p.history = join(p.directory, L"history.state"); }
    if (p.current == NULL || p.history == NULL) goto done;
    status = OBSCURA64_IO_ERROR;
    if (create && (!directory(root) || !directory(brand) || !directory(projects) || !directory(p.directory))) goto done;
    *out = p; memset(&p, 0, sizeof(p)); status = OBSCURA64_OK;
done:
    obscura64_snapshot_paths_free(&p); free(projects); free(brand); free(root);
    return status;
}

obscura64_status obscura64_redundancy_sync(const wchar_t *project,
    const obscura64_project_state *state, const obscura64_project_history *history)
{
    obscura64_snapshot_paths p;
    obscura64_project_history empty;
    obscura64_status status;
    if (obscura64_managed_state_validate(state, history, NULL) != OBSCURA64_OK)
        return OBSCURA64_STATE_CORRUPT;
    if (history == NULL) {
        memset(&empty, 0, sizeof(empty)); memcpy(empty.project_id, state->project_id, 16); history = &empty;
    }
    status = obscura64_redundancy_paths(project, 1, &p);
    if (status != OBSCURA64_OK) return status;
    status = obscura64_snapshot_write_history(p.history, history);
    if (status == OBSCURA64_OK) status = obscura64_snapshot_write_current(p.current, state);
    obscura64_snapshot_paths_free(&p);
    return status;
}
