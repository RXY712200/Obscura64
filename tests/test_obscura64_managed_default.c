/* Public-header-only compile-time custom-default Provider test. */
#include "obscura64.h"
#include <stdio.h>
#include <string.h>
static unsigned int calls;
static obscura64_status encoded_size(void *u,const char p[64],size_t n,size_t *out)
{(void)u;(void)p;++calls;*out=n;return OBSCURA64_OK;}
static obscura64_status decoded_size(void *u,const char p[64],const char *in,size_t n,size_t *out)
{(void)u;(void)p;(void)in;++calls;*out=n;return OBSCURA64_OK;}
static obscura64_status encode(void *u,const char p[64],const void *in,size_t n,char *out,size_t cap,size_t *len)
{(void)u;(void)p;++calls;*len=n;if(cap<n)return OBSCURA64_BUFFER_TOO_SMALL;if(n)memcpy(out,in,n);return OBSCURA64_OK;}
static obscura64_status decode(void *u,const char p[64],const char *in,size_t n,void *out,size_t cap,size_t *len)
{return encode(u,p,in,n,(char *)out,cap,len);}
const obscura64_provider obscura64_test_managed_default_provider={sizeof(obscura64_provider),OBSCURA64_PROVIDER_ABI_VERSION,NULL,encoded_size,decoded_size,encode,decode};
int main(void)
{
    const char pool[]="ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnpqrstuvwxyz23456789@#$%&!?~";
    const unsigned char data[]={0,0xff,0x80,'X'};
    obscura64_context *c=NULL;char *encoded=NULL;void *decoded=NULL;size_t n=0,m=0;
    unsigned int checks=0;
#define CHECK(c) do { ++checks; if(!(c)) return 1; } while(0)
    CHECK(obscura64_context_create_from_profile(pool,64,&c)==OBSCURA64_OK);
    CHECK(obscura64_managed_encode_alloc(c,data,sizeof(data),&encoded,&n)==OBSCURA64_OK);
    CHECK(n==sizeof(data)+64 && !memcmp(encoded,"OB64MP01",8));
    CHECK(obscura64_managed_decode_alloc(c,encoded,n,&decoded,&m)==OBSCURA64_OK);
    CHECK(m==sizeof(data) && !memcmp(decoded,data,m));
    CHECK(calls>=4);
    obscura64_free(encoded);obscura64_free(decoded);obscura64_context_destroy(c);
    printf("PASS: %u compile-time custom default checks\n",checks);return 0;
}
