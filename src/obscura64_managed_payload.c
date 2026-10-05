#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include "obscura64_internal.h"
#include <stdlib.h>
#include <string.h>

obscura64_status obscura64_managed_sha256(const unsigned char *data, size_t n, unsigned char digest[32])
{
    BCRYPT_ALG_HANDLE alg=NULL;
    BCRYPT_HASH_HANDLE hash=NULL;
    obscura64_status s=OBSCURA64_IO_ERROR;
    if ((!data && n) || !digest) return OBSCURA64_INVALID_ARGUMENT;
    if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,NULL,0)) ||
        !BCRYPT_SUCCESS(BCryptCreateHash(alg,&hash,NULL,0,NULL,0,0))) goto done;
    while(n) {
        ULONG chunk=n>MAXDWORD ? MAXDWORD : (ULONG)n;
        if (!BCRYPT_SUCCESS(BCryptHashData(hash,(PUCHAR)data,chunk,0))) goto done;
        data+=chunk; n-=chunk;
    }
    if (BCRYPT_SUCCESS(BCryptFinishHash(hash,digest,32,0))) s=OBSCURA64_OK;
done:
    if(hash) BCryptDestroyHash(hash);
    if(alg) BCryptCloseAlgorithmProvider(alg,0);
    return s;
}

