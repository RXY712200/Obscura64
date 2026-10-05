#ifndef OBSCURA64_TEST_RECOVERY_HELPERS_H
#define OBSCURA64_TEST_RECOVERY_HELPERS_H
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "obscura64_internal.h"
#include "obscura64_profiles_v1.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

static unsigned int checks;
#define CHECK(c,label) do { ++checks; if (!(c)) { fprintf(stderr,"FAIL: %s line %d\n",label,__LINE__); return 1; } } while(0)
typedef struct recovery_fixture {
    wchar_t root[MAX_PATH*2], project[MAX_PATH*2], directory[MAX_PATH*2];
    wchar_t current[MAX_PATH*2], history[MAX_PATH*2], lock[MAX_PATH*2];
    char utf8[MAX_PATH*6];
    obscura64_snapshot_paths backup;
} recovery_fixture;

static inline int bind_fixture(recovery_fixture *f)
{
    wcscpy(f->directory,f->project); wcscat(f->directory,L"\\.obscura64");
    wcscpy(f->current,f->directory); wcscat(f->current,L"\\current.state");
    wcscpy(f->history,f->directory); wcscat(f->history,L"\\history.state");
    wcscpy(f->lock,f->directory); wcscat(f->lock,L"\\operation.lock");
    return WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,f->project,-1,f->utf8,sizeof(f->utf8),NULL,NULL) != 0;
}
static inline int setup(recovery_fixture *f)
{
    wchar_t temp[MAX_PATH],seed[MAX_PATH];
    memset(f,0,sizeof(*f));
    if (!GetTempPathW(MAX_PATH,temp) || !GetTempFileNameW(temp,L"o6",0,seed) || !DeleteFileW(seed)) return 0;
    wcscpy(f->root,seed); wcscat(f->root,L"-stage6");
    if (!CreateDirectoryW(f->root,NULL) || !SetEnvironmentVariableW(L"OBSCURA64_TEST_BACKUP_ROOT",f->root)) return 0;
    wcscpy(f->project,f->root); wcscat(f->project,L"\\\x9879\x76EE");
    return CreateDirectoryW(f->project,NULL) && bind_fixture(f) &&
        obscura64_redundancy_paths(f->project,0,&f->backup) == OBSCURA64_OK;
}
static inline int raw_file(const wchar_t *p, unsigned char *bytes, DWORD size, int write)
{
    HANDLE h = CreateFileW(p,write ? GENERIC_WRITE : GENERIC_READ,FILE_SHARE_READ,NULL,
        write ? CREATE_ALWAYS : OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    DWORD count = 0;
    LARGE_INTEGER n;
    int ok;
    if (h == INVALID_HANDLE_VALUE) return 0;
    ok = write ? WriteFile(h,bytes,size,&count,NULL) :
        GetFileSizeEx(h,&n) && n.QuadPart == size && ReadFile(h,bytes,size,&count,NULL);
    if (write && ok) ok = FlushFileBuffers(h);
    if (!CloseHandle(h)) ok = 0;
    return ok && count == size;
}
static inline int put_current(const wchar_t *p,const obscura64_project_state *s)
{
    unsigned char b[160];
    return obscura64_state_serialize(s,b) == OBSCURA64_CORE_STATUS_SUCCESS && raw_file(p,b,160,1);
}
static inline int put_history(const wchar_t *p,const obscura64_project_history *h)
{
    unsigned char b[336];
    return obscura64_history_serialize(h,b) == OBSCURA64_CORE_STATUS_SUCCESS && raw_file(p,b,336,1);
}
static inline int poison(const wchar_t *p)
{ unsigned char b[3] = {1,2,3}; return raw_file(p,b,3,1); }
static inline int open_ok(recovery_fixture *f)
{
    obscura64_context *c = NULL;
    obscura64_status s = obscura64_open(f->utf8,&c);
    obscura64_context_destroy(c);
    return s == OBSCURA64_OK;
}
static inline int force_ok(recovery_fixture *f)
{
    obscura64_context *c = NULL;
    obscura64_status s = obscura64_force_reinitialize(f->utf8,&c);
    obscura64_context_destroy(c);
    return s == OBSCURA64_OK;
}
static inline int pair(const wchar_t *cp,const wchar_t *hp,obscura64_project_state *s,obscura64_project_history *h)
{
    return obscura64_snapshot_read_current(cp,s) == OBSCURA64_OK &&
        obscura64_snapshot_read_history(hp,h) == OBSCURA64_OK &&
        obscura64_managed_state_validate(s,h,NULL) == OBSCURA64_OK;
}
static inline int same(const obscura64_project_state *a,const obscura64_project_state *b)
{ return a->generation == b->generation && memcmp(a->project_id,b->project_id,16) == 0 && memcmp(a->profile,b->profile,64) == 0; }
static inline void make_history_for(obscura64_project_history *h,const obscura64_project_state *s,int overlap)
{
    uint64_t newest = s->generation - (overlap ? 0U : 1U);
    size_t i;
    memset(h,0,sizeof(*h)); memcpy(h->project_id,s->project_id,16);
    h->count = newest < 3 ? (uint16_t)newest : 3;
    for (i = 0; i < h->count; ++i) {
        h->entries[i].generation = newest-i;
        memcpy(h->entries[i].profile,obscura64_profiles_v1[i+1U],64);
    }
    if (overlap && h->count) memcpy(h->entries[0].profile,s->profile,64);
}
/* Recursive deletion is restricted to the freshly created test root. Never
   follows reparse points and never receives real LocalAppData paths. */
static inline int remove_test_tree(const wchar_t *path,const wchar_t *root)
{
    wchar_t pattern[MAX_PATH*3],child[MAX_PATH*3];
    WIN32_FIND_DATAW data;
    HANDLE find;
    size_t n=wcslen(root);
    unsigned int retry;
    if (wcsncmp(path,root,n) != 0 || (path[n] != 0 && path[n] != L'\\')) return 0;
    wcscpy(pattern,path); wcscat(pattern,L"\\*");
    find=FindFirstFileW(pattern,&data);
    if (find != INVALID_HANDLE_VALUE) {
        do {
            if (!wcscmp(data.cFileName,L".") || !wcscmp(data.cFileName,L"..")) continue;
            wcscpy(child,path); wcscat(child,L"\\"); wcscat(child,data.cFileName);
            if (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) { FindClose(find); return 0; }
            if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                if (!remove_test_tree(child,root)) { FindClose(find); return 0; }
            } else if (!DeleteFileW(child) && GetLastError() != ERROR_FILE_NOT_FOUND) { FindClose(find); return 0; }
        } while (FindNextFileW(find,&data));
        FindClose(find);
    }
    for (retry=0; retry<25; ++retry) {
        if (RemoveDirectoryW(path)) return 1;
        if (GetLastError() != ERROR_DIR_NOT_EMPTY) return 0;
        Sleep(10);
    }
    return 0;
}
static inline int cleanup(recovery_fixture *f)
{
    obscura64_snapshot_paths_free(&f->backup);
    SetEnvironmentVariableW(L"OBSCURA64_TEST_BACKUP_ROOT",NULL);
    return remove_test_tree(f->root,f->root);
}
#endif
