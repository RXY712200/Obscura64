#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>

#include "obscura64_internal.h"
#include "obscura64_profiles_v1.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#define PROJECT_STATE_DIRECTORY L".obscura64"
#define PROJECT_STATE_FILENAME L"current.state"
#define PROJECT_RACE_RETRIES 25U
#define PROJECT_RACE_DELAY_MS 10U
#define PROJECT_ID_ZERO_RETRIES 8U

static int is_path_not_found_error(DWORD error)
{
    return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
}

static WCHAR *append_component(const WCHAR *base, const WCHAR *component)
{
    size_t base_len = wcslen(base);
    size_t component_len = wcslen(component);
    int add_separator = base_len != 0 && base[base_len - 1] != L'\\' &&
                        base[base_len - 1] != L'/';
    size_t separator_count = add_separator ? 1U : 0U;
    size_t allocation_count;
    WCHAR *joined;

    if (base_len > SIZE_MAX - component_len - separator_count - 1U) {
        return NULL;
    }
    allocation_count = base_len + separator_count + component_len + 1U;
    if (allocation_count > SIZE_MAX / sizeof(*joined)) {
        return NULL;
    }
    joined = (WCHAR *)malloc(allocation_count * sizeof(*joined));
    if (joined == NULL) {
        return NULL;
    }
    memcpy(joined, base, base_len * sizeof(*joined));
    if (add_separator) {
        joined[base_len++] = L'\\';
    }
    memcpy(joined + base_len, component, (component_len + 1U) * sizeof(*joined));
    return joined;
}

static obscura64_status verify_state(
    const unsigned char *data, size_t size, void *user_data)
{
    obscura64_project_state state;
    obscura64_core_status parsed = obscura64_state_deserialize(data, size, &state);
    if (parsed != OBSCURA64_CORE_STATUS_SUCCESS)
        return parsed == OBSCURA64_CORE_STATUS_CRYPTO_FAILURE ? OBSCURA64_IO_ERROR : OBSCURA64_STATE_CORRUPT;
    if (user_data != NULL) *(obscura64_project_state *)user_data = state;
    return OBSCURA64_OK;
}

static obscura64_status verify_history(
    const unsigned char *data, size_t size, void *user_data)
{
    obscura64_project_history history;
    obscura64_core_status parsed = obscura64_history_deserialize(data, size, &history);
    if (parsed != OBSCURA64_CORE_STATUS_SUCCESS)
        return parsed == OBSCURA64_CORE_STATUS_CRYPTO_FAILURE ? OBSCURA64_IO_ERROR : OBSCURA64_STATE_CORRUPT;
    if (user_data != NULL) *(obscura64_project_history *)user_data = history;
    return OBSCURA64_OK;
}

static obscura64_status read_state_file(
    const WCHAR *state_path, obscura64_project_state *state)
{
    unsigned char bytes[OBSCURA64_STATE_V1_SIZE];
    return obscura64_persistence_read_verified(state_path, bytes, sizeof(bytes), verify_state, state);
}

static int project_id_is_nonzero(const uint8_t project_id[OBSCURA64_PROJECT_ID_SIZE])
{
    size_t i;
    uint8_t aggregate = 0;
    for (i = 0; i < OBSCURA64_PROJECT_ID_SIZE; ++i) {
        aggregate |= project_id[i];
    }
    return aggregate != 0;
}

static obscura64_status generate_project_id(uint8_t project_id[OBSCURA64_PROJECT_ID_SIZE])
{
    unsigned int attempt;
    for (attempt = 0; attempt < PROJECT_ID_ZERO_RETRIES; ++attempt) {
        NTSTATUS status = BCryptGenRandom(NULL, project_id, OBSCURA64_PROJECT_ID_SIZE,
                                         BCRYPT_USE_SYSTEM_PREFERRED_RNG);
        if (!BCRYPT_SUCCESS(status)) {
            return OBSCURA64_RNG_FAILURE;
        }
        if (project_id_is_nonzero(project_id)) {
            return OBSCURA64_OK;
        }
    }
    return OBSCURA64_RNG_FAILURE;
}

static obscura64_status map_random_profile_status(obscura64_core_status status)
{
    if (status == OBSCURA64_CORE_STATUS_SUCCESS) {
        return OBSCURA64_OK;
    }
    if (status == OBSCURA64_CORE_STATUS_RNG_FAILURE) {
        return OBSCURA64_RNG_FAILURE;
    }
    return OBSCURA64_IO_ERROR;
}

