#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "obscura64_internal.h"
#include <stdio.h>
#include <stdlib.h>

int wmain(int argc,wchar_t **argv)
{
    HANDLE input=INVALID_HANDLE_VALUE,output=INVALID_HANDLE_VALUE;
    LARGE_INTEGER length;
    char *encoded=NULL;
    size_t size=0,pos=0;
    obscura64_disaster_result result={0};
    obscura64_disaster_status status;
    int exit_code=1,owned_output=0;
    if(argc!=3) {
        fprintf(stderr,"usage: obscura64_recover <encoded-input-file> <new-decoded-output-file>\n"
            "Builtin codec Managed Payload only; custom Providers are unsupported.\n"
            "Recovers payload/Profile, never project identity or state. No whitespace trimming.\n");
        return 2;
    }
    input=CreateFileW(argv[1],GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(input==INVALID_HANDLE_VALUE || !GetFileSizeEx(input,&length) || length.QuadPart<0 ||
        (uint64_t)length.QuadPart>SIZE_MAX) { fprintf(stderr,"cannot read input or input too large\n"); goto done; }
    size=(size_t)length.QuadPart;
    if(size) { encoded=(char *)malloc(size); if(!encoded) { fprintf(stderr,"out of memory\n"); goto done; } }
    while(pos<size) {
        DWORD got=0,request=size-pos>MAXDWORD ? MAXDWORD : (DWORD)(size-pos);
        if(!ReadFile(input,encoded+pos,request,&got,NULL) || !got) { fprintf(stderr,"input read failed\n"); goto done; }
        pos+=got;
    }
    if(!CloseHandle(input)) { input=INVALID_HANDLE_VALUE; goto done; } input=INVALID_HANDLE_VALUE;
    status=obscura64_disaster_scan(encoded,size,&result);
    if(status!=OBSCURA64_DISASTER_SUCCESS) {
        fprintf(stderr,status==OBSCURA64_DISASTER_NOT_FOUND ? "no matching Profile\n" :
            status==OBSCURA64_DISASTER_AMBIGUOUS ? "ambiguous Profile matches; no output\n" : "scan failed\n");
        goto done;
    }
    output=CreateFileW(argv[2],GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    if(output==INVALID_HANDLE_VALUE) { fprintf(stderr,"cannot create output: existing paths are never overwritten\n"); goto done; }
    owned_output=1; pos=0;
    while(pos<result.payload_size) {
        DWORD written=0,request=result.payload_size-pos>MAXDWORD ? MAXDWORD : (DWORD)(result.payload_size-pos);
        if(!WriteFile(output,(unsigned char *)result.payload+pos,request,&written,NULL) || !written) goto done;
        pos+=written;
    }
    if(!FlushFileBuffers(output)) goto done;
    if(!CloseHandle(output)) { output=INVALID_HANDLE_VALUE; goto done; } output=INVALID_HANDLE_VALUE;
    printf("Recovered Profile ID: %u\nProfile: %.*s\n",(unsigned)result.profile_id,64,(const char *)result.profile);
    exit_code=0;
done:
    if(input!=INVALID_HANDLE_VALUE) CloseHandle(input);
    if(output!=INVALID_HANDLE_VALUE) CloseHandle(output);
    if(exit_code && owned_output) { fprintf(stderr,"output write failed\n"); (void)DeleteFileW(argv[2]); }
    free(encoded); obscura64_free(result.payload); return exit_code;
}
