#ifndef OBSCURA64_SHA256_H
#define OBSCURA64_SHA256_H

#include "obscura64.h"

/* Hash two byte segments as one stream; either may be empty. */
obscura64_status obscura64_sha256_segments(const void *first, size_t first_len,
    const void *second, size_t second_len, unsigned char digest[32]);

#endif
