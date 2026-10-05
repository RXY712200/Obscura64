#include "obscura64_test_recovery_helpers.h"

static int fail_point = -1;
static int fail_history;
int obscura64_test_fault(obscura64_test_fault_point point,const wchar_t *target,const wchar_t *temp)
{
    (void)temp;
    if ((int)point == fail_point && wcsstr(target,L"\\Obscura64\\Projects\\") != NULL &&
        wcsstr(target,fail_history ? L"history.state" : L"current.state") != NULL) { fail_point=-1; return 1; }
    return 0;
}
int main(void)
{
    recovery_fixture f;
    obscura64_project_state s,b,after,foreign;
    obscura64_project_history h,bh;
    obscura64_snapshot_paths variant={0},moved={0};
    unsigned char before[160],bytes[160];
    wchar_t trailing[MAX_PATH*2],newpath[MAX_PATH*2],newdir[MAX_PATH*2],target[MAX_PATH*2];
    obscura64_context *c=NULL;
    obscura64_lock *reader=NULL;
    size_t i;
    CHECK(setup(&f),"isolated Unicode fixture");
    CHECK(open_ok(&f),"fresh initialization");
    CHECK(obscura64_snapshot_read_current(f.current,&s)==OBSCURA64_OK && s.generation==1,"fresh generation one");
    CHECK(pair(f.backup.current,f.backup.history,&b,&bh) && same(&s,&b) && bh.count==0,"generation one self-contained backup");
    CHECK(memcmp(s.project_id,bh.project_id,16)==0,"empty backup history identity");
    CHECK(wcslen(wcsrchr(f.backup.directory,L'\\')+1)==64,"locator 64 characters");
    for(i=0;i<64;++i) { wchar_t x=(wcsrchr(f.backup.directory,L'\\')+1)[i]; CHECK((x>=L'0'&&x<=L'9')||(x>=L'a'&&x<=L'f'),"lowercase hex locator"); }
    wcscpy(trailing,f.project); wcscat(trailing,L"\\");
    CHECK(obscura64_redundancy_paths(trailing,0,&variant)==OBSCURA64_OK && !wcscmp(variant.directory,f.backup.directory),"trailing separator locator stable");
    obscura64_snapshot_paths_free(&variant);
    CHECK(raw_file(f.current,before,160,0),"snapshot main bytes");
    CHECK(DeleteFileW(f.backup.current)&&DeleteFileW(f.backup.history),"delete only test backup");
    CHECK(open_ok(&f)&&pair(f.backup.current,f.backup.history,&b,&bh)&&same(&b,&s),"missing backup regenerated");
    CHECK(raw_file(f.current,bytes,160,0)&&!memcmp(before,bytes,160),"main unchanged during sync");
    CHECK(poison(f.backup.current)&&poison(f.backup.history),"corrupt redundant pair");
    CHECK(open_ok(&f)&&pair(f.backup.current,f.backup.history,&b,&bh)&&same(&b,&s),"corrupt backup repaired");
    foreign=s; foreign.project_id[0]^=0x5a; foreign.generation=9; make_history_for(&h,&foreign,0);
    CHECK(put_current(f.backup.current,&foreign)&&put_history(f.backup.history,&h),"foreign newer backup fixture");
    CHECK(open_ok(&f)&&pair(f.backup.current,f.backup.history,&b,&bh)&&same(&b,&s),"valid main beats foreign newer backup");
    foreign=s; foreign.generation=10; make_history_for(&h,&foreign,0);
    CHECK(put_current(f.backup.current,&foreign)&&put_history(f.backup.history,&h),"same-ID newer backup fixture");
    CHECK(open_ok(&f)&&pair(f.backup.current,f.backup.history,&b,&bh)&&same(&b,&s),"valid main beats same-ID newer backup");
    CHECK(obscura64_lock_acquire_shared(f.lock,&reader)==OBSCURA64_OK,"hold shared reader");
    CHECK(poison(f.backup.current),"poison backup under reader");
    CHECK(open_ok(&f)&&obscura64_snapshot_read_current(f.backup.current,&b)==OBSCURA64_STATE_CORRUPT,"maintenance skips conflicting reader without failing open");
    CHECK(obscura64_lock_release(reader)==OBSCURA64_OK,"release reader"); reader=NULL;
    CHECK(open_ok(&f)&&pair(f.backup.current,f.backup.history,&b,&bh),"subsequent maintenance succeeds");
    CHECK(force_ok(&f)&&pair(f.current,f.history,&s,&h),"generation two primary and backup");
    CHECK(pair(f.backup.current,f.backup.history,&b,&bh)&&same(&s,&b),"Force snapshot synchronized");
    fail_point=OBSCURA64_FAULT_REPLACE;
    CHECK(force_ok(&f)&&fail_point==-1,"backup current replacement failure does not fail Force");
    CHECK(pair(f.current,f.history,&after,&h)&&after.generation==3&&!memcmp(s.project_id,after.project_id,16),"authoritative Force stays committed");
    CHECK(pair(f.backup.current,f.backup.history,&b,&bh)&&b.generation==2&&bh.entries[0].generation==2,"backup remains safe overlap");
    CHECK(open_ok(&f)&&pair(f.backup.current,f.backup.history,&b,&bh)&&same(&b,&after)&&bh.entries[0].generation==2,"maintenance normalizes overlap to main");
    fail_history=1; fail_point=OBSCURA64_FAULT_TEMP_FLUSH;
    CHECK(force_ok(&f)&&fail_point==-1,"backup history flush failure cannot roll back primary Force");
    CHECK(pair(f.current,f.history,&after,&h)&&after.generation==4,"primary managed state stays committed after mirror history failure");
    CHECK(open_ok(&f)&&pair(f.backup.current,f.backup.history,&b,&bh)&&same(&b,&after),"later maintenance repairs failed mirror history sync");
    CHECK(raw_file(f.current,before,160,0)&&SetEnvironmentVariableW(L"OBSCURA64_TEST_BACKUP_ROOT",f.current),"make test-only backup root an unavailable regular file");
    CHECK(open_ok(&f)&&raw_file(f.current,bytes,160,0)&&!memcmp(before,bytes,160),"unavailable backup root does not fail normal open or alter current");
    CHECK(SetEnvironmentVariableW(L"OBSCURA64_TEST_BACKUP_ROOT",f.root)&&open_ok(&f),"restore isolated root and maintain normally");

    wcscpy(newpath,f.root); wcscat(newpath,L"\\copied"); CHECK(CreateDirectoryW(newpath,NULL),"copy project directory");
    wcscpy(newdir,newpath); wcscat(newdir,L"\\.obscura64"); CHECK(CreateDirectoryW(newdir,NULL),"copy state directory");
    wcscpy(target,newdir); wcscat(target,L"\\current.state"); CHECK(CopyFileW(f.current,target,TRUE),"copy current");
    wcscpy(target,newdir); wcscat(target,L"\\history.state"); CHECK(CopyFileW(f.history,target,TRUE),"copy history");
    CHECK(obscura64_redundancy_paths(newpath,0,&moved)==OBSCURA64_OK&&wcscmp(moved.directory,f.backup.directory),"copy obtains new locator");
    CHECK(GetFileAttributesW(moved.current)==INVALID_FILE_ATTRIBUTES,"new location has no backup initially");
    CHECK(bind_fixture(&f),"original path remains bound");
    {
        char utf8[MAX_PATH*6];
        CHECK(WideCharToMultiByte(CP_UTF8,0,newpath,-1,utf8,sizeof(utf8),NULL,NULL),"copied UTF-8 path");
        CHECK(obscura64_open(utf8,&c)==OBSCURA64_OK,"open copied project"); obscura64_context_destroy(c); c=NULL;
    }
    CHECK(pair(moved.current,moved.history,&b,&bh)&&same(&b,&after),"copy preserves identity generation Profile and creates mirror");
    obscura64_snapshot_paths_free(&moved);
    CHECK(cleanup(&f),"cleanup only isolated test root");
    printf("PASS: %u redundancy checks\n",checks); return 0;
}
