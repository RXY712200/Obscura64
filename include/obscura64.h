#ifndef OBSCURA64_H
#define OBSCURA64_H

/* Obscura64 public C API. */

#include <stddef.h>

#define OBSCURA64_VERSION_MAJOR 0
#define OBSCURA64_VERSION_MINOR 1
#define OBSCURA64_VERSION_PATCH 0
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
    OBSCURA64_BUSY
} obscura64_status;

typedef struct obscura64_context obscura64_context;

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

/* Return a static, read-only English status string. */
const char *obscura64_status_string(obscura64_status status);

#ifdef __cplusplus
}
#endif

#endif /* OBSCURA64_H */
