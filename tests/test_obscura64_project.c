#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "obscura64_internal.h"
#include "obscura64_profiles_v1.h"

#include <stdio.h>
#include <string.h>
#include <wchar.h>

static unsigned int checks;

#define CHECK(condition, label) do { \
    ++checks; \
    if (!(condition)) { \
        fprintf(stderr, "FAIL: %s\n", label); \
        return 1; \
    } \
} while (0)

static int wide_to_utf8(const WCHAR *wide, char *utf8, int capacity)
{
    return WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide, -1,
                               utf8, capacity, NULL, NULL) != 0;
}

static int make_child(WCHAR *output, size_t capacity, const WCHAR *parent, const WCHAR *suffix)
{
    size_t parent_len = wcslen(parent);
    size_t suffix_len = wcslen(suffix);
    if (parent_len + suffix_len + 1 > capacity) return 0;
    memcpy(output, parent, parent_len * sizeof(*output));
    memcpy(output + parent_len, suffix, (suffix_len + 1) * sizeof(*output));
    return 1;
}

static int read_state_bytes(const WCHAR *project, unsigned char bytes[OBSCURA64_STATE_V1_SIZE])
{
    WCHAR state_directory[MAX_PATH * 2];
    WCHAR state_path[MAX_PATH * 2];
    HANDLE file;
    DWORD read_count = 0;
    LARGE_INTEGER file_size;
    if (!make_child(state_directory, sizeof(state_directory) / sizeof(state_directory[0]),
                    project, L"\\.obscura64") ||
        !make_child(state_path, sizeof(state_path) / sizeof(state_path[0]),
                    state_directory, L"\\current.state")) return 0;
    file = CreateFileW(state_path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                       NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return 0;
    if (!GetFileSizeEx(file, &file_size) || file_size.QuadPart != OBSCURA64_STATE_V1_SIZE ||
        !ReadFile(file, bytes, OBSCURA64_STATE_V1_SIZE, &read_count, NULL) ||
        read_count != OBSCURA64_STATE_V1_SIZE) {
        CloseHandle(file);
        return 0;
    }
    return CloseHandle(file) != 0;
}

static int overwrite_state_bytes(const WCHAR *project, const unsigned char *bytes)
{
    WCHAR state_directory[MAX_PATH * 2];
    WCHAR state_path[MAX_PATH * 2];
    HANDLE file;
    DWORD written = 0;
    if (!make_child(state_directory, sizeof(state_directory) / sizeof(state_directory[0]),
                    project, L"\\.obscura64") ||
        !make_child(state_path, sizeof(state_path) / sizeof(state_path[0]),
                    state_directory, L"\\current.state")) return 0;
    file = CreateFileW(state_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                       FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return 0;
    if (!WriteFile(file, bytes, OBSCURA64_STATE_V1_SIZE, &written, NULL) ||
        written != OBSCURA64_STATE_V1_SIZE || !FlushFileBuffers(file)) {
        CloseHandle(file);
        return 0;
    }
    return CloseHandle(file) != 0;
}

static int delete_current_state(const WCHAR *project)
{
    WCHAR state_directory[MAX_PATH * 2];
    WCHAR state_path[MAX_PATH * 2];
    if (!make_child(state_directory, sizeof(state_directory) / sizeof(state_directory[0]),
                    project, L"\\.obscura64") ||
        !make_child(state_path, sizeof(state_path) / sizeof(state_path[0]),
                    state_directory, L"\\current.state")) return 0;
    return DeleteFileW(state_path) != 0;
}

static void cleanup_project(const WCHAR *project)
{
    WCHAR state_directory[MAX_PATH * 2];
    WCHAR state_path[MAX_PATH * 2];
    if (make_child(state_directory, sizeof(state_directory) / sizeof(state_directory[0]),
                   project, L"\\.obscura64") &&
        make_child(state_path, sizeof(state_path) / sizeof(state_path[0]),
                   state_directory, L"\\current.state")) {
        (void)DeleteFileW(state_path);
        if (make_child(state_path, sizeof(state_path) / sizeof(state_path[0]), state_directory, L"\\operation.lock"))
            (void)DeleteFileW(state_path);
        (void)RemoveDirectoryW(state_directory);
    }
    (void)RemoveDirectoryW(project);
}

static int has_status_text(obscura64_status status)
{
    const char *text = obscura64_status_string(status);
    return text != NULL && text[0] != '\0';
}

static obscura64_status counted_encoded_size(void *user_data, const char profile[64],
                                              size_t input_len, size_t *encoded_len)
{
    int *count = (int *)user_data;
    ++*count;
    return obscura64_builtin_provider()->encoded_size(NULL, profile, input_len, encoded_len);
}

static obscura64_status counted_decoded_size(void *user_data, const char profile[64],
                                              const char *encoded, size_t encoded_len,
                                              size_t *decoded_len)
{
    int *count = (int *)user_data;
    ++*count;
    return obscura64_builtin_provider()->decoded_size(NULL, profile, encoded, encoded_len,
                                                       decoded_len);
}

static obscura64_status counted_encode(void *user_data, const char profile[64],
                                       const void *input, size_t input_len, char *output,
                                       size_t capacity, size_t *output_len)
{
    int *count = (int *)user_data;
    ++*count;
    return obscura64_builtin_provider()->encode(NULL, profile, input, input_len, output,
                                                 capacity, output_len);
}

static obscura64_status counted_decode(void *user_data, const char profile[64],
                                       const char *encoded, size_t encoded_len, void *output,
                                       size_t capacity, size_t *output_len)
{
    int *count = (int *)user_data;
    ++*count;
    return obscura64_builtin_provider()->decode(NULL, profile, encoded, encoded_len, output,
                                                 capacity, output_len);
}

typedef struct race_call {
    HANDLE start_event;
    const char *path;
    obscura64_status status;
    char profile[OBSCURA64_PROFILE_SIZE];
} race_call;

static DWORD WINAPI race_open_thread(LPVOID argument)
{
    race_call *call = (race_call *)argument;
    obscura64_context *context = NULL;
    (void)WaitForSingleObject(call->start_event, INFINITE);
    call->status = obscura64_open(call->path, &context);
    if (call->status == OBSCURA64_OK && context != NULL) {
        memcpy(call->profile, context->profile, sizeof(call->profile));
        obscura64_context_destroy(context);
    }
    return 0;
}

int main(void)
{
    WCHAR temp_path[MAX_PATH];
    WCHAR unique_path[MAX_PATH];
    WCHAR project[MAX_PATH * 2];
    WCHAR empty_project[MAX_PATH * 2];
    WCHAR race_project[MAX_PATH * 2];
    WCHAR nonexisting_project[MAX_PATH * 2];
    WCHAR nondirectory_path[MAX_PATH * 2];
    WCHAR state_directory[MAX_PATH * 2];
    char project_utf8[MAX_PATH * 6];
    char empty_utf8[MAX_PATH * 6];
    char race_utf8[MAX_PATH * 6];
    char missing_utf8[MAX_PATH * 6];
    char nondirectory_utf8[MAX_PATH * 6];
    const unsigned char sample[] = {0, 1, 2, 0xFE, 0xFF, 0x41};
    obscura64_context *context = NULL;
    obscura64_context *provider_context = NULL;
    obscura64_project_state state;
    unsigned char stable[OBSCURA64_STATE_V1_SIZE];
    unsigned char current[OBSCURA64_STATE_V1_SIZE];
    char *encoded = NULL;
    size_t encoded_len = 0;
    void *decoded = NULL;
    size_t decoded_len = 0;
    uint16_t profile_id = 0;
    obscura64_provider provider;
    int callback_count = 0;
    HANDLE start_event;
    HANDLE threads[2];
    race_call calls[2];
    unsigned int i;
    HANDLE file;
    obscura64_context *invalid_context = (obscura64_context *)1;

    if (GetTempPathW(MAX_PATH, temp_path) == 0 ||
        GetTempFileNameW(temp_path, L"o64", 0, unique_path) == 0) {
        fprintf(stderr, "FAIL: cannot create temporary test path\n");
        return 1;
    }
    (void)DeleteFileW(unique_path);
    if (!make_child(project, sizeof(project) / sizeof(project[0]), unique_path,
                    L"-测试-项目") || !CreateDirectoryW(project, NULL) ||
        !make_child(empty_project, sizeof(empty_project) / sizeof(empty_project[0]),
                    project, L"-empty") || !CreateDirectoryW(empty_project, NULL) ||
        !make_child(race_project, sizeof(race_project) / sizeof(race_project[0]),
                    project, L"-race") || !CreateDirectoryW(race_project, NULL) ||
        !make_child(nonexisting_project, sizeof(nonexisting_project) / sizeof(nonexisting_project[0]),
                    project, L"-does-not-exist") ||
        !make_child(nondirectory_path, sizeof(nondirectory_path) / sizeof(nondirectory_path[0]),
                    project, L"-ordinary-file")) {
        fprintf(stderr, "FAIL: cannot prepare temporary directories\n");
        cleanup_project(project);
        (void)RemoveDirectoryW(empty_project);
        (void)RemoveDirectoryW(race_project);
        return 1;
    }
    if (!make_child(state_directory, sizeof(state_directory) / sizeof(state_directory[0]),
                    empty_project, L"\\.obscura64") || !CreateDirectoryW(state_directory, NULL)) {
        fprintf(stderr, "FAIL: cannot prepare empty state container\n");
        cleanup_project(project);
        (void)RemoveDirectoryW(state_directory);
        (void)RemoveDirectoryW(empty_project);
        (void)RemoveDirectoryW(race_project);
        return 1;
    }
    if (!wide_to_utf8(project, project_utf8, sizeof(project_utf8)) ||
        !wide_to_utf8(empty_project, empty_utf8, sizeof(empty_utf8)) ||
        !wide_to_utf8(race_project, race_utf8, sizeof(race_utf8)) ||
        !wide_to_utf8(nonexisting_project, missing_utf8, sizeof(missing_utf8)) ||
        !wide_to_utf8(nondirectory_path, nondirectory_utf8, sizeof(nondirectory_utf8))) {
        fprintf(stderr, "FAIL: cannot convert test paths to UTF-8\n");
        return 1;
    }
    file = CreateFileW(nondirectory_path, GENERIC_WRITE, 0, NULL, CREATE_NEW,
                       FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "FAIL: cannot create regular-file path test\n");
        return 1;
    }
    CloseHandle(file);

    CHECK(obscura64_open(project_utf8, &context) == OBSCURA64_OK && context != NULL,
          "first open initializes Unicode project");
    obscura64_context_destroy(context);
    context = NULL;
    CHECK(read_state_bytes(project, stable), "first state file exists and is 160 bytes");
    CHECK(obscura64_state_deserialize(stable, sizeof(stable), &state) == OBSCURA64_CORE_STATUS_SUCCESS,
          "first state validates");
    CHECK(state.generation == 1, "initial generation equals one");
    for (i = 0; i < OBSCURA64_PROJECT_ID_SIZE; ++i) current[i] = state.project_id[i];
    {
        unsigned char any = 0;
        for (i = 0; i < OBSCURA64_PROJECT_ID_SIZE; ++i) any |= current[i];
        CHECK(any != 0, "project id is nonzero");
    }
    CHECK(obscura64_profile_library_find((const unsigned char *)state.profile, &profile_id),
          "selected profile is in frozen library");

    CHECK(obscura64_open(project_utf8, &context) == OBSCURA64_OK, "open for initial encoding");
    CHECK(obscura64_encode_alloc(context, sample, sizeof(sample), &encoded, &encoded_len) == OBSCURA64_OK,
          "encode sample with initialized context");
    obscura64_context_destroy(context);
    context = NULL;
    for (i = 0; i < 4; ++i) {
        CHECK(obscura64_open(project_utf8, &context) == OBSCURA64_OK,
              "normal reopen succeeds");
        CHECK(obscura64_decode_alloc(context, encoded, encoded_len, &decoded, &decoded_len) == OBSCURA64_OK,
              "reopened context decodes original encoding");
        CHECK(decoded_len == sizeof(sample) && memcmp(decoded, sample, sizeof(sample)) == 0,
              "reopen preserves exact codec Profile");
        obscura64_free(decoded);
        decoded = NULL;
        decoded_len = 0;
        obscura64_context_destroy(context);
        context = NULL;
        CHECK(read_state_bytes(project, current), "state readable after repeated open");
        CHECK(memcmp(stable, current, sizeof(stable)) == 0, "state bytes stable after repeated open");
    }
    obscura64_free(encoded);
    encoded = NULL;

    CHECK(obscura64_open_with_provider(project_utf8, obscura64_builtin_provider(),
                                       &provider_context) == OBSCURA64_OK,
          "custom provider project open succeeds");
    CHECK(read_state_bytes(project, current) && memcmp(stable, current, sizeof(stable)) == 0,
          "provider open leaves project state unchanged");
    obscura64_context_destroy(provider_context);
    provider_context = NULL;

    memset(&provider, 0, sizeof(provider));
    provider.struct_size = sizeof(provider);
    provider.abi_version = OBSCURA64_PROVIDER_ABI_VERSION;
    provider.user_data = &callback_count;
    provider.encoded_size = counted_encoded_size;
    provider.decoded_size = counted_decoded_size;
    provider.encode = counted_encode;
    provider.decode = counted_decode;
    CHECK(obscura64_open_with_provider(project_utf8, &provider, &provider_context) == OBSCURA64_OK,
          "runtime custom provider open succeeds");
    CHECK(obscura64_encode_alloc(provider_context, sample, sizeof(sample), &encoded, &encoded_len) == OBSCURA64_OK,
          "custom provider encoding succeeds");
    CHECK(callback_count > 0, "custom provider callback actually called");
    obscura64_free(encoded);
    encoded = NULL;
    obscura64_context_destroy(provider_context);
    provider_context = NULL;
    CHECK(read_state_bytes(project, current) && memcmp(stable, current, sizeof(stable)) == 0,
          "custom provider does not change state identity");

    CHECK(obscura64_open(NULL, &invalid_context) == OBSCURA64_INVALID_ARGUMENT && invalid_context == NULL,
          "null path rejected");
    invalid_context = (obscura64_context *)1;
    CHECK(obscura64_open("\xC3\x28", &invalid_context) == OBSCURA64_INVALID_ARGUMENT &&
          invalid_context == NULL, "invalid UTF-8 rejected");
    invalid_context = (obscura64_context *)1;
    CHECK(obscura64_open(missing_utf8, &invalid_context) == OBSCURA64_IO_ERROR && invalid_context == NULL,
          "nonexistent project path rejected");
    invalid_context = (obscura64_context *)1;
    CHECK(obscura64_open(nondirectory_utf8, &invalid_context) == OBSCURA64_IO_ERROR &&
          invalid_context == NULL, "regular file project path rejected");
    CHECK(obscura64_open(empty_utf8, &invalid_context) == OBSCURA64_STATE_MISSING &&
          invalid_context == NULL, "empty state directory is missing state");

    memcpy(current, stable, sizeof(current));
    current[70] ^= 1;
    CHECK(overwrite_state_bytes(project, current), "write corrupted test state");
    invalid_context = (obscura64_context *)1;
    CHECK(obscura64_open(project_utf8, &invalid_context) == OBSCURA64_STATE_CORRUPT &&
          invalid_context == NULL, "corrupt state rejected");
    CHECK(read_state_bytes(project, current) && current[70] == (unsigned char)(stable[70] ^ 1),
          "corrupt state remains untouched");
    CHECK(delete_current_state(project), "delete current state for missing test");
    invalid_context = (obscura64_context *)1;
    CHECK(obscura64_open(project_utf8, &invalid_context) == OBSCURA64_STATE_MISSING &&
          invalid_context == NULL, "existing state directory without file is missing");
    {
        char *status_names[] = {"I/O error", "project state missing",
                                 "project state corrupt", "random number generation failure"};
        obscura64_status statuses[] = {OBSCURA64_IO_ERROR, OBSCURA64_STATE_MISSING,
                                       OBSCURA64_STATE_CORRUPT, OBSCURA64_RNG_FAILURE};
        for (i = 0; i < 4; ++i) {
            CHECK(has_status_text(statuses[i]) &&
                  strcmp(obscura64_status_string(statuses[i]), status_names[i]) == 0,
                  "new status has nonempty descriptive text");
        }
    }

    CHECK(CreateDirectoryW(race_project, NULL) == 0 && GetLastError() == ERROR_ALREADY_EXISTS,
          "concurrent test project already exists as root");
    start_event = CreateEventW(NULL, TRUE, FALSE, NULL);
    CHECK(start_event != NULL, "create race start event");
    memset(calls, 0, sizeof(calls));
    for (i = 0; i < 2; ++i) {
        calls[i].start_event = start_event;
        calls[i].path = race_utf8;
        threads[i] = CreateThread(NULL, 0, race_open_thread, &calls[i], 0, NULL);
        CHECK(threads[i] != NULL, "create concurrent open thread");
    }
    SetEvent(start_event);
    CHECK(WaitForMultipleObjects(2, threads, TRUE, 10000) == WAIT_OBJECT_0,
          "concurrent opens finish");
    for (i = 0; i < 2; ++i) {
        if (calls[i].status != OBSCURA64_OK)
            fprintf(stderr, "concurrent first-open status: %s\n", obscura64_status_string(calls[i].status));
        CHECK(calls[i].status == OBSCURA64_OK, "both concurrent first opens succeed");
        CloseHandle(threads[i]);
    }
    CloseHandle(start_event);
    CHECK(memcmp(calls[0].profile, calls[1].profile, OBSCURA64_PROFILE_SIZE) == 0,
          "concurrent first opens use one winning Profile");
    CHECK(read_state_bytes(race_project, current) &&
          obscura64_state_deserialize(current, sizeof(current), &state) == OBSCURA64_CORE_STATUS_SUCCESS,
          "concurrent state is one valid authoritative file");

    CHECK(has_status_text(OBSCURA64_IO_ERROR) && has_status_text(OBSCURA64_STATE_MISSING) &&
          has_status_text(OBSCURA64_STATE_CORRUPT) && has_status_text(OBSCURA64_RNG_FAILURE),
          "all new public statuses have text");
    printf("PASS: %u project checks\n", checks);

    cleanup_project(project);
    cleanup_project(empty_project);
    cleanup_project(race_project);
    (void)DeleteFileW(nondirectory_path);
    return 0;
}
