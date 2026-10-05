#include "obscura64_test_recovery_helpers.h"

int main(void)
{
    uint16_t runtime=0,ids[4],duplicate[2]={0,0};
    unsigned char profile[64],payload[4096];
    obscura64_context *c=NULL;
    obscura64_disaster_result r;
    char *encoded=NULL;
    size_t n,i,j;
    LARGE_INTEGER frequency,start,end;
    CHECK(obscura64_profile_runtime_select_random(&runtime,profile)==OBSCURA64_CORE_STATUS_SUCCESS,"runtime Profile selection");
    ids[0]=0; ids[1]=1; ids[2]=4095; ids[3]=runtime;
    for(i=0;i<sizeof(payload);++i) payload[i]=(unsigned char)i;
    memcpy(payload,"normal text",11);
    for(j=0;j<4;++j) {
        CHECK(obscura64_context_create_from_profile((const char *)obscura64_profiles_v1[ids[j]],64,&c)==OBSCURA64_OK,"frozen encoding context");
        CHECK(obscura64_managed_encode_alloc(c,payload,sizeof(payload),&encoded,&n)==OBSCURA64_OK,"binary managed encoding");
        QueryPerformanceFrequency(&frequency); QueryPerformanceCounter(&start);
        CHECK(obscura64_disaster_scan(encoded,n,&r)==OBSCURA64_DISASTER_SUCCESS&&r.attempts==4096,"genuine exhaustive unique scan");
        QueryPerformanceCounter(&end);
        if(j==0) printf("4096 scan / 4096-byte payload: %.3f ms\n",1000.0*(double)(end.QuadPart-start.QuadPart)/(double)frequency.QuadPart);
        CHECK(r.profile_id==ids[j]&&!memcmp(r.profile,obscura64_profiles_v1[ids[j]],64)&&r.payload_size==sizeof(payload)&&!memcmp(r.payload,payload,sizeof(payload)),"exact Profile ID Profile and application bytes");
        obscura64_free(r.payload);
        if(j==0) CHECK(obscura64_disaster_scan_candidates(encoded,n,duplicate,2,&r)==OBSCURA64_DISASTER_AMBIGUOUS&&r.payload==NULL&&r.payload_size==0&&r.attempts==2,"duplicate test candidates refuse ambiguity");
        encoded[0]=encoded[0]=='A'?'B':'A';
        CHECK(obscura64_disaster_scan(encoded,n,&r)==OBSCURA64_DISASTER_NOT_FOUND&&r.payload==NULL,"corrupt managed input no match");
        obscura64_free(encoded); encoded=NULL;
        CHECK(obscura64_managed_encode_alloc(c,NULL,0,&encoded,&n)==OBSCURA64_OK,"empty managed encoding");
        CHECK(obscura64_disaster_scan(encoded,n,&r)==OBSCURA64_DISASTER_SUCCESS&&r.profile_id==ids[j]&&r.attempts==4096&&r.payload==NULL&&r.payload_size==0,"unique empty payload recovery");
        CHECK(obscura64_disaster_scan(encoded,n-4,&r)==OBSCURA64_DISASTER_NOT_FOUND,"truncated envelope no match");
        obscura64_free(encoded); encoded=NULL;
        CHECK(obscura64_encode_alloc(c,payload,20,&encoded,&n)==OBSCURA64_OK,"raw data encoding");
        CHECK(obscura64_disaster_scan(encoded,n,&r)==OBSCURA64_DISASTER_NOT_FOUND&&r.payload==NULL,"raw codec output not managed no match");
        obscura64_free(encoded); encoded=NULL; obscura64_context_destroy(c); c=NULL;
    }
    CHECK(obscura64_disaster_scan("AAAA",4,&r)==OBSCURA64_DISASTER_NOT_FOUND,"arbitrary alphabet no match");
    CHECK(obscura64_disaster_scan(NULL,0,&r)==OBSCURA64_DISASTER_NOT_FOUND,"empty encoded input not envelope");
    printf("PASS: %u disaster checks\n",checks); return 0;
}
