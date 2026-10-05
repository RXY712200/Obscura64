#include "obscura64_internal.h"
#include "obscura64_profiles_v1.h"
#include <stdlib.h>
#include <string.h>

static obscura64_disaster_status scan(const char *encoded,size_t n,const uint16_t *ids,
    size_t count,obscura64_disaster_result *out)
{
    unsigned char *scratch;
    size_t capacity,i,actual=0,size=0,matches=0;
    uint16_t winner=0;
    const unsigned char *view;
    obscura64_status s;
    obscura64_disaster_result result;
    if(!out) return OBSCURA64_DISASTER_ERROR;
    memset(out,0,sizeof(*out)); memset(&result,0,sizeof(result));
    if(!encoded || n==0 || n%4U) return OBSCURA64_DISASTER_NOT_FOUND;
    capacity=(n/4U)*3U; /* No multiplication overflow: factor is less than four. */
    scratch=(unsigned char *)malloc(capacity);
    if(!scratch) return OBSCURA64_DISASTER_ERROR;
    for(i=0;i<count;++i) {
        uint16_t id=ids ? ids[i] : (uint16_t)i;
        if(id>=OBSCURA64_PROFILE_COUNT) { free(scratch); return OBSCURA64_DISASTER_ERROR; }
        ++result.attempts;
        if(obscura64_codec_decode(encoded,n,(const char *)obscura64_profiles_v1[id],64,
            scratch,capacity,&actual)!=OBSCURA64_CORE_STATUS_SUCCESS) continue;
        s=obscura64_managed_payload_parse(scratch,actual,&view,&size);
        if(s==OBSCURA64_OK) { winner=id; ++matches; }
        else if(s!=OBSCURA64_INVALID_DATA) { free(scratch); return OBSCURA64_DISASTER_ERROR; }
    }
    out->attempts=result.attempts;
    if(matches!=1) { free(scratch); return matches ? OBSCURA64_DISASTER_AMBIGUOUS : OBSCURA64_DISASTER_NOT_FOUND; }
    /* Decode winner once more; allocate application payload only after uniqueness. */
    if(obscura64_codec_decode(encoded,n,(const char *)obscura64_profiles_v1[winner],64,
        scratch,capacity,&actual)!=OBSCURA64_CORE_STATUS_SUCCESS ||
        obscura64_managed_payload_parse(scratch,actual,&view,&size)!=OBSCURA64_OK) {
        free(scratch); return OBSCURA64_DISASTER_ERROR;
    }
    if(size) { result.payload=malloc(size); if(!result.payload) { free(scratch); return OBSCURA64_DISASTER_ERROR; } memcpy(result.payload,view,size); }
    result.payload_size=size; result.profile_id=winner;
    memcpy(result.profile,obscura64_profiles_v1[winner],64);
    free(scratch); *out=result; return OBSCURA64_DISASTER_SUCCESS;
}
obscura64_disaster_status obscura64_disaster_scan(const char *encoded,size_t n,obscura64_disaster_result *out)
{ return scan(encoded,n,NULL,OBSCURA64_PROFILE_COUNT,out); }
#ifdef OBSCURA64_DISASTER_TESTING
obscura64_disaster_status obscura64_disaster_scan_candidates(const char *encoded,size_t n,
    const uint16_t *ids,size_t count,obscura64_disaster_result *out)
{
    if(!ids || !count) { if(out) memset(out,0,sizeof(*out)); return OBSCURA64_DISASTER_ERROR; }
    return scan(encoded,n,ids,count,out);
}
#endif
