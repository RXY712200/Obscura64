#include "obscura64_test_recovery_helpers.h"

static int fail_main;
int obscura64_test_fault(obscura64_test_fault_point point,const wchar_t *target,const wchar_t *temp)
{
    (void)temp;
    if (fail_main && point==OBSCURA64_FAULT_REPLACE && wcsstr(target,L"\\.obscura64\\current.state")!=NULL) {
        fail_main=0; return 1;
    }
    return 0;
}
static int reset(recovery_fixture *f,const obscura64_project_state *s,const obscura64_project_history *h)
{
    return put_current(f->current,s)&&put_history(f->history,h)&&
        put_current(f->backup.current,s)&&put_history(f->backup.history,h);
}
typedef struct opener { recovery_fixture *f; obscura64_status status; } opener;
static DWORD WINAPI concurrent_open(void *arg)
{
    opener *o=(opener *)arg;
    obscura64_context *c=NULL;
    o->status=obscura64_open(o->f->utf8,&c); obscura64_context_destroy(c); return 0;
}
int main(void)
{
    recovery_fixture f;
    obscura64_project_state one,two,three,base,s,b,foreign;
    obscura64_project_history h,h3,hbase,bh;
    obscura64_context *c=NULL;
    obscura64_lock *reader=NULL;
    unsigned char main_before[160],main_after[160],history_before[336],history_after[336];
    const unsigned char sample[]={0,1,2,0x80,0xff,37};
    char *encoded1=NULL,*encoded2=NULL;
    void *decoded=NULL;
    size_t n1=0,n2=0,dn=0;
    wchar_t stale[MAX_PATH*2];
    HANDLE threads[2],blocked;
    opener calls[2];
    CHECK(setup(&f),"isolated recovery root Unicode project");
    CHECK(obscura64_open(f.utf8,&c)==OBSCURA64_OK,"initialize generation one");
    CHECK(obscura64_snapshot_read_current(f.current,&one)==OBSCURA64_OK,"save generation one");
    CHECK(obscura64_encode_alloc(c,sample,sizeof(sample),&encoded1,&n1)==OBSCURA64_OK,"encode generation-one bytes");
    obscura64_context_destroy(c); c=NULL;
    CHECK(obscura64_force_reinitialize(f.utf8,&c)==OBSCURA64_OK,"Force generation two");
    CHECK(obscura64_encode_alloc(c,sample,sizeof(sample),&encoded2,&n2)==OBSCURA64_OK,"encode generation-two bytes");
    obscura64_context_destroy(c); c=NULL;
    CHECK(pair(f.current,f.history,&two,&h)&&raw_file(f.history,history_before,336,0),"save generation-two state");
    CHECK(poison(f.current),"corrupt current only");
    CHECK(obscura64_open(f.utf8,&c)==OBSCURA64_OK,"exact backup recovery succeeds");
    CHECK(obscura64_snapshot_read_current(f.current,&s)==OBSCURA64_OK&&same(&s,&two),"exact recovery preserves ID generation Profile");
    CHECK(obscura64_decode_alloc(c,encoded2,n2,&decoded,&dn)==OBSCURA64_OK&&dn==sizeof(sample)&&!memcmp(decoded,sample,dn),"exact recovery decodes previously encoded data");
    obscura64_free(decoded); decoded=NULL; obscura64_context_destroy(c); c=NULL;
    CHECK(DeleteFileW(f.current)&&open_ok(&f)&&obscura64_snapshot_read_current(f.current,&s)==OBSCURA64_OK&&same(&s,&two),"missing current restored exactly");
    CHECK(poison(f.backup.current)&&poison(f.backup.history)&&poison(f.current),"disable exact mirror keep local history");
    CHECK(obscura64_open(f.utf8,&c)==OBSCURA64_OK,"project history fallback");
    CHECK(pair(f.current,f.history,&s,&bh)&&same(&s,&one),"history rollback to stored generation one");
    CHECK(raw_file(f.history,history_after,336,0)&&!memcmp(history_before,history_after,336),"project history remains byte-for-byte unchanged");
    CHECK(obscura64_decode_alloc(c,encoded1,n1,&decoded,&dn)==OBSCURA64_OK&&dn==sizeof(sample)&&!memcmp(decoded,sample,dn),"rollback decodes original older Profile data");
    obscura64_free(decoded); decoded=NULL; obscura64_context_destroy(c); c=NULL;
    obscura64_free(encoded1); obscura64_free(encoded2);
    CHECK(reset(&f,&two,&h),"reset stored generation two");
    CHECK(force_ok(&f)&&pair(f.current,f.history,&three,&h3),"reach generation three");
    CHECK(force_ok(&f)&&pair(f.current,f.history,&base,&hbase)&&base.generation==4,"reach generation four");

    CHECK(DeleteFileW(f.history)&&open_ok(&f)&&pair(f.current,f.history,&s,&bh)&&same(&s,&base),"main-valid missing history repaired from exact backup");
    CHECK(poison(f.history)&&open_ok(&f)&&pair(f.current,f.history,&s,&bh)&&same(&s,&base),"main-valid corrupt history repaired");
    CHECK(reset(&f,&base,&hbase)&&put_current(f.backup.current,&three)&&put_history(f.backup.history,&h3)&&DeleteFileW(f.history),"stale-by-one mirror fixture");
    CHECK(open_ok(&f)&&pair(f.current,f.history,&s,&bh)&&same(&s,&base)&&bh.count==3&&bh.entries[0].generation==3&&bh.entries[1].generation==2&&bh.entries[2].generation==1,"reconstruct full N-1 N-2 N-3 window");
    CHECK(force_ok(&f),"Force succeeds after legitimate history reconstruction");
    CHECK(reset(&f,&base,&hbase)&&poison(f.history)&&poison(f.backup.history)&&poison(f.backup.current),"unrepairable history fixture");
    CHECK(raw_file(f.current,main_before,160,0)&&open_ok(&f),"valid current remains available without history or backup");
    CHECK(obscura64_force_reinitialize(f.utf8,&c)==OBSCURA64_UNRECOVERABLE&&c==NULL,"Force rejects missing mutation-safe history");
    CHECK(raw_file(f.current,main_after,160,0)&&!memcmp(main_before,main_after,160),"failed Force never invents Profile generation or identity");

    CHECK(reset(&f,&base,&hbase)&&poison(f.current)&&poison(f.history)&&poison(f.backup.current),"backup-history-only fixture");
    CHECK(open_ok(&f)&&pair(f.current,f.history,&s,&bh)&&same(&s,&three)&&bh.entries[0].generation==3,"redundant history promoted to valid overlap");
    CHECK(reset(&f,&base,&hbase)&&poison(f.current)&&poison(f.history)&&poison(f.backup.history),"exact backup current without usable history");
    CHECK(open_ok(&f)&&obscura64_snapshot_read_current(f.current,&s)==OBSCURA64_OK&&same(&s,&base),"partial exact-current recovery succeeds for runtime");
    CHECK(obscura64_force_reinitialize(f.utf8,&c)==OBSCURA64_UNRECOVERABLE&&c==NULL,"partial recovery does not fabricate mutation history");

    CHECK(reset(&f,&base,&hbase),"reset identity conflict fixture");
    foreign=base; foreign.project_id[0]^=0x55; make_history_for(&bh,&foreign,0);
    CHECK(put_current(f.backup.current,&foreign)&&put_history(f.backup.history,&bh)&&poison(f.current),"foreign mirror with valid project history anchor");
    CHECK(open_ok(&f)&&pair(f.current,f.history,&s,&bh)&&same(&s,&three),"project identity anchor rejects foreign exact backup and uses local history");
    CHECK(reset(&f,&base,&hbase)&&poison(f.current)&&poison(f.history)&&put_current(f.backup.current,&foreign),"ambiguous external identities without local anchor");
    CHECK(obscura64_open(f.utf8,&c)==OBSCURA64_UNRECOVERABLE&&c==NULL,"ambiguous identity never guessed");
    CHECK(reset(&f,&base,&hbase),"reset noncanonical history fixture");
    bh=hbase; bh.entries[1].generation=1; bh.count=2; memset(&bh.entries[2],0,sizeof(bh.entries[2]));
    CHECK(put_history(f.history,&bh)&&poison(f.current)&&poison(f.backup.current)&&poison(f.backup.history),"generic-valid incomplete history");
    CHECK(obscura64_open(f.utf8,&c)==OBSCURA64_UNRECOVERABLE&&c==NULL,"noncanonical history cannot be promoted");

    CHECK(reset(&f,&base,&hbase)&&poison(f.current)&&poison(f.history)&&poison(f.backup.current)&&poison(f.backup.history),"all named candidates invalid");
    wcscpy(stale,f.current); wcscat(stale,L".tmp.valid"); CHECK(put_current(stale,&base),"valid stale temp is not recovery authority");
    CHECK(obscura64_open(f.utf8,&c)==OBSCURA64_UNRECOVERABLE&&c==NULL,"no candidate returns explicit UNRECOVERABLE");
    CHECK(obscura64_snapshot_read_current(f.current,&s)==OBSCURA64_STATE_CORRUPT,"failed recovery preserves corrupt evidence");
    CHECK(DeleteFileW(stale),"remove only test sentinel");
    CHECK(reset(&f,&base,&hbase)&&poison(f.current),"reset lock conflict recovery");
    CHECK(obscura64_lock_acquire_shared(f.lock,&reader)==OBSCURA64_OK,"active reader during recovery");
    CHECK(obscura64_open(f.utf8,&c)==OBSCURA64_BUSY&&c==NULL,"recovery exclusive upgrade conflicts with reader");
    CHECK(obscura64_force_reinitialize(f.utf8,&c)==OBSCURA64_BUSY&&c==NULL,"Force recovery also honors lock");
    CHECK(obscura64_lock_release(reader)==OBSCURA64_OK&&open_ok(&f),"release allows recovery"); reader=NULL;
    CHECK(poison(f.current),"two concurrent recoverers fixture");
    calls[0].f=&f; calls[1].f=&f;
    threads[0]=CreateThread(NULL,0,concurrent_open,&calls[0],0,NULL);
    threads[1]=CreateThread(NULL,0,concurrent_open,&calls[1],0,NULL);
    CHECK(threads[0]&&threads[1]&&WaitForMultipleObjects(2,threads,TRUE,10000)==WAIT_OBJECT_0,"concurrent recovery threads complete");
    CloseHandle(threads[0]); CloseHandle(threads[1]);
    CHECK((calls[0].status==OBSCURA64_OK||calls[0].status==OBSCURA64_BUSY)&&
        (calls[1].status==OBSCURA64_OK||calls[1].status==OBSCURA64_BUSY),"concurrent results obey nonblocking policy");
    CHECK(open_ok(&f)&&pair(f.current,f.history,&s,&bh)&&same(&s,&base),"retry loads one consistent repaired state");
    CHECK(poison(f.current),"recovery write failure fixture"); fail_main=1;
    CHECK(obscura64_open(f.utf8,&c)==OBSCURA64_IO_ERROR&&c==NULL&&fail_main==0,"recovery replacement failure propagated");
    CHECK(obscura64_snapshot_read_current(f.current,&s)==OBSCURA64_STATE_CORRUPT&&open_ok(&f),"failed replacement preserves evidence and lock released for retry");
    blocked=CreateFileW(f.current,GENERIC_READ,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    CHECK(blocked!=INVALID_HANDLE_VALUE,"deny actual primary file access");
    CHECK(obscura64_open(f.utf8,&c)==OBSCURA64_IO_ERROR&&c==NULL,"ordinary filesystem error is not UNRECOVERABLE");
    CloseHandle(blocked);
    CHECK(reset(&f,&base,&hbase)&&poison(f.current)&&poison(f.history)&&poison(f.backup.history),"prepare inaccessible backup with no history candidate");
    blocked=CreateFileW(f.backup.current,GENERIC_READ,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    CHECK(blocked!=INVALID_HANDLE_VALUE&&obscura64_open(f.utf8,&c)==OBSCURA64_IO_ERROR&&c==NULL,"backup access failure is not misreported as unrecoverable absence");
    CloseHandle(blocked);
    CHECK(reset(&f,&base,&hbase),"restore owned named state after access-denial test");
    CHECK(poison(f.current)&&force_ok(&f)&&pair(f.current,f.history,&s,&bh)&&s.generation==5,"Force recovers exact state then increments once");

    CHECK(remove_test_tree(f.directory,f.root),"remove owned state container only for path reuse");
    CHECK(open_ok(&f)&&obscura64_snapshot_read_current(f.current,&s)==OBSCURA64_OK&&s.generation==1&&memcmp(s.project_id,base.project_id,16),"fresh path does not resurrect stale backup");
    CHECK(pair(f.backup.current,f.backup.history,&b,&bh)&&same(&b,&s)&&bh.count==0,"new identity replaces stale path mirror");
    CHECK(cleanup(&f),"cleanup isolated recovery and backup root");
    printf("PASS: %u recovery checks\n",checks); return 0;
}
