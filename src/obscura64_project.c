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
#define PROJECT_RACE_RETRIES 5U
#define PROJECT_RACE_DELAY_MS 10U
#define PROJECT_ID_ZERO_RETRIES 8U

static int is_path_not_found_error(DWORD error)
{
    return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
}

static obscura64_status convert_path_utf8(const char *path_utf8, WCHAR **path_out)
{
    int required;
    WCHAR *path;

    if (path_utf8 == NULL || path_out == NULL || path_utf8[0] == '\0') {
        return OBSCURA64_INVALID_ARGUMENT;
    }
    *path_out = NULL;
    required = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path_utf8, -1, NULL, 0);
    if (required == 0) {
        return OBSCURA64_INVALID_ARGUMENT;
    }
    if ((size_t)required > SIZE_MAX / sizeof(*path)) {
        return OBSCURA64_SIZE_OVERFLOW;
    }
    path = (WCHAR *)malloc((size_t)required * sizeof(*path));
    if (path == NULL) {
        return OBSCURA64_OUT_OF_MEMORY;
    }
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path_utf8, -1,
                            path, required) != required) {
        free(path);
        return OBSCURA64_INVALID_ARGUMENT;
    }
    *path_out = path;
    return OBSCURA64_OK;
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

static obscura64_status read_state_with_retries(
    const WCHAR *state_path,
    obscura64_project_state *state,
    unsigned int retries)
{
    unsigned int attempt;
    obscura64_status status = OBSCURA64_STATE_MISSING;
    for (attempt = 0; attempt < retries; ++attempt) {
        status = read_state_file(state_path, state);
        if (status == OBSCURA64_OK) {
            return status;
        }
        if (attempt + 1U < retries) {
            Sleep(PROJECT_RACE_DELAY_MS);
        }
    }
    return status;
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

static obscura64_status write_initial_state_file(
    const WCHAR *state_path,
    const unsigned char serialized[OBSCURA64_STATE_V1_SIZE],
    obscura64_project_state *state)
{
    obscura64_status status = obscura64_persistence_create_new_verified(
        state_path, serialized, OBSCURA64_STATE_V1_SIZE, verify_state, state);
    if (status == OBSCURA64_BUSY)
        return read_state_with_retries(state_path, state, PROJECT_RACE_RETRIES);
    return status;
}

static obscura64_status open_project_state(
    const WCHAR *project_path,
    obscura64_project_state *state)
{
    DWORD project_attributes = GetFileAttributesW(project_path);
    WCHAR *state_directory;
    WCHAR *state_path;
    DWORD directory_attributes;
    obscura64_status status;
    obscura64_project_state candidate_state;
    unsigned char candidate_blob[OBSCURA64_STATE_V1_SIZE];

    if (project_attributes == INVALID_FILE_ATTRIBUTES ||
        (project_attributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        return OBSCURA64_IO_ERROR;
    }
    state_directory = append_component(project_path, PROJECT_STATE_DIRECTORY);
    if (state_directory == NULL) {
        return OBSCURA64_OUT_OF_MEMORY;
    }
    state_path = append_component(state_directory, PROJECT_STATE_FILENAME);
    if (state_path == NULL) {
        free(state_directory);
        return OBSCURA64_OUT_OF_MEMORY;
    }

    directory_attributes = GetFileAttributesW(state_directory);
    if (directory_attributes != INVALID_FILE_ATTRIBUTES) {
        if ((directory_attributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
            status = OBSCURA64_IO_ERROR;
        } else {
            status = read_state_with_retries(state_path, state, PROJECT_RACE_RETRIES);
        }
        free(state_path);
        free(state_directory);
        return status;
    }

    if (!is_path_not_found_error(GetLastError())) {
        free(state_path);
        free(state_directory);
        return OBSCURA64_IO_ERROR;
    }
    status = prepare_initial_state(&candidate_state, candidate_blob);
    if (status != OBSCURA64_OK) {
        free(state_path);
        free(state_directory);
        return status;
    }
    if (CreateDirectoryW(state_directory, NULL)) {
        status = write_initial_state_file(state_path, candidate_blob, state);
        free(state_path);
        free(state_directory);
        return status;
    }

    {
        DWORD error = GetLastError();
        unsigned int attempt;
        if (error != ERROR_ALREADY_EXISTS && error != ERROR_FILE_EXISTS) {
            free(state_path);
            free(state_directory);
            return OBSCURA64_IO_ERROR;
        }
        status = OBSCURA64_STATE_MISSING;
        for (attempt = 0; attempt < PROJECT_RACE_RETRIES; ++attempt) {
            directory_attributes = GetFileAttributesW(state_directory);
            if (directory_attributes != INVALID_FILE_ATTRIBUTES) {
                if ((directory_attributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
                    status = OBSCURA64_IO_ERROR;
                    break;
                }
                status = read_state_file(state_path, state);
                if (status == OBSCURA64_OK || status == OBSCURA64_IO_ERROR) {
                    break;
                }
            }
            if (attempt + 1U < PROJECT_RACE_RETRIES) {
                Sleep(PROJECT_RACE_DELAY_MS);
            }
        }
    }
    free(state_path);
    free(state_directory);
    return status;
}

obscura64_status obscura64_project_open_internal(
    const char *project_path_utf8,
    const obscura64_provider *provider,
    obscura64_context **out_context)
{
    WCHAR *project_path = NULL;
    obscura64_project_state state;
    obscura64_context *provider_probe = NULL;
    obscura64_status status;

    if (out_context == NULL) {
        return OBSCURA64_INVALID_ARGUMENT;
    }
    *out_context = NULL;
    status = convert_path_utf8(project_path_utf8, &project_path);
    if (status != OBSCURA64_OK) {
        return status;
    }
    status = obscura64_context_create_from_profile_with_provider(
        obscura64_v1_character_pool, OBSCURA64_PROFILE_SIZE, provider, &provider_probe);
    if (status != OBSCURA64_OK) {
        free(project_path);
        return status;
    }
    obscura64_context_destroy(provider_probe);

    status = open_project_state(project_path, &state);
    free(project_path);
    if (status != OBSCURA64_OK) {
        return status;
    }
    return obscura64_context_create_from_profile_with_provider(
        state.profile, OBSCURA64_PROFILE_SIZE, provider, out_context);
}

/* Preserve the given newest-first order; the generic history parser stays strict.
 * Only entry 0 may overlap current after a prior current-commit failure. */
static obscura64_status build_force_history(
    const obscura64_project_state *current,
    const obscura64_project_history *existing,
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
        for (i = 0; i < existing->count; ++i) {
            const obscura64_history_entry *entry = &existing->entries[i];
            if (entry->generation > current->generation) return OBSCURA64_STATE_CORRUPT;
            if (entry->generation == current->generation) {
                if (i != 0 || memcmp(entry->profile, current->profile, OBSCURA64_PROFILE_SIZE) != 0)
                    return OBSCURA64_STATE_CORRUPT;
                continue;
            }
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
    HANDLE lock = INVALID_HANDLE_VALUE;
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

    if (out_context == NULL) return OBSCURA64_INVALID_ARGUMENT;
    *out_context = NULL;
    status = convert_path_utf8(project_path_utf8, &project);
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
    lock = CreateFileW(lock_path, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (lock == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        status = (error == ERROR_FILE_EXISTS || error == ERROR_ALREADY_EXISTS ||
                  GetFileAttributesW(lock_path) != INVALID_FILE_ATTRIBUTES) ?
            OBSCURA64_BUSY : OBSCURA64_IO_ERROR;
        goto cleanup;
    }
    status = read_state_file(current_path, &old_state);
    if (status != OBSCURA64_OK) goto cleanup;
    if (old_state.generation == UINT64_MAX) { status = OBSCURA64_SIZE_OVERFLOW; goto cleanup; }
    status = obscura64_persistence_read_verified(history_path, old_history_bytes,
        OBSCURA64_HISTORY_V1_SIZE, verify_history, &old_history);
    if (status == OBSCURA64_OK) {
        if (memcmp(old_history.project_id, old_state.project_id, OBSCURA64_PROJECT_ID_SIZE) != 0) {
            status = OBSCURA64_STATE_CORRUPT; goto cleanup;
        }
        history_exists = 1;
    } else if (status != OBSCURA64_STATE_MISSING) {
        goto cleanup;
    }
    status = build_force_history(&old_state, history_exists ? &old_history : NULL, &new_history);
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
    *out_context = reserved_context;
    reserved_context = NULL;
cleanup:
    if (lock != INVALID_HANDLE_VALUE) {
        (void)CloseHandle(lock);
        (void)DeleteFileW(lock_path);
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
