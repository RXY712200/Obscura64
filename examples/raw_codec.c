/* Raw Codec does not validate wrong Profile use. New apps should prefer Managed. */
#include "obscura64.h"
#include <stdio.h>
#include <string.h>
int main(int argc,char **argv)
{
    const unsigned char data[]={0,'r','a','w',0xff};
    obscura64_context *context=NULL;char *encoded=NULL;void *decoded=NULL;
    size_t n=0,m=0;obscura64_status status;
    if(argc!=2){fprintf(stderr,"usage: raw_codec <existing-project-directory-utf8>\n");return 2;}
    status=obscura64_open(argv[1],&context);
    if(status!=OBSCURA64_OK)goto done;
    status=obscura64_encode_alloc(context,data,sizeof(data),&encoded,&n);
    if(status!=OBSCURA64_OK)goto done;
    status=obscura64_decode_alloc(context,encoded,n,&decoded,&m);
    if(status==OBSCURA64_OK&&(m!=sizeof(data)||memcmp(decoded,data,m)))status=OBSCURA64_INVALID_DATA;
done:
    obscura64_free(encoded);obscura64_free(decoded);obscura64_context_destroy(context);
    if(status!=OBSCURA64_OK)fprintf(stderr,"%s\n",obscura64_status_string(status));
    return status==OBSCURA64_OK?0:1;
}