static obscura64_status prepare_initial_state(
    obscura64_project_state *state,
    unsigned char serialized[OBSCURA64_STATE_V1_SIZE])
{
    uint16_t selected_profile_id;
    uint8_t selected_profile[OBSCURA64_PROFILE_SIZE];
    obscura64_core_status serialize_status;
    obscura64_status status;

    memset(state, 0, sizeof(*state));
    status = generate_project_id(state->project_id);
    if (status != OBSCURA64_OK) {
        return status;
    }
    status = map_random_profile_status(obscura64_profile_runtime_select_random(
        &selected_profile_id, selected_profile));
    if (status != OBSCURA64_OK) {
        return status;
    }
    (void)selected_profile_id;
    state->generation = 1;
    memcpy(state->profile, selected_profile, sizeof(selected_profile));

    serialize_status = obscura64_state_serialize(state, serialized);
    if (serialize_status != OBSCURA64_CORE_STATUS_SUCCESS) {
        return serialize_status == OBSCURA64_CORE_STATUS_CRYPTO_FAILURE ?
            OBSCURA64_IO_ERROR : OBSCURA64_STATE_CORRUPT;
    }

    return OBSCURA64_OK;
}

obscura64_status obscura64_project_open_internal(
    const char *project_path_utf8, const obscura64_provider *provider,
    obscura64_context **out_context)
{
    WCHAR *project = NULL, *directory = NULL, *current = NULL, *lock_path = NULL;
    obscura64_lock *lock = NULL;
    obscura64_context *context = NULL;
    obscura64_project_state state;
    unsigned char blob[OBSCURA64_STATE_V1_SIZE];
    DWORD attributes;
    int fresh = 0, initializing;
    unsigned int attempt;
    obscura64_status status;
    if (out_context == NULL) return OBSCURA64_INVALID_ARGUMENT;
    *out_context = NULL;
    status = obscura64_path_from_utf8(project_path_utf8, &project);
    if (status != OBSCURA64_OK) return status;
    status = obscura64_context_create_from_profile_with_provider(
        obscura64_v1_character_pool, OBSCURA64_PROFILE_SIZE, provider, &context);
    if (status != OBSCURA64_OK) goto done;
    attributes = GetFileAttributesW(project);
    if (attributes == INVALID_FILE_ATTRIBUTES || !(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
        status = OBSCURA64_IO_ERROR; goto done;
    }
    directory = append_component(project, PROJECT_STATE_DIRECTORY);
    if (directory == NULL) { status = OBSCURA64_OUT_OF_MEMORY; goto done; }
    attributes = GetFileAttributesW(directory);
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        if (!is_path_not_found_error(GetLastError())) { status = OBSCURA64_IO_ERROR; goto done; }
        fresh = 1;
        if (!CreateDirectoryW(directory, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
            status = OBSCURA64_IO_ERROR; goto done;
        }
    } else if (!(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
        status = OBSCURA64_IO_ERROR; goto done;
    }
    current = append_component(directory, PROJECT_STATE_FILENAME);
    lock_path = append_component(directory, L"operation.lock");
    if (current == NULL || lock_path == NULL) { status = OBSCURA64_OUT_OF_MEMORY; goto done; }
    /* Only first-init coordination gets the bounded 240ms race grace. Existing
     * healthy projects never retry lock contention. A pre-existing empty container
     * is never initialized unless this call originally observed it absent. */
    initializing = fresh || (GetFileAttributesW(current) == INVALID_FILE_ATTRIBUTES &&
                              is_path_not_found_error(GetLastError()));
    for (attempt = 0; attempt < PROJECT_RACE_RETRIES; ++attempt) {
        status = fresh ? obscura64_lock_acquire_initial(lock_path, &lock) :
                         obscura64_lock_acquire_shared(lock_path, &lock);
        if (status == OBSCURA64_BUSY && !fresh) {
            int active = 0;
            obscura64_status probe = obscura64_lock_initialization_active(lock_path, &active);
            if (probe != OBSCURA64_OK) { status = probe; break; }
            if (!active) {
                /* Holder may have exited between failed acquire and marker probe.
                 * One immediate recheck avoids misclassifying that init race. */
                status = obscura64_lock_acquire_shared(lock_path, &lock);
                if (status != OBSCURA64_OK &&
                    !(status == OBSCURA64_BUSY && initializing)) break;
            } else initializing = 1;
        }
        if (status == OBSCURA64_OK) {
            status = read_state_file(current, &state);
            if (status == OBSCURA64_STATE_MISSING && fresh) {
                status = prepare_initial_state(&state, blob);
                if (status == OBSCURA64_OK)
                    status = obscura64_persistence_create_new_verified(current, blob,
                        sizeof(blob), verify_state, &state);
                if (status == OBSCURA64_BUSY) status = read_state_file(current, &state);
            }
            if (status == OBSCURA64_OK) {
                memcpy(context->profile, state.profile, OBSCURA64_PROFILE_SIZE);
                break;
            }
            if (obscura64_lock_release(lock) != OBSCURA64_OK) status = OBSCURA64_IO_ERROR;
            lock = NULL;
        }
        if (!initializing || (status != OBSCURA64_BUSY && status != OBSCURA64_STATE_MISSING)) break;
        if (attempt + 1U < PROJECT_RACE_RETRIES) Sleep(PROJECT_RACE_DELAY_MS);
    }
done:
    if (lock != NULL && obscura64_lock_release(lock) != OBSCURA64_OK) status = OBSCURA64_IO_ERROR;
    lock = NULL;
    if (!fresh && directory != NULL && lock_path != NULL &&
        (status == OBSCURA64_STATE_MISSING || status == OBSCURA64_STATE_CORRUPT)) {
        status = obscura64_lock_acquire_exclusive(lock_path, &lock);
        if (status == OBSCURA64_OK) {
            status = obscura64_recovery_ensure(project, directory, 0, &state);
            if (status == OBSCURA64_OK) memcpy(context->profile, state.profile, OBSCURA64_PROFILE_SIZE);
            if (obscura64_lock_release(lock) != OBSCURA64_OK) status = OBSCURA64_IO_ERROR;
            lock = NULL;
        }
    }
    /* Maintenance never demotes a successfully constructed current Context.
       Re-read under exclusive lock: another Force may have committed meanwhile. */
    if (status == OBSCURA64_OK &&
        obscura64_lock_acquire_exclusive(lock_path, &lock) == OBSCURA64_OK) {
        obscura64_recovery_maintain(project, directory);
        (void)obscura64_lock_release(lock);
        lock = NULL;
    }
    if (status == OBSCURA64_OK) { *out_context = context; context = NULL; }
    obscura64_context_destroy(context);
    free(lock_path); free(current); free(directory); free(project);
    return status;
}

/* Managed-set validation precedes this builder. Keep newest-first order and
 * include a validated current/history overlap only once. */
static obscura64_status build_force_history(
    const obscura64_project_state *current,
    const obscura64_project_history *existing,
    obscura64_managed_state_kind kind,
    obscura64_project_history *result)
{
    obscura64_project_history temporary;
    size_t i;
    memset(&temporary, 0, sizeof(temporary));
    memcpy(temporary.project_id, current->project_id, OBSCURA64_PROJECT_ID_SIZE);
    temporary.count = 1;
    temporary.entries[0].generation = current->generation;
    memcpy(temporary.entries[0].profile, current->profile, OBSCURA64_PROFILE_SIZE);
    if (existing != NULL) {
        for (i = kind == OBSCURA64_MANAGED_OVERLAP ? 1U : 0U; i < existing->count; ++i) {
            const obscura64_history_entry *entry = &existing->entries[i];
            if (temporary.count < OBSCURA64_HISTORY_CAPACITY)
                temporary.entries[temporary.count++] = *entry;
        }
    }
    *result = temporary;
    return OBSCURA64_OK;
}

static obscura64_status force_reinitialize_internal(
    const char *project_path_utf8, const obscura64_provider *provider,
    obscura64_context **out_context)
{
    WCHAR *project = NULL, *directory = NULL, *current_path = NULL;
    WCHAR *history_path = NULL, *lock_path = NULL;
    obscura64_lock *lock = NULL;
    obscura64_context *reserved_context = NULL;
    obscura64_project_state old_state, new_state;
    obscura64_project_history old_history, new_history;
    unsigned char current_bytes[OBSCURA64_STATE_V1_SIZE];
    unsigned char new_current_bytes[OBSCURA64_STATE_V1_SIZE];
    unsigned char old_history_bytes[OBSCURA64_HISTORY_V1_SIZE];
    unsigned char history_bytes[OBSCURA64_HISTORY_V1_SIZE];
    int history_exists = 0;
    unsigned int attempt;
    uint16_t profile_id;
    DWORD attributes;
    obscura64_status status;
    obscura64_core_status core_status;
    obscura64_managed_state_kind managed_kind;

    if (out_context == NULL) return OBSCURA64_INVALID_ARGUMENT;
    *out_context = NULL;
    status = obscura64_path_from_utf8(project_path_utf8, &project);
    if (status != OBSCURA64_OK) return status;
    status = obscura64_context_create_from_profile_with_provider(
        obscura64_v1_character_pool, OBSCURA64_PROFILE_SIZE, provider, &reserved_context);
    if (status != OBSCURA64_OK) goto cleanup;
    attributes = GetFileAttributesW(project);
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        status = OBSCURA64_IO_ERROR; goto cleanup;
    }
    directory = append_component(project, PROJECT_STATE_DIRECTORY);
    if (directory == NULL) { status = OBSCURA64_OUT_OF_MEMORY; goto cleanup; }
    attributes = GetFileAttributesW(directory);
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        status = is_path_not_found_error(GetLastError()) ? OBSCURA64_STATE_MISSING : OBSCURA64_IO_ERROR;
        goto cleanup;
    }
    if ((attributes & FILE_ATTRIBUTE_DIRECTORY) == 0) { status = OBSCURA64_IO_ERROR; goto cleanup; }
    current_path = append_component(directory, L"current.state");
    history_path = append_component(directory, L"history.state");
    lock_path = append_component(directory, L"operation.lock");
    if (current_path == NULL || history_path == NULL || lock_path == NULL) {
        status = OBSCURA64_OUT_OF_MEMORY; goto cleanup;
    }
    status = obscura64_lock_acquire_exclusive(lock_path, &lock);
    if (status != OBSCURA64_OK) goto cleanup;
    status = read_state_file(current_path, &old_state);
    if (status == OBSCURA64_OK && old_state.generation == UINT64_MAX) {
        status = OBSCURA64_SIZE_OVERFLOW; goto cleanup;
    }
    if (status != OBSCURA64_OK && status != OBSCURA64_STATE_MISSING && status != OBSCURA64_STATE_CORRUPT)
        goto cleanup;
    status = obscura64_recovery_ensure(project, directory, 1, &old_state);
    if (status != OBSCURA64_OK) goto cleanup;
    if (old_state.generation == UINT64_MAX) { status = OBSCURA64_SIZE_OVERFLOW; goto cleanup; }
    status = obscura64_persistence_read_verified(history_path, old_history_bytes,
        OBSCURA64_HISTORY_V1_SIZE, verify_history, &old_history);
    if (status == OBSCURA64_OK) {
        history_exists = 1;
    } else if (status != OBSCURA64_STATE_MISSING) {
        goto cleanup;
    }
    status = obscura64_managed_state_validate(&old_state,
        history_exists ? &old_history : NULL, &managed_kind);
    if (status != OBSCURA64_OK) goto cleanup;
    status = build_force_history(&old_state, history_exists ? &old_history : NULL,
        managed_kind, &new_history);
    if (status != OBSCURA64_OK) goto cleanup;
    core_status = obscura64_history_serialize(&new_history, history_bytes);
    if (core_status != OBSCURA64_CORE_STATUS_SUCCESS) { status = OBSCURA64_IO_ERROR; goto cleanup; }
    status = obscura64_persistence_replace_verified(history_path, history_bytes,
        sizeof(history_bytes), verify_history, NULL,
        history_exists ? old_history_bytes : NULL);
    if (status != OBSCURA64_OK) goto cleanup;

    new_state = old_state;
    new_state.generation++;
    for (attempt = 0; attempt < 128U; ++attempt) {
        uint8_t selected[OBSCURA64_PROFILE_SIZE];
        status = map_random_profile_status(obscura64_profile_runtime_select_random(&profile_id, selected));
        if (status != OBSCURA64_OK) goto cleanup;
        if (memcmp(selected, old_state.profile, OBSCURA64_PROFILE_SIZE) != 0) {
            memcpy(new_state.profile, selected, OBSCURA64_PROFILE_SIZE);
            break;
        }
    }
    if (attempt == 128U) { status = OBSCURA64_RNG_FAILURE; goto cleanup; }
    if (obscura64_state_serialize(&old_state, current_bytes) != OBSCURA64_CORE_STATUS_SUCCESS ||
        obscura64_state_serialize(&new_state, new_current_bytes) != OBSCURA64_CORE_STATUS_SUCCESS) {
        status = OBSCURA64_IO_ERROR; goto cleanup;
    }
    status = obscura64_persistence_replace_verified(current_path, new_current_bytes,
        sizeof(new_current_bytes), verify_state, NULL, current_bytes);
    if (status != OBSCURA64_OK) goto cleanup;
    /* The provider-validated context was reserved before any disk change to avoid
     * reporting allocation failure after committing the new generation. */
    memcpy(reserved_context->profile, new_state.profile, OBSCURA64_PROFILE_SIZE);
    (void)obscura64_redundancy_sync(project, &new_state, &new_history);
    *out_context = reserved_context;
    reserved_context = NULL;
cleanup:
    if (lock != NULL && obscura64_lock_release(lock) != OBSCURA64_OK) {
        status = OBSCURA64_IO_ERROR;
        obscura64_context_destroy(*out_context);
        *out_context = NULL;
    }
    obscura64_context_destroy(reserved_context);
    free(lock_path); free(history_path); free(current_path); free(directory); free(project);
    return status;
}

obscura64_status obscura64_force_reinitialize(
    const char *project_path_utf8, obscura64_context **out_context)
{
    return force_reinitialize_internal(project_path_utf8, &OBSCURA64_DEFAULT_PROVIDER_SYMBOL, out_context);
}

obscura64_status obscura64_force_reinitialize_with_provider(
    const char *project_path_utf8, const obscura64_provider *provider,
    obscura64_context **out_context)
{
    return force_reinitialize_internal(project_path_utf8, provider, out_context);
}
