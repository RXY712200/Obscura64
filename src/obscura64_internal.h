#ifndef OBSCURA64_INTERNAL_H
#define OBSCURA64_INTERNAL_H

#include "obscura64.h"

#include <stddef.h>
#include <stdint.h>
#include <wchar.h>

typedef struct obscura64_snapshot_paths {
    wchar_t *directory;
    wchar_t *current;
    wchar_t *history;
} obscura64_snapshot_paths;
obscura64_status obscura64_redundancy_paths(const wchar_t *project,
    int create, obscura64_snapshot_paths *paths);
void obscura64_snapshot_paths_free(obscura64_snapshot_paths *paths);

typedef struct obscura64_lock obscura64_lock;
obscura64_status obscura64_lock_acquire_shared(const wchar_t *path, obscura64_lock **lock);
obscura64_status obscura64_lock_acquire_exclusive(const wchar_t *path, obscura64_lock **lock);
/* Initial writer locks coordination byte 0 plus transient marker byte 1.
 * The marker is kernel-only; it distinguishes initialization from normal BUSY. */
obscura64_status obscura64_lock_acquire_initial(const wchar_t *path, obscura64_lock **lock);
obscura64_status obscura64_lock_initialization_active(const wchar_t *path, int *active);
obscura64_status obscura64_lock_release(obscura64_lock *lock);

#ifdef OBSCURA64_TESTING
typedef enum obscura64_test_fault_point {
    OBSCURA64_FAULT_TEMP_CREATE,
    OBSCURA64_FAULT_TEMP_WRITE,
    OBSCURA64_FAULT_TEMP_FLUSH,
    OBSCURA64_FAULT_TEMP_REOPEN,
    OBSCURA64_FAULT_PRE_VERIFY,
    OBSCURA64_FAULT_REPLACE,
    OBSCURA64_FAULT_FINAL_REOPEN,
    OBSCURA64_FAULT_POST_VERIFY,
    OBSCURA64_FAULT_AFTER_REPLACE
} obscura64_test_fault_point;
/* Supplied only by the dedicated test executable; no production hook state. */
int obscura64_test_fault(obscura64_test_fault_point point,
    const wchar_t *target, const wchar_t *temporary);
#endif

/* Windows-private byte persistence; verifiers own format semantics. */
typedef obscura64_status (*obscura64_persistence_verify_fn)(
    const unsigned char *data, size_t length, void *user_data);
obscura64_status obscura64_persistence_read_verified(
    const wchar_t *path, unsigned char *output, size_t expected_size,
    obscura64_persistence_verify_fn verify, void *user_data);
/* BUSY means CREATE_NEW lost the race; the existing file is untouched. */
obscura64_status obscura64_persistence_create_new_verified(
    const wchar_t *path, const unsigned char *data, size_t size,
    obscura64_persistence_verify_fn verify, void *user_data);
/* Optional old_data preserves the prior best-effort post-rename rollback. */
obscura64_status obscura64_persistence_replace_verified(
    const wchar_t *path, const unsigned char *data, size_t size,
    obscura64_persistence_verify_fn verify, void *user_data,
    const unsigned char *old_data);

#define OBSCURA64_ALPHABET_SIZE OBSCURA64_PROFILE_SIZE
#define OBSCURA64_REVERSE_INVALID UINT8_C(0xFF)

#ifndef OBSCURA64_DEFAULT_PROVIDER_SYMBOL
#define OBSCURA64_DEFAULT_PROVIDER_SYMBOL obscura64_builtin_provider_instance
#endif

extern const obscura64_provider OBSCURA64_DEFAULT_PROVIDER_SYMBOL;

struct obscura64_context {
    char profile[OBSCURA64_ALPHABET_SIZE];
    obscura64_provider provider;
};

typedef enum obscura64_core_status {
    OBSCURA64_CORE_STATUS_SUCCESS = 0,
    OBSCURA64_CORE_STATUS_INVALID_ARGUMENT,
    OBSCURA64_CORE_STATUS_INVALID_PROFILE,
    OBSCURA64_CORE_STATUS_INVALID_DATA,
    OBSCURA64_CORE_STATUS_BUFFER_TOO_SMALL,
    OBSCURA64_CORE_STATUS_SIZE_OVERFLOW,
    OBSCURA64_CORE_STATUS_RNG_FAILURE,
    OBSCURA64_CORE_STATUS_CRYPTO_FAILURE
} obscura64_core_status;

#define OBSCURA64_PROJECT_ID_SIZE 16U
#define OBSCURA64_STATE_V1_SIZE 160U
#define OBSCURA64_STATE_V1_HEADER_SIZE 64U
#define OBSCURA64_STATE_V1_HASH_OFFSET 128U

typedef struct obscura64_project_state {
    uint8_t project_id[OBSCURA64_PROJECT_ID_SIZE];
    uint64_t generation;
    char profile[OBSCURA64_PROFILE_SIZE];
} obscura64_project_state;

#define OBSCURA64_HISTORY_V1_SIZE 336U
#define OBSCURA64_HISTORY_CAPACITY 3U
#define OBSCURA64_HISTORY_ENTRY_SIZE 80U
#define OBSCURA64_HISTORY_HASH_OFFSET 304U

typedef struct obscura64_history_entry {
    uint64_t generation;
    char profile[OBSCURA64_PROFILE_SIZE];
} obscura64_history_entry;

