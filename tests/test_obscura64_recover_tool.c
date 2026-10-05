#include "obscura64_test_recovery_helpers.h"

static int run(const wchar_t *exe,const wchar_t *input,const wchar_t *output,DWORD *code,char log[512])
{
    SECURITY_ATTRIBUTES sa={sizeof(sa),NULL,TRUE};
    HANDLE read=NULL,write=NULL;
    STARTUPINFOW si={0}; PROCESS_INFORMATION pi={0};
    wchar_t command[MAX_PATH*8];
    DWORD got=0;
    int ok;
    if(!CreatePipe(&read,&write,&sa,0)) return 0;
    if(!SetHandleInformation(read,HANDLE_FLAG_INHERIT,0)) {CloseHandle(read);CloseHandle(write);return 0;}
    swprintf(command,MAX_PATH*8,L"\"%ls\" \"%ls\" \"%ls\"",exe,input,output);
    si.cb=sizeof(si); si.dwFlags=STARTF_USESTDHANDLES; si.hStdOutput=write; si.hStdError=write; si.hStdInput=GetStdHandle(STD_INPUT_HANDLE);
    ok=CreateProcessW(NULL,command,NULL,NULL,TRUE,CREATE_NO_WINDOW,NULL,NULL,&si,&pi)!=0;
    CloseHandle(write);
    if(!ok) {CloseHandle(read);return 0;}
    ok=WaitForSingleObject(pi.hProcess,30000)==WAIT_OBJECT_0;
    if(!ok) {TerminateProcess(pi.hProcess,99);WaitForSingleObject(pi.hProcess,10000);}
    if(ok) ok=GetExitCodeProcess(pi.hProcess,code)!=0;
    if(!ReadFile(read,log,511,&got,NULL)) got=0;
    log[got]=0; CloseHandle(read);CloseHandle(pi.hThread);CloseHandle(pi.hProcess);return ok;
}
int main(int argc,char **argv)
{
    recovery_fixture f;
    wchar_t exe[MAX_PATH*3],input[MAX_PATH*3],output[MAX_PATH*3],empty_output[MAX_PATH*3];
    const unsigned char bytes[]={0,'A',0x80,0xff,13,10};
    unsigned char got[sizeof(bytes)];
    obscura64_context *c=NULL;
    char *encoded=NULL,log[512]; size_t n; DWORD code;
    CHECK(argc==2&&MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,argv[1],-1,exe,MAX_PATH*3),"CLI executable path");
    CHECK(setup(&f),"isolated Unicode file fixture");
    wcscpy(input,f.project);wcscat(input,L"\\\x8F93\x5165.txt");
    wcscpy(output,f.project);wcscat(output,L"\\\x8F93\x51FA.bin");
    wcscpy(empty_output,f.project);wcscat(empty_output,L"\\empty.bin");
    CHECK(obscura64_context_create_from_profile((const char *)obscura64_profiles_v1[1],64,&c)==OBSCURA64_OK,"encoding Profile one");
    CHECK(obscura64_managed_encode_alloc(c,bytes,sizeof(bytes),&encoded,&n)==OBSCURA64_OK&&raw_file(input,(unsigned char *)encoded,(DWORD)n,1),"write encoded input exactly");
    CHECK(run(exe,input,output,&code,log)&&code==0&&
        (strstr(log,"Recovered Profile ID: 1\n")||strstr(log,"Recovered Profile ID: 1\r\n")),"real CLI unique success and ID report");
    CHECK(strstr(log,"Profile: ")&&!memcmp(strstr(log,"Profile: ")+9,obscura64_profiles_v1[1],64),"CLI reports exact frozen Profile");
    CHECK(raw_file(output,got,sizeof(got),0)&&!memcmp(got,bytes,sizeof(bytes)),"CLI exact binary output");
    CHECK(GetFileAttributesW(f.directory)==INVALID_FILE_ATTRIBUTES,"CLI never creates project state");
    CHECK(run(exe,input,output,&code,log)&&code!=0&&raw_file(output,got,sizeof(got),0)&&!memcmp(got,bytes,sizeof(bytes)),"existing output never overwritten");
    CHECK(poison(input)&&run(exe,input,empty_output,&code,log)&&code!=0&&strstr(log,"no matching Profile")&&GetFileAttributesW(empty_output)==INVALID_FILE_ATTRIBUTES,"no match no output");
    CHECK(raw_file(input,(unsigned char *)encoded,(DWORD)n,1),"restore input");
    {
        char *space=(char *)malloc(n+1); CHECK(space!=NULL,"whitespace input fixture");
        memcpy(space,encoded,n);space[n]='\n';
        CHECK(raw_file(input,(unsigned char *)space,(DWORD)n+1,1)&&run(exe,input,empty_output,&code,log)&&code!=0,"CLI never trims whitespace");free(space);
    }
    obscura64_free(encoded);encoded=NULL;
    CHECK(obscura64_managed_encode_alloc(c,NULL,0,&encoded,&n)==OBSCURA64_OK&&raw_file(input,(unsigned char *)encoded,(DWORD)n,1),"empty Managed Payload input");
    CHECK(run(exe,input,empty_output,&code,log)&&code==0,"empty payload CLI success");
    {
        HANDLE h=CreateFileW(empty_output,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
        LARGE_INTEGER size;
        CHECK(h!=INVALID_HANDLE_VALUE&&GetFileSizeEx(h,&size)&&size.QuadPart==0,"zero-byte output file");CloseHandle(h);
    }
    obscura64_context_destroy(c);obscura64_free(encoded);
    CHECK(cleanup(&f),"cleanup owned tool fixtures");
    printf("PASS: %u recover tool checks\n",checks);return 0;
}