obscura64_status obscura64_managed_payload_build(const void *data,size_t n,unsigned char **out,size_t *size)
{
    unsigned char *p;
    size_t i;
    uint64_t length=n;
    obscura64_status s;
    if(out) *out=NULL;
    if(size) *size=0;
    if(!out || !size || (!data && n)) return OBSCURA64_INVALID_ARGUMENT;
    if(n>SIZE_MAX-64U) return OBSCURA64_SIZE_OVERFLOW;
    p=(unsigned char *)malloc(n+64U);
    if(!p) return OBSCURA64_OUT_OF_MEMORY;
    memset(p,0,32); memcpy(p,"OB64MP01",8); p[8]=1; p[10]=32;
    for(i=0;i<8;++i) p[16+i]=(unsigned char)(length>>(8*i));
    if(n) memcpy(p+32,data,n);
    s=obscura64_managed_sha256(p,n+32U,p+n+32U);
    if(s!=OBSCURA64_OK) { free(p); return s; }
    *out=p; *size=n+64U; return OBSCURA64_OK;
}
obscura64_status obscura64_managed_payload_parse(const unsigned char *p,size_t n,const unsigned char **out,size_t *size)
{
    size_t i,length;
    uint64_t stored=0;
    unsigned char digest[32];
    obscura64_status s;
    if(out) *out=NULL;
    if(size) *size=0;
    if(!out || !size || (!p && n)) return OBSCURA64_INVALID_ARGUMENT;
    if(n<64U || !p || memcmp(p,"OB64MP01",8) || p[8]!=1 || p[9]!=0 || p[10]!=32 || p[11]!=0)
        return OBSCURA64_INVALID_DATA;
    for(i=12;i<16;++i) if(p[i]) return OBSCURA64_INVALID_DATA;
    for(i=24;i<32;++i) if(p[i]) return OBSCURA64_INVALID_DATA;
    for(i=0;i<8;++i) stored|=(uint64_t)p[16+i]<<(8*i);
    if(stored>SIZE_MAX-64U) return OBSCURA64_INVALID_DATA;
    length=(size_t)stored;
    if(n!=length+64U) return OBSCURA64_INVALID_DATA;
    s=obscura64_managed_sha256(p,length+32U,digest);
    if(s!=OBSCURA64_OK) return s;
    if(memcmp(digest,p+length+32U,32)) return OBSCURA64_INVALID_DATA;
    *out=p+32; *size=length; return OBSCURA64_OK;
}
obscura64_status obscura64_managed_encoded_size(const obscura64_context *c,size_t n,size_t *out)
{
    if(!out) return OBSCURA64_INVALID_ARGUMENT;
    *out=0;
    if(!c) return OBSCURA64_INVALID_ARGUMENT;
    if(n>SIZE_MAX-64U) return OBSCURA64_SIZE_OVERFLOW;
    return obscura64_encoded_size(c,n+64U,out);
}
static obscura64_status decoded_envelope(const obscura64_context *c,const char *encoded,size_t n,
    void **raw,const unsigned char **payload,size_t *size)
{
    size_t length=0;
    obscura64_status s;
    *raw=NULL; *payload=NULL; *size=0;
    if(!c || (!encoded && n)) return OBSCURA64_INVALID_ARGUMENT;
    s=obscura64_decode_alloc(c,encoded,n,raw,&length);
    if(s==OBSCURA64_OK) s=obscura64_managed_payload_parse((const unsigned char *)*raw,length,payload,size);
    if(s!=OBSCURA64_OK) { free(*raw); *raw=NULL; }
    return s;
}
obscura64_status obscura64_managed_decoded_size(const obscura64_context *c,const char *encoded,size_t n,size_t *out)
{
    void *raw;
    const unsigned char *view;
    size_t length;
    obscura64_status s;
    if(!out) return OBSCURA64_INVALID_ARGUMENT;
    *out=0;
    s=decoded_envelope(c,encoded,n,&raw,&view,&length);
    if(s==OBSCURA64_OK) *out=length;
    free(raw); return s;
}
obscura64_status obscura64_managed_encode_alloc(const obscura64_context *c,const void *input,size_t n,char **out,size_t *length)
{
    unsigned char *raw;
    size_t size;
    obscura64_status s;
    if(out) *out=NULL;
    if(length) *length=0;
    if(!out || !length || !c || (!input && n)) return OBSCURA64_INVALID_ARGUMENT;
    s=obscura64_managed_encoded_size(c,n,&size);
    if(s!=OBSCURA64_OK) return s;
    s=obscura64_managed_payload_build(input,n,&raw,&size);
    if(s!=OBSCURA64_OK) return s;
    s=obscura64_encode_alloc(c,raw,size,out,length); free(raw); return s;
}
obscura64_status obscura64_managed_decode_alloc(const obscura64_context *c,const char *encoded,size_t n,void **out,size_t *length)
{
    void *raw,*p=NULL;
    const unsigned char *view;
    size_t size;
    obscura64_status s;
    if(out) *out=NULL;
    if(length) *length=0;
    if(!out || !length) return OBSCURA64_INVALID_ARGUMENT;
    s=decoded_envelope(c,encoded,n,&raw,&view,&size);
    if(s!=OBSCURA64_OK) return s;
    if(size) { p=malloc(size); if(!p) { free(raw); return OBSCURA64_OUT_OF_MEMORY; } memcpy(p,view,size); }
    free(raw); *out=p; *length=size; return OBSCURA64_OK;
}
obscura64_status obscura64_managed_encode(const obscura64_context *c,const void *input,size_t n,char *out,size_t cap,size_t *length)
{
    char *temp=NULL;
    size_t required=0,actual=0;
    obscura64_status s;
    if(!length) return OBSCURA64_INVALID_ARGUMENT;
    *length=0;
    if((!input && n) || (!out && cap)) return OBSCURA64_INVALID_ARGUMENT;
    s=obscura64_managed_encoded_size(c,n,&required);
    if(s!=OBSCURA64_OK) return s;
    if(cap<required) { *length=required; return OBSCURA64_BUFFER_TOO_SMALL; }
    if(!out) return OBSCURA64_INVALID_ARGUMENT;
    s=obscura64_managed_encode_alloc(c,input,n,&temp,&actual);
    if(s==OBSCURA64_OK && actual>cap) s=OBSCURA64_INVALID_PROVIDER;
    if(s==OBSCURA64_OK) { if(actual) memcpy(out,temp,actual); *length=actual; }
    free(temp); return s;
}
obscura64_status obscura64_managed_decode(const obscura64_context *c,const char *encoded,size_t n,void *out,size_t cap,size_t *length)
{
    void *raw;
    const unsigned char *view;
    size_t size;
    obscura64_status s;
    if(!length) return OBSCURA64_INVALID_ARGUMENT;
    *length=0;
    if(!out && cap) return OBSCURA64_INVALID_ARGUMENT;
    s=decoded_envelope(c,encoded,n,&raw,&view,&size);
    if(s!=OBSCURA64_OK) return s;
    if(cap<size) { free(raw); *length=size; return OBSCURA64_BUFFER_TOO_SMALL; }
    if(size) memcpy(out,view,size);
    free(raw); *length=size; return OBSCURA64_OK;
}
