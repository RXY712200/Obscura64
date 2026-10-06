#include "obscura64_v2_internal.h"
#include "obscura64_internal.h"

obscura64_status obscura64_v2_legacy_read(const void *data, size_t length,
    void **output, size_t *output_len)
{
    obscura64_disaster_result found = {0};
    obscura64_disaster_status status;
    if (output != NULL) *output = NULL;
    if (output_len != NULL) *output_len = 0;
    if (output == NULL || output_len == NULL || (data == NULL && length != 0))
        return OBSCURA64_INVALID_ARGUMENT;
    status = obscura64_disaster_scan((const char *)data, length, &found);
    switch (status) {
    case OBSCURA64_DISASTER_SUCCESS:
        *output = found.payload;
        *output_len = found.payload_size;
        return OBSCURA64_OK;
    case OBSCURA64_DISASTER_NOT_FOUND:
        return OBSCURA64_UNRECOGNIZED_DATA;
    case OBSCURA64_DISASTER_AMBIGUOUS:
        return OBSCURA64_LEGACY_AMBIGUOUS;
    default:
        return OBSCURA64_LEGACY_SCAN_FAILURE;
    }
}
