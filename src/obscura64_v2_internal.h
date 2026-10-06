#ifndef OBSCURA64_V2_INTERNAL_H
#define OBSCURA64_V2_INTERNAL_H

#include "obscura64.h"
#include <stddef.h>
#include <stdint.h>

#define OBSCURA64_V2_HEADER_SIZE 24U

obscura64_status obscura64_v2_envelope_build(uint16_t kind,
    const void *body, size_t body_len, void **output, size_t *output_len);
obscura64_status obscura64_v2_envelope_parse(const void *data, size_t length,
    uint16_t *kind, const unsigned char **body, size_t *body_len);
int obscura64_v2_has_magic(const void *data, size_t length);

obscura64_status obscura64_v2_casual_protect(const void *input, size_t length,
    void **body, size_t *body_len);
obscura64_status obscura64_v2_casual_unprotect(const void *body, size_t body_len,
    void **output, size_t *output_len);
/* Deterministic seam for canonical fixture tests; not a public API. */
obscura64_status obscura64_v2_casual_protect_with_profile(uint16_t profile_id,
    const void *input, size_t length, void **body, size_t *body_len);

obscura64_status obscura64_v2_current_user_protect(const void *input, size_t length,
    void **body, size_t *body_len);
obscura64_status obscura64_v2_current_user_unprotect(const void *body, size_t body_len,
    void **output, size_t *output_len);

obscura64_status obscura64_v2_legacy_read(const void *data, size_t length,
    void **output, size_t *output_len);

#endif
