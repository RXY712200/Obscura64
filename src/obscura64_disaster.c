#include "obscura64_internal.h"
#include "obscura64_profiles_v1.h"

#include <stdlib.h>
#include <string.h>

/* Fifteen fixed raw header bytes produce twenty encoded characters regardless
 * of the following payload-length field. The 16th raw byte would share a
 * Base64 quartet with variable data, so it is deliberately excluded. */
static const unsigned char managed_prefix[15] = {
    'O','B','6','4','M','P','0','1', 1,0,32,0, 0,0,0
};

static int possible_builtin_input(const char *encoded, size_t n)
{
    unsigned char allowed[256] = {0};
    size_t i;
    if (encoded == NULL || n < 88U || n % 4U != 0) return 0;
    for (i = 0; i < OBSCURA64_ALPHABET_SIZE; ++i)
        allowed[(unsigned char)obscura64_v1_character_pool[i]] = 1;
    for (i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)encoded[i];
        if (c == '=') {
            if (i < n - 2U || (i == n - 2U && encoded[n - 1U] != '='))
                return 0;
        } else if (!allowed[c]) return 0;
    }
    return 1;
}

static obscura64_disaster_status scan(const char *encoded, size_t n,
    const uint16_t *ids, size_t count, obscura64_disaster_result *out)
{
    unsigned char *scratch = NULL;
    size_t capacity, i, actual = 0, size = 0, matches = 0;
    uint16_t winner = 0;
    const unsigned char *view;
    obscura64_status status;
    obscura64_disaster_result result = {0};
    if (out == NULL) return OBSCURA64_DISASTER_ERROR;
    memset(out, 0, sizeof(*out));
    if (!possible_builtin_input(encoded, n)) return OBSCURA64_DISASTER_NOT_FOUND;
    capacity = (n / 4U) * 3U; /* Cannot overflow: factor is less than four. */
    for (i = 0; i < count; ++i) {
        uint16_t id = ids != NULL ? ids[i] : (uint16_t)i;
        char prefix[20];
        size_t prefix_len = 0;
        if (id >= OBSCURA64_PROFILE_COUNT) {
            free(scratch);
            return OBSCURA64_DISASTER_ERROR;
        }
        ++result.attempts;
        if (obscura64_codec_encode(managed_prefix, sizeof(managed_prefix),
            (const char *)obscura64_profiles_v1[id], OBSCURA64_PROFILE_SIZE,
            prefix, sizeof(prefix), &prefix_len) != OBSCURA64_CORE_STATUS_SUCCESS ||
            prefix_len != sizeof(prefix)) {
            free(scratch);
            return OBSCURA64_DISASTER_ERROR;
        }
        if (memcmp(prefix, encoded, sizeof(prefix)) != 0) continue;
        ++result.candidates;
        if (scratch == NULL) {
            scratch = (unsigned char *)malloc(capacity);
            if (scratch == NULL) return OBSCURA64_DISASTER_ERROR;
        }
        ++result.full_decodes;
        if (obscura64_codec_decode(encoded, n,
            (const char *)obscura64_profiles_v1[id], OBSCURA64_PROFILE_SIZE,
            scratch, capacity, &actual) != OBSCURA64_CORE_STATUS_SUCCESS)
            continue;
        status = obscura64_managed_payload_parse(scratch, actual, &view, &size);
        if (status == OBSCURA64_OK) { winner = id; ++matches; }
        else if (status != OBSCURA64_INVALID_DATA) {
            free(scratch);
            return OBSCURA64_DISASTER_ERROR;
        }
    }
    out->attempts = result.attempts;
    out->candidates = result.candidates;
    out->full_decodes = result.full_decodes;
    if (matches != 1) {
        free(scratch);
        return matches ? OBSCURA64_DISASTER_AMBIGUOUS : OBSCURA64_DISASTER_NOT_FOUND;
    }
    /* Allocate application payload only after the uniqueness decision. */
    ++out->full_decodes;
    if (obscura64_codec_decode(encoded, n,
        (const char *)obscura64_profiles_v1[winner], OBSCURA64_PROFILE_SIZE,
        scratch, capacity, &actual) != OBSCURA64_CORE_STATUS_SUCCESS ||
        obscura64_managed_payload_parse(scratch, actual, &view, &size) != OBSCURA64_OK) {
        free(scratch);
        return OBSCURA64_DISASTER_ERROR;
    }
    if (size != 0) {
        result.payload = malloc(size);
        if (result.payload == NULL) { free(scratch); return OBSCURA64_DISASTER_ERROR; }
        memcpy(result.payload, view, size);
    }
    result.payload_size = size;
    result.profile_id = winner;
    memcpy(result.profile, obscura64_profiles_v1[winner], OBSCURA64_PROFILE_SIZE);
    result.attempts = out->attempts;
    result.candidates = out->candidates;
    result.full_decodes = out->full_decodes;
    free(scratch);
    *out = result;
    return OBSCURA64_DISASTER_SUCCESS;
}

obscura64_disaster_status obscura64_disaster_scan(const char *encoded,
    size_t n, obscura64_disaster_result *out)
{
    return scan(encoded, n, NULL, OBSCURA64_PROFILE_COUNT, out);
}

#ifdef OBSCURA64_DISASTER_TESTING
obscura64_disaster_status obscura64_disaster_scan_candidates(const char *encoded,
    size_t n, const uint16_t *ids, size_t count, obscura64_disaster_result *out)
{
    if (ids == NULL || count == 0) {
        if (out != NULL) memset(out, 0, sizeof(*out));
        return OBSCURA64_DISASTER_ERROR;
    }
    return scan(encoded, n, ids, count, out);
}
#endif
