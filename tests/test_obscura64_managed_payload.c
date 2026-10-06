#include "obscura64_test_recovery_helpers.h"
#include "obscura64_sha256.h"

static unsigned int provider_calls;
static int fail_codec;
static obscura64_status sz(void *u,const char p[64],size_t n,size_t *out)
{ (void)u;(void)p; ++provider_calls; if(n==SIZE_MAX) return OBSCURA64_SIZE_OVERFLOW; *out=n+1; return OBSCURA64_OK; }
static obscura64_status ds(void *u,const char p[64],const char *in,size_t n,size_t *out)
{ (void)u;(void)p; ++provider_calls; if(!in||!n||in[0]!='!') return OBSCURA64_INVALID_DATA; *out=n-1; return OBSCURA64_OK; }
static obscura64_status enc(void *u,const char p[64],const void *in,size_t n,char *out,size_t cap,size_t *len)
{ (void)u;(void)p; ++provider_calls; *len=n+1; if(cap<n+1) return OBSCURA64_BUFFER_TOO_SMALL; out[0]='!'; if(fail_codec) return OBSCURA64_INVALID_DATA; if(n) memcpy(out+1,in,n); return OBSCURA64_OK; }
static obscura64_status dec(void *u,const char p[64],const char *in,size_t n,void *out,size_t cap,size_t *len)
{ obscura64_status s=ds(u,p,in,n,len); if(s!=OBSCURA64_OK) return s; if(cap<*len) return OBSCURA64_BUFFER_TOO_SMALL; if(*len) memcpy(out,in+1,*len); return OBSCURA64_OK; }