typedef struct obscura64_project_history {
    uint8_t project_id[OBSCURA64_PROJECT_ID_SIZE];
    uint16_t count;
    obscura64_history_entry entries[OBSCURA64_HISTORY_CAPACITY];
} obscura64_project_history;

obscura64_status obscura64_snapshot_read_current(const wchar_t *path, obscura64_project_state *state);
obscura64_status obscura64_snapshot_read_history(const wchar_t *path, obscura64_project_history *history);
obscura64_status obscura64_snapshot_write_current(const wchar_t *path, const obscura64_project_state *state);
obscura64_status obscura64_snapshot_write_history(const wchar_t *path, const obscura64_project_history *history);
/* Caller holds the project exclusive lock. No policy decisions in the mirror. */
obscura64_status obscura64_redundancy_sync(const wchar_t *project,
    const obscura64_project_state *state, const obscura64_project_history *history);
/* Single recovery decision path; caller owns exclusive project coordination. */
obscura64_status obscura64_recovery_ensure(const wchar_t *project, const wchar_t *directory,
    int require_managed, obscura64_project_state *state);
/* Re-reads disk state under the caller's exclusive lock before best-effort sync. */
void obscura64_recovery_maintain(const wchar_t *project, const wchar_t *directory);

/* Force-only cross-file validation. NULL history means the file is absent.
 * Does not read, mutate, repair or recover either authoritative file.
 * Optional kind is published only on success. */
typedef enum obscura64_managed_state_kind {
    OBSCURA64_MANAGED_STEADY,
    OBSCURA64_MANAGED_OVERLAP
} obscura64_managed_state_kind;
obscura64_status obscura64_managed_state_validate(
    const obscura64_project_state *current,
    const obscura64_project_history *history,
    obscura64_managed_state_kind *kind);

int obscura64_history_validate(const obscura64_project_history *history);
obscura64_core_status obscura64_history_serialize(
    const obscura64_project_history *history,
    unsigned char output[OBSCURA64_HISTORY_V1_SIZE]);
obscura64_core_status obscura64_history_deserialize(
    const unsigned char *data, size_t length,
    obscura64_project_history *history);

extern const char obscura64_v1_character_pool[OBSCURA64_ALPHABET_SIZE + 1];

int obscura64_profile_is_valid(const char *profile, size_t length);
int obscura64_profile_build_reverse_map(
    const char *profile,
    size_t length,
    uint8_t reverse_map[256]);

obscura64_core_status obscura64_codec_encoded_size(size_t input_len, size_t *output_len);
obscura64_core_status obscura64_codec_decoded_size(
    const char *input,
    size_t input_len,
    const char *profile,
    size_t profile_len,
    size_t *output_len);
obscura64_core_status obscura64_codec_encode(
    const uint8_t *input,
    size_t input_len,
    const char *profile,
    size_t profile_len,
    char *output,
    size_t output_capacity,
    size_t *output_len);
obscura64_core_status obscura64_codec_decode(
    const char *input,
    size_t input_len,
    const char *profile,
    size_t profile_len,
    uint8_t *output,
    size_t output_capacity,
    size_t *output_len);

obscura64_core_status obscura64_profile_library_get(
    uint16_t profile_id,
    uint8_t profile_out[OBSCURA64_ALPHABET_SIZE]);
int obscura64_profile_library_find(
    const unsigned char profile[OBSCURA64_PROFILE_SIZE],
    uint16_t *profile_id);
obscura64_core_status obscura64_profile_runtime_select_random(
    uint16_t *profile_id,
    uint8_t profile_out[OBSCURA64_ALPHABET_SIZE]);

int obscura64_state_validate(const obscura64_project_state *state);
obscura64_core_status obscura64_state_serialize(
    const obscura64_project_state *state,
    unsigned char output[OBSCURA64_STATE_V1_SIZE]);
obscura64_core_status obscura64_state_deserialize(
    const unsigned char *data,
    size_t data_len,
    obscura64_project_state *out_state);

obscura64_status obscura64_project_open_internal(
    const char *project_path_utf8,
    const obscura64_provider *provider,
    obscura64_context **out_context);

obscura64_status obscura64_managed_payload_build(const void *data, size_t length, unsigned char **envelope, size_t *size);
obscura64_status obscura64_managed_payload_parse(const unsigned char *envelope, size_t size, const unsigned char **payload, size_t *length);
typedef enum obscura64_disaster_status {
    OBSCURA64_DISASTER_SUCCESS, OBSCURA64_DISASTER_NOT_FOUND,
    OBSCURA64_DISASTER_AMBIGUOUS, OBSCURA64_DISASTER_ERROR
} obscura64_disaster_status;
typedef struct obscura64_disaster_result {
    uint16_t profile_id;
    unsigned char profile[64];
    void *payload;
    size_t payload_size;
    size_t attempts;
    /* Private diagnostics: full decodes includes the unique winner re-decode. */
    size_t candidates;
    size_t full_decodes;
} obscura64_disaster_result;
/* Result is reset on entry. Only SUCCESS owns payload (free with obscura64_free);
 * release a previous successful result before reusing it. No project mutation. */
obscura64_disaster_status obscura64_disaster_scan(const char *encoded, size_t size, obscura64_disaster_result *result);
#ifdef OBSCURA64_DISASTER_TESTING
obscura64_disaster_status obscura64_disaster_scan_candidates(const char *encoded, size_t size,
    const uint16_t *ids, size_t count, obscura64_disaster_result *result);
#endif

#endif /* OBSCURA64_INTERNAL_H */
