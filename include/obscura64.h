#ifndef OBSCURA64_H
#define OBSCURA64_H

/* Obscura64 public C API. */

#include <stddef.h>

#define OBSCURA64_VERSION_MAJOR 2
#define OBSCURA64_VERSION_MINOR 0
#define OBSCURA64_VERSION_PATCH 0
#define OBSCURA64_VERSION_PRERELEASE "preview.5"
#define OBSCURA64_PROFILE_LIBRARY_VERSION 1
#define OBSCURA64_PROFILE_SIZE 64
#define OBSCURA64_PROVIDER_ABI_VERSION 1

#ifdef __cplusplus
extern "C" {
#endif

typedef enum obscura64_status {
    OBSCURA64_OK = 0,
    OBSCURA64_INVALID_ARGUMENT,
    OBSCURA64_INVALID_PROFILE,
    OBSCURA64_INVALID_PROVIDER,
    OBSCURA64_INVALID_DATA,
    OBSCURA64_BUFFER_TOO_SMALL,
    OBSCURA64_SIZE_OVERFLOW,
    OBSCURA64_OUT_OF_MEMORY,
    OBSCURA64_IO_ERROR,
    OBSCURA64_STATE_MISSING,
    OBSCURA64_STATE_CORRUPT,
    OBSCURA64_RNG_FAILURE,
    OBSCURA64_BUSY,
    OBSCURA64_UNRECOVERABLE,
    OBSCURA64_ENVELOPE_CORRUPT,
    OBSCURA64_UNSUPPORTED_VERSION,
    OBSCURA64_UNSUPPORTED_PROTECTION,
    OBSCURA64_CASUAL_CORRUPT,
    OBSCURA64_PROTECTION_FAILURE,
    OBSCURA64_UNRECOGNIZED_DATA,
    OBSCURA64_LEGACY_AMBIGUOUS,
    OBSCURA64_LEGACY_SCAN_FAILURE,
    OBSCURA64_PROTECTED_CORRUPT,
    OBSCURA64_FILE_NOT_FOUND
} obscura64_status;

typedef struct obscura64_context obscura64_context;

/* Semantic protection choices. NONE is unprotected and explicit opt-in only;
 * CASUAL is reversible obfuscation; CURRENT_USER delegates to Windows DPAPI.
 * There is no implicit default. */
typedef enum obscura64_protection {
    OBSCURA64_PROTECTION_NONE = 0,
    OBSCURA64_PROTECTION_CASUAL = 1,
    OBSCURA64_PROTECTION_CURRENT_USER = 2
} obscura64_protection;

/* V2 ordinary byte API. Allocation-first is recommended: protection backends
 * may determine exact output size only after performing the operation.
 * Unprotect reads V2 kind from the envelope or recognizes supported V1 builtin
 * Managed Payload; it requires no selection by the caller.
 * Input and output buffers must not overlap. NULL input is valid only at zero
 * length. Caller-buffer operations perform the backend operation to discover
 * exact size; BUFFER_TOO_SMALL reports that size, all other failures set
 * output_len to zero. Caller output bytes remain unchanged on failure. NULL
 * output/capacity zero is an operation-driven size query, not a cheap length
 * calculation. A retry may produce a different size for variable-output
 * backends. Allocating failures clear pointer and length; free successful
 * results with obscura64_free. Empty unprotect returns NULL/zero. */
obscura64_status obscura64_protect(
    obscura64_protection protection, const void *input, size_t input_len, void *output,
    size_t output_capacity, size_t *output_len);
obscura64_status obscura64_unprotect(
    const void *protected_data, size_t protected_len, void *output,
    size_t output_capacity, size_t *output_len);
obscura64_status obscura64_protect_alloc(
    obscura64_protection protection, const void *input, size_t input_len,
    void **output, size_t *output_len);
obscura64_status obscura64_unprotect_alloc(
    const void *protected_data, size_t protected_len,
    void **output, size_t *output_len);

/* Optional success classification; a V1 result should be upgraded. A non-NULL
 * format output is set only on success and cleared to UNKNOWN on failure. */
typedef enum obscura64_data_format {
    OBSCURA64_FORMAT_UNKNOWN = 0,
    OBSCURA64_FORMAT_V2 = 1,
    OBSCURA64_FORMAT_V1_LEGACY_UPGRADE_RECOMMENDED = 2
} obscura64_data_format;
obscura64_status obscura64_unprotect_alloc_ex(
    const void *protected_data, size_t protected_len,
    void **output, size_t *output_len, obscura64_data_format *format);

/* Full, NUL-terminated UTF-8 Windows target paths. These functions own one
 * adjacent backup and coordination lock; no V1 project state is required.
 * Successful allocations are released with obscura64_free. */
typedef enum obscura64_file_source {
    OBSCURA64_FILE_SOURCE_UNKNOWN = 0,
    OBSCURA64_FILE_SOURCE_PRIMARY = 1,
    OBSCURA64_FILE_SOURCE_BACKUP = 2
} obscura64_file_source;
obscura64_status obscura64_write_file(const char *path_utf8,
    obscura64_protection protection, const void *input, size_t input_len);
obscura64_status obscura64_read_file_alloc(const char *path_utf8,
    void **output, size_t *output_len);
obscura64_status obscura64_read_file_alloc_ex(const char *path_utf8,
    void **output, size_t *output_len, obscura64_file_source *source,
    obscura64_data_format *format);
/* Explicit in-place maintenance operation under one exclusive file lock.
 * The target semantic is required. A current V2 primary with the same semantic
 * succeeds without touching primary or backup. */
obscura64_status obscura64_upgrade_file(const char *path_utf8,
    obscura64_protection target_protection);

/* Only V1 builtin Managed Payload is supported. No project state is read or
 * modified. Custom V1 Providers still require the existing V1 context API. */
obscura64_status obscura64_migrate_v1_managed_alloc(
    obscura64_protection protection, const char *legacy_data, size_t legacy_len,
    void **output, size_t *output_len);

/* Common contracts: contexts are owned by the caller and released with
 * obscura64_context_destroy; constructors/open/Force clear *out_context on
 * failure. Paths are NUL-terminated UTF-8 existing project directories.
 * Profiles are explicit-length 64-byte permutations, not necessarily strings.
 * All byte buffers use explicit lengths; a NULL input is allowed only at zero
 * length. Length/output-pointer arguments are required. Allocating results use
 * obscura64_free, never context_destroy; failures clear pointer and length.
 * Raw empty results are NULL/zero. Caller buffers are unchanged on failure;
 * BUFFER_TOO_SMALL reports required length, other failures report zero.
 * NULL output with capacity zero is a size query (empty output succeeds).
 * Raw Codec cannot detect use of the wrong Profile; prefer Managed for new data.
 * A supplied Provider is copied, but its user_data remains caller-owned.
 */

/*
 * Advanced codec strategy interface. Every callback receives user_data and
 * the context's complete Profile. Callbacks use explicit lengths, never
 * append NUL, report required capacity for BUFFER_TOO_SMALL, and must leave
 * output unchanged on failure. Obscura64 does not own user_data; it must
 * remain valid until all contexts using this Provider are destroyed.
 */
typedef obscura64_status (*obscura64_provider_encoded_size_fn)(
    void *user_data,
    const char profile[OBSCURA64_PROFILE_SIZE],
    size_t input_len,
    size_t *encoded_len);
typedef obscura64_status (*obscura64_provider_decoded_size_fn)(
    void *user_data,
    const char profile[OBSCURA64_PROFILE_SIZE],
    const char *encoded,
    size_t encoded_len,
    size_t *decoded_len);
typedef obscura64_status (*obscura64_provider_encode_fn)(
    void *user_data,
    const char profile[OBSCURA64_PROFILE_SIZE],
    const void *input,
    size_t input_len,
    char *output,
    size_t output_capacity,
    size_t *output_len);
typedef obscura64_status (*obscura64_provider_decode_fn)(
    void *user_data,
    const char profile[OBSCURA64_PROFILE_SIZE],
    const char *encoded,
    size_t encoded_len,
    void *output,
    size_t output_capacity,
    size_t *output_len);

typedef struct obscura64_provider {
    size_t struct_size;
    unsigned int abi_version;
    void *user_data;
    obscura64_provider_encoded_size_fn encoded_size;
    obscura64_provider_decoded_size_fn decoded_size;
    obscura64_provider_encode_fn encode;
    obscura64_provider_decode_fn decode;
} obscura64_provider;

/* Create a context using the configured default Provider. */
obscura64_status obscura64_context_create_from_profile(
    const char *profile,
    size_t profile_len,
    obscura64_context **out_context);

/* Open a project directory using a UTF-8 path and the configured Provider. */
obscura64_status obscura64_open(
    const char *project_path_utf8,
    obscura64_context **out_context);

/* Advanced open: use the supplied codec Provider for the project context. */
obscura64_status obscura64_open_with_provider(
    const char *project_path_utf8,
    const obscura64_provider *provider,
    obscura64_context **out_context);

/* Explicitly change the project Profile; existing contexts retain their Profile. */
obscura64_status obscura64_force_reinitialize(
    const char *project_path_utf8,
    obscura64_context **out_context);
obscura64_status obscura64_force_reinitialize_with_provider(
    const char *project_path_utf8,
    const obscura64_provider *provider,
    obscura64_context **out_context);

/* Advanced constructor: Provider is validated and copied into the context. */
obscura64_status obscura64_context_create_from_profile_with_provider(
    const char *profile,
    size_t profile_len,
    const obscura64_provider *provider,
    obscura64_context **out_context);

/* Static read-only built-in codec Provider; the caller must not free it. */
const obscura64_provider *obscura64_builtin_provider(void);

/* Release a context created by this module; NULL is accepted. */
void obscura64_context_destroy(obscura64_context *context);

/* Query the context Provider's encoded byte count. Zero input returns zero. */
obscura64_status obscura64_encoded_size(
    const obscura64_context *context,
    size_t input_len,
    size_t *encoded_len);

/* Validate encoded bytes and query decoded byte count; empty input is valid. */
obscura64_status obscura64_decoded_size(
    const obscura64_context *context,
    const char *encoded,
    size_t encoded_len,
    size_t *decoded_len);

/* Caller-buffer encoding; explicit binary input length; no NUL is appended. */
obscura64_status obscura64_encode(
    const obscura64_context *context,
    const void *input,
    size_t input_len,
    char *output,
    size_t output_capacity,
    size_t *output_len);

/* Caller-buffer decoding; explicit lengths; binary output is not terminated. */
obscura64_status obscura64_decode(
    const obscura64_context *context,
    const char *encoded,
    size_t encoded_len,
    void *output,
    size_t output_capacity,
    size_t *output_len);

/* Allocating form; outputs are length-delimited, not NUL-terminated. */
obscura64_status obscura64_encode_alloc(
    const obscura64_context *context,
    const void *input,
    size_t input_len,
    char **output,
    size_t *output_len);

/* Allocating form; outputs are binary and length-delimited. */
obscura64_status obscura64_decode_alloc(
    const obscura64_context *context,
    const char *encoded,
    size_t encoded_len,
    void **output,
    size_t *output_len);

/* Release any non-NULL buffer returned by an obscura64_*_alloc function. */
void obscura64_free(void *ptr);

/* Managed V1 envelope above the context Provider. SHA detects corruption/wrong
 * Profile, not attacker authentication. Explicit lengths; no NUL appended.
 * Empty input encodes a nonempty envelope. Size queries validate decoded data.
 * Caller buffers stay unchanged on failure. BUFFER_TOO_SMALL reports required
 * output_len; other failures report zero. NULL output/capacity zero queries size.
 * Alloc outputs use obscura64_free; failures clear pointer/length. Empty allocating
 * decode succeeds with NULL/zero, without malloc(0). */
obscura64_status obscura64_managed_encoded_size(const obscura64_context *context,
    size_t input_len, size_t *encoded_size);
obscura64_status obscura64_managed_decoded_size(const obscura64_context *context,
    const char *encoded, size_t encoded_len, size_t *decoded_size);
obscura64_status obscura64_managed_encode(const obscura64_context *context,
    const void *input, size_t input_len, char *output, size_t output_capacity, size_t *output_len);
obscura64_status obscura64_managed_decode(const obscura64_context *context,
    const char *encoded, size_t encoded_len, void *output, size_t output_capacity, size_t *output_len);
obscura64_status obscura64_managed_encode_alloc(const obscura64_context *context,
    const void *input, size_t input_len, char **output, size_t *output_len);
obscura64_status obscura64_managed_decode_alloc(const obscura64_context *context,
    const char *encoded, size_t encoded_len, void **output, size_t *output_len);

/* Return a static, read-only English status string. */
const char *obscura64_status_string(obscura64_status status);

#ifdef __cplusplus
}
#endif

#endif /* OBSCURA64_H */