int main(void)
{
    obscura64_context *a=NULL,*b=NULL,*custom=NULL;
    obscura64_provider provider={sizeof(obscura64_provider),OBSCURA64_PROVIDER_ABI_VERSION,NULL,sz,ds,enc,dec};
    unsigned char input[4097],out[8192],before[8192],hash[32],*envelope=NULL,*damaged;
    const unsigned char *view;
    char *encoded=NULL,*altered=NULL;
    void *decoded=NULL,*raw=NULL;
    size_t n=0,size=0,required=0,actual=0,i,j;
    const size_t offsets[]={0,8,9,10,11,12,15,16,23,24,31,32,4097+32};
    static const unsigned char abc_sha[]={0xf8,0xdc,0x08,0xcd,0x78,0x3d,0x31,0xc9,0x29,0xe8,0x8d,0xe1,0xef,0x25,0x9d,0x30,0xea,0x4d,0x4b,0x9f,0x45,0xae,0xbc,0x4a,0x08,0xb2,0xa6,0xed,0xe2,0x81,0xb5,0xf3};
    CHECK(obscura64_managed_payload_build("abc",3,&envelope,&size)==OBSCURA64_OK&&size==67,"fixed abc envelope vector");
    CHECK(!memcmp(envelope+32,"abc",3)&&!memcmp(envelope+35,abc_sha,32),"independent SHA vector over canonical header and abc");
    free(envelope);envelope=NULL;
    for(i=0;i<sizeof(input);++i) input[i]=(unsigned char)i;
    CHECK(obscura64_context_create_from_profile((const char *)obscura64_profiles_v1[0],64,&a)==OBSCURA64_OK,"Profile zero context");
    CHECK(obscura64_context_create_from_profile((const char *)obscura64_profiles_v1[1],64,&b)==OBSCURA64_OK,"different frozen context");
    CHECK(obscura64_managed_encode_alloc(a,input,sizeof(input),&encoded,&n)==OBSCURA64_OK&&n>0,"binary multi-quanta managed encode");
    CHECK(obscura64_managed_encoded_size(a,sizeof(input),&required)==OBSCURA64_OK&&required==n,"exact encode sizing");
    CHECK(obscura64_managed_decoded_size(a,encoded,n,&required)==OBSCURA64_OK&&required==sizeof(input),"validated payload sizing");
    CHECK(obscura64_managed_decode_alloc(a,encoded,n,&decoded,&actual)==OBSCURA64_OK&&actual==sizeof(input)&&!memcmp(decoded,input,actual),"binary allocating roundtrip");
    obscura64_free(decoded); decoded=NULL;
    CHECK(obscura64_managed_decode_alloc(b,encoded,n,&decoded,&actual)==OBSCURA64_INVALID_DATA&&decoded==NULL&&actual==0,"wrong Profile rejected atomically");
    CHECK(obscura64_decode_alloc(a,encoded,n,&raw,&size)==OBSCURA64_OK&&size==sizeof(input)+64,"inspect deterministic envelope");
    envelope=(unsigned char *)raw;
    CHECK(!memcmp(envelope,"OB64MP01",8)&&envelope[8]==1&&envelope[9]==0&&envelope[10]==32&&envelope[11]==0,"exact fixed header");
    CHECK(envelope[16]==1&&envelope[17]==16,"little endian payload length 4097");
    for(i=12;i<16;++i) CHECK(envelope[i]==0,"flags zero");
    for(i=18;i<32;++i) CHECK(envelope[i]==0,"high length and reserved zero");
    CHECK(obscura64_sha256_segments(envelope,size-32,NULL,0,hash)==OBSCURA64_OK&&!memcmp(hash,envelope+size-32,32),"exact SHA header plus payload coverage");
    memset(out,0xa5,sizeof(out)); memcpy(before,out,sizeof(out));
    CHECK(obscura64_managed_decode(a,encoded,n,out,1,&actual)==OBSCURA64_BUFFER_TOO_SMALL&&actual==sizeof(input)&&!memcmp(out,before,sizeof(out)),"decode too small unchanged");
    CHECK(obscura64_managed_encode(a,input,sizeof(input),(char *)out,1,&actual)==OBSCURA64_BUFFER_TOO_SMALL&&actual==n&&!memcmp(out,before,sizeof(out)),"encode too small unchanged");
    CHECK(obscura64_managed_encode(a,input,sizeof(input),NULL,0,&actual)==OBSCURA64_BUFFER_TOO_SMALL&&actual==n,"NULL encode size request");
    CHECK(obscura64_managed_decode(a,encoded,n,NULL,0,&actual)==OBSCURA64_BUFFER_TOO_SMALL&&actual==sizeof(input),"NULL decode size request");
    CHECK(obscura64_managed_decode(b,encoded,n,out,sizeof(out),&actual)==OBSCURA64_INVALID_DATA&&actual==0&&!memcmp(out,before,sizeof(out)),"invalid decode unchanged");
    CHECK(obscura64_managed_decode(a,encoded,n,out,sizeof(out),&actual)==OBSCURA64_OK&&actual==sizeof(input)&&!memcmp(out,input,actual),"caller buffer roundtrip");
    memset(out,0xa5,sizeof(out));
    CHECK(obscura64_managed_encode(a,input,sizeof(input),(char *)out,sizeof(out),&actual)==OBSCURA64_OK&&actual==n&&!memcmp(out,encoded,n)&&out[n]==0xa5,"caller encode exact bytes and no NUL terminator");
    damaged=(unsigned char *)malloc(size+1); CHECK(damaged!=NULL,"tamper scratch");
    for(j=0;j<sizeof(offsets)/sizeof(offsets[0]);++j) {
        memcpy(damaged,envelope,size); damaged[offsets[j]]^=1;
        CHECK(obscura64_managed_payload_parse(damaged,size,&view,&actual)==OBSCURA64_INVALID_DATA&&view==NULL&&actual==0,"tamper parser failure atomicity");
        CHECK(obscura64_encode_alloc(a,damaged,size,&altered,&required)==OBSCURA64_OK,"re-encode tampered raw envelope");
        CHECK(obscura64_managed_decode_alloc(a,altered,required,&decoded,&actual)==OBSCURA64_INVALID_DATA&&decoded==NULL&&actual==0,"raw-valid envelope corruption rejected");
        obscura64_free(altered); altered=NULL;
    }
    memcpy(damaged,envelope,size); damaged[size]=0;
    CHECK(obscura64_managed_payload_parse(damaged,size+1,&view,&actual)==OBSCURA64_INVALID_DATA,"trailing decoded byte rejected");
    for(i=0;i<64;++i) CHECK(obscura64_managed_payload_parse(envelope,i,&view,&actual)==OBSCURA64_INVALID_DATA,"truncated header or missing digest rejected");
    memcpy(damaged,envelope,size); memset(damaged+16,0xff,8);
    CHECK(obscura64_managed_payload_parse(damaged,size,&view,&actual)==OBSCURA64_INVALID_DATA,"impossible uint64 payload size");
    CHECK(obscura64_managed_encoded_size(a,SIZE_MAX,&actual)==OBSCURA64_SIZE_OVERFLOW&&actual==0,"envelope overflow");
    CHECK(obscura64_managed_encoded_size(a,SIZE_MAX-64,&actual)==OBSCURA64_SIZE_OVERFLOW&&actual==0,"codec encoded-size overflow");
    decoded=(void *)1; actual=99;
    CHECK(obscura64_managed_decode_alloc(NULL,encoded,n,&decoded,&actual)==OBSCURA64_INVALID_ARGUMENT&&decoded==NULL&&actual==0,"invalid allocating decode clears outputs");
    altered=(char *)1; actual=99;
    CHECK(obscura64_managed_encode_alloc(a,NULL,1,&altered,&actual)==OBSCURA64_INVALID_ARGUMENT&&altered==NULL&&actual==0,"invalid allocating encode clears outputs");
    free(damaged); free(raw); obscura64_free(encoded); encoded=NULL;
    CHECK(obscura64_managed_encode_alloc(a,NULL,0,&encoded,&n)==OBSCURA64_OK&&n>0,"nonempty envelope for empty payload");
    CHECK(obscura64_managed_decode_alloc(a,encoded,n,&decoded,&actual)==OBSCURA64_OK&&decoded==NULL&&actual==0,"empty decode no malloc zero");
    obscura64_free(encoded); encoded=NULL;
    CHECK(obscura64_context_create_from_profile_with_provider((const char *)obscura64_profiles_v1[0],64,&provider,&custom)==OBSCURA64_OK,"custom wrapper Provider");
    CHECK(obscura64_managed_encode_alloc(custom,input,sizeof(input),&encoded,&n)==OBSCURA64_OK&&encoded[0]=='!',"managed invokes custom encode");
    CHECK(obscura64_managed_decode_alloc(custom,encoded,n,&decoded,&actual)==OBSCURA64_OK&&actual==sizeof(input)&&!memcmp(decoded,input,actual)&&provider_calls>=4,"managed invokes custom decode");
    memset(out,0xa5,sizeof(out));memcpy(before,out,sizeof(out));fail_codec=1;
    CHECK(obscura64_managed_encode(custom,input,sizeof(input),(char *)out,sizeof(out),&actual)==OBSCURA64_INVALID_DATA&&actual==0&&!memcmp(out,before,sizeof(out)),"Provider partial-write failure cannot modify caller buffer");
    altered=(char *)1;actual=99;
    CHECK(obscura64_managed_encode_alloc(custom,input,sizeof(input),&altered,&actual)==OBSCURA64_INVALID_DATA&&altered==NULL&&actual==0,"Provider failure clears allocated outputs");
    fail_codec=0;
    {
        obscura64_disaster_result result;
        CHECK(obscura64_disaster_scan(encoded,n,&result)==OBSCURA64_DISASTER_NOT_FOUND&&result.payload==NULL,"custom transformation not falsely recoverable");
    }
    obscura64_free(encoded); obscura64_free(decoded);
    obscura64_context_destroy(a); obscura64_context_destroy(b); obscura64_context_destroy(custom);
    printf("PASS: %u managed payload checks\n",checks); return 0;
}
