#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "obscura64_internal.h"
#include "obscura64_profiles_v1.h"
#include <stdio.h>
#include <string.h>
#include <wchar.h>

static unsigned int checks;
#define CHECK(c, label) do { ++checks; if (!(c)) { \
    fprintf(stderr, "FAIL: %s (line %d)\n", label, __LINE__); return 1; } } while (0)

static void make_current(obscura64_project_state *current, uint64_t generation)
{
    memset(current, 0, sizeof(*current));
    current->project_id[0] = 0x53;
    current->generation = generation;
    memcpy(current->profile, obscura64_profiles_v1[0], OBSCURA64_PROFILE_SIZE);
}

static void make_history(obscura64_project_history *history,
    const obscura64_project_state *current, int overlap)
{
    uint64_t newest = current->generation - (overlap ? 0U : 1U);
    size_t i;
    memset(history, 0, sizeof(*history));
    memcpy(history->project_id, current->project_id, OBSCURA64_PROJECT_ID_SIZE);
    history->count = newest < 3U ? (uint16_t)newest : 3U;
    for (i = 0; i < history->count; ++i) {
        history->entries[i].generation = newest - i;
        memcpy(history->entries[i].profile, obscura64_profiles_v1[i + 1U], OBSCURA64_PROFILE_SIZE);
    }
    if (overlap) memcpy(history->entries[0].profile, current->profile, OBSCURA64_PROFILE_SIZE);
}

static int direct_checks(void)
{
    const uint64_t generations[] = {1, 2, 3, 4, 5, 8, 100, UINT64_MAX};
    obscura64_project_state current, current_before;
    obscura64_project_history history, before, parsed;
    unsigned char bytes[OBSCURA64_HISTORY_V1_SIZE];
    size_t i;
    unsigned int overlap;
    obscura64_managed_state_kind kind;
    CHECK(obscura64_managed_state_validate(NULL, NULL, NULL) == OBSCURA64_INVALID_ARGUMENT, "null current");
    for (i = 0; i < sizeof(generations) / sizeof(generations[0]); ++i) {
        make_current(&current, generations[i]);
        CHECK(obscura64_managed_state_validate(&current, NULL, NULL) ==
            (generations[i] == 1 ? OBSCURA64_OK : OBSCURA64_STATE_MISSING), "missing history by generation");
        for (overlap = 0; overlap < 2; ++overlap) {
            make_history(&history, &current, (int)overlap);
            current_before = current;
            before = history;
            CHECK(obscura64_managed_state_validate(&current, &history, &kind) == OBSCURA64_OK &&
                kind == (overlap ? OBSCURA64_MANAGED_OVERLAP : OBSCURA64_MANAGED_STEADY),
                  "canonical steady or complete overlap window");
            CHECK(memcmp(&current, &current_before, sizeof(current)) == 0 &&
                memcmp(&history, &before, sizeof(history)) == 0, "validator leaves inputs untouched");
        }
    }
    make_current(&current, 5);
    make_history(&history, &current, 0);
    history.entries[1].generation = 2;
    history.entries[2].generation = 1;
    before = history;
    CHECK(obscura64_history_validate(&history), "generic accepts non-contiguous descending generations");
    CHECK(obscura64_history_serialize(&history, bytes) == OBSCURA64_CORE_STATUS_SUCCESS &&
        obscura64_history_deserialize(bytes, sizeof(bytes), &parsed) == OBSCURA64_CORE_STATUS_SUCCESS,
        "generic serializer and parser accept structurally valid gap");
    kind = OBSCURA64_MANAGED_OVERLAP;
    CHECK(obscura64_managed_state_validate(&current, &parsed, &kind) == OBSCURA64_STATE_CORRUPT &&
        kind == OBSCURA64_MANAGED_OVERLAP,
          "managed validator rejects generic-valid gap");
    CHECK(memcmp(&history, &before, sizeof(history)) == 0, "gap validation does not repair");
    make_history(&history, &current, 0);
    memcpy(history.entries[1].profile, current.profile, OBSCURA64_PROFILE_SIZE);
    CHECK(obscura64_managed_state_validate(&current, &history, NULL) == OBSCURA64_OK,
          "repeated Profile at distinct nonadjacent generations allowed");
    current.generation = 0;
    CHECK(obscura64_managed_state_validate(&current, &history, NULL) == OBSCURA64_STATE_CORRUPT, "invalid current generation");
    make_current(&current, 5);
    memset(current.project_id, 0, sizeof(current.project_id));
    CHECK(obscura64_managed_state_validate(&current, &history, NULL) == OBSCURA64_STATE_CORRUPT, "invalid current identity");
    make_current(&current, 5);
    current.profile[0] = '=';
    CHECK(obscura64_managed_state_validate(&current, &history, NULL) == OBSCURA64_STATE_CORRUPT, "invalid current Profile");
    return 0;
}

static int file_io(const WCHAR *path, unsigned char *data, DWORD size, int write)
{
    HANDLE file = CreateFileW(path, write ? GENERIC_WRITE : GENERIC_READ,
        FILE_SHARE_READ, NULL, write ? CREATE_ALWAYS : OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    LARGE_INTEGER length;
    DWORD count = 0;
    int ok;
    if (file == INVALID_HANDLE_VALUE) return 0;
    ok = write ? WriteFile(file, data, size, &count, NULL) :
        (GetFileSizeEx(file, &length) && length.QuadPart == size && ReadFile(file, data, size, &count, NULL));
    if (write && ok) ok = FlushFileBuffers(file);
    if (!CloseHandle(file)) ok = 0;
    return ok && count == size;
}

static int directory_clean_once(const WCHAR *directory, int has_history)
{
    WCHAR pattern[MAX_PATH * 2];
    WIN32_FIND_DATAW entry;
    HANDLE search;
    unsigned int count = 0;
    int ok = 1;
    if (wcslen(directory) + 3U >= MAX_PATH * 2) return 0;
    wcscpy(pattern, directory); wcscat(pattern, L"\\*");
    search = FindFirstFileW(pattern, &entry);
    if (search == INVALID_HANDLE_VALUE) return 0;
    do {
        WCHAR entry_path[MAX_PATH * 2];
        if (wcscmp(entry.cFileName, L".") == 0 || wcscmp(entry.cFileName, L"..") == 0) continue;
        /* Enumeration may include an already deleted rename source. Check existence. */
        if (wcslen(directory) + wcslen(entry.cFileName) + 2U >= MAX_PATH * 2) { ok = 0; break; }
        wcscpy(entry_path, directory); wcscat(entry_path, L"\\"); wcscat(entry_path, entry.cFileName);
        if (GetFileAttributesW(entry_path) == INVALID_FILE_ATTRIBUTES &&
            GetLastError() == ERROR_FILE_NOT_FOUND) continue;
        ++count;
        if (wcscmp(entry.cFileName, L"current.state") != 0 &&
            wcscmp(entry.cFileName, L"history.state") != 0 &&
            wcscmp(entry.cFileName, L"operation.lock") != 0) {
            ok = 0;
        }
    } while (FindNextFileW(search, &entry));
    FindClose(search);
    return ok && count == (has_history ? 3U : 2U);
}

static int directory_clean(const WCHAR *directory, int has_history)
{
    unsigned int attempt;
    /* Deleted rename sources can remain briefly enumerable on this Windows volume.
       A persistent extra file still fails; this does not remove or accept it. */
    for (attempt = 0; attempt < 25U; ++attempt) {
        if (directory_clean_once(directory, has_history)) return 1;
        Sleep(10);
    }
    fwprintf(stderr, L"Directory contents did not settle: %ls\n", directory);
    return 0;
}

static int remove_owned_directory(const WCHAR *path)
{
    unsigned int attempt;
    for (attempt = 0; attempt < 25U; ++attempt) {
        if (RemoveDirectoryW(path)) return 1;
        if (GetLastError() != ERROR_DIR_NOT_EMPTY) return 0;
        Sleep(10);
    }
    return 0;
}

static int project_checks(void)
{
    WCHAR temp[MAX_PATH], seed[MAX_PATH], project[MAX_PATH * 2], directory[MAX_PATH * 2];
    WCHAR current_path[MAX_PATH * 2], history_path[MAX_PATH * 2], lock_path[MAX_PATH * 2];
    char utf8[MAX_PATH * 6];
    obscura64_project_state current, after_current;
    obscura64_project_history history, after_history;
    obscura64_context *context = NULL;
    unsigned char current_bytes[OBSCURA64_STATE_V1_SIZE], current_after[OBSCURA64_STATE_V1_SIZE];
    unsigned char history_bytes[OBSCURA64_HISTORY_V1_SIZE], history_after[OBSCURA64_HISTORY_V1_SIZE];
    unsigned int variant;
    CHECK(GetTempPathW(MAX_PATH, temp) && GetTempFileNameW(temp, L"oms", 0, seed) && DeleteFileW(seed), "fixture seed");
    wcscpy(project, seed); wcscat(project, L"-\x9879\x76EE");
    wcscpy(directory, project); wcscat(directory, L"\\.obscura64");
    wcscpy(current_path, directory); wcscat(current_path, L"\\current.state");
    wcscpy(history_path, directory); wcscat(history_path, L"\\history.state");
    wcscpy(lock_path, directory); wcscat(lock_path, L"\\operation.lock");
    CHECK(CreateDirectoryW(project, NULL) && CreateDirectoryW(directory, NULL), "Unicode managed project fixture");
    CHECK(WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, project, -1, utf8, sizeof(utf8), NULL, NULL), "UTF-8 project path");

    /* Every fixture keeps valid current authoritative for normal open. */
    for (variant = 0; variant < 25; ++variant) {
        int missing_history = variant == 0 || variant == 1;
        obscura64_status expected = OBSCURA64_STATE_CORRUPT;
        make_current(&current, 5);
        if (variant == 0 || variant == 2 || variant == 16) current.generation = 1;
        if (variant == 3 || variant == 17) current.generation = 2;
        if (variant == 4 || variant == 18) current.generation = 3;
        if (variant == 5 || variant == 19) current.generation = 4;
        if (variant == 21 || variant == 22 || variant == 24) current.generation = 8;
        make_history(&history, &current, variant >= 16 && variant <= 20);
        if (variant <= 6 || (variant >= 16 && variant <= 20)) expected = OBSCURA64_OK;
        if (variant == 1) expected = OBSCURA64_STATE_MISSING;
        if (variant == 8) history.project_id[0] ^= 1;
        if (variant == 9) history.entries[0].generation = 6; /* Future. */
        if (variant == 10) { /* Same generation, different Profile. */
            make_history(&history, &current, 1);
            memcpy(history.entries[0].profile, obscura64_profiles_v1[1], OBSCURA64_PROFILE_SIZE);
        }
        if (variant == 11) { history.entries[1].generation = 2; history.entries[2].generation = 1; }
        if (variant == 12) { history.count = 2; memset(&history.entries[2], 0, sizeof(history.entries[2])); }
        if (variant == 13) { memset(history.entries, 0, sizeof(history.entries)); history.count = 0; }
        if (variant == 14) { for (size_t i = 0; i < history.count; ++i) --history.entries[i].generation; }
        if (variant == 15) { make_history(&history, &current, 1); history.entries[1].generation = 3; history.entries[2].generation = 2; }
        if (variant == 21) history.entries[2].generation = 4; /* 7,6,4 */
        if (variant == 22) { history.entries[1].generation = 5; history.entries[2].generation = 4; } /* 7,5,4 */
        if (variant == 23) { history.count = 1; memset(history.entries + 1, 0, 2U * sizeof(history.entries[0])); }
        if (variant == 24) { make_history(&history, &current, 1); history.entries[1].generation = 6; history.entries[2].generation = 5; }
        CHECK(obscura64_state_serialize(&current, current_bytes) == OBSCURA64_CORE_STATUS_SUCCESS &&
            file_io(current_path, current_bytes, sizeof(current_bytes), 1), "write valid authoritative current fixture");
        if (missing_history) {
            if (GetFileAttributesW(history_path) != INVALID_FILE_ATTRIBUTES)
                CHECK(DeleteFileW(history_path), "remove only test history");
        } else {
            CHECK(obscura64_history_serialize(&history, history_bytes) == OBSCURA64_CORE_STATUS_SUCCESS,
                  "cross-file fixture remains generically valid");
            CHECK(obscura64_history_deserialize(history_bytes, sizeof(history_bytes), &after_history) == OBSCURA64_CORE_STATUS_SUCCESS,
                  "generic parser accepts cross-file fixture before corruption");
            if (variant == 7) history_bytes[80] ^= 1; /* Bad SHA. */
            CHECK(file_io(history_path, history_bytes, sizeof(history_bytes), 1), "write test history");
        }
        CHECK(obscura64_open(utf8, &context) == OBSCURA64_OK && context != NULL &&
            memcmp(context->profile, current.profile, OBSCURA64_PROFILE_SIZE) == 0,
            "normal open uses valid current despite missing/corrupt/noncanonical history");
        obscura64_context_destroy(context); context = NULL;
        CHECK(file_io(current_path, current_after, sizeof(current_after), 0) &&
            memcmp(current_bytes, current_after, sizeof(current_bytes)) == 0, "normal open never changes current");
        CHECK(directory_clean(directory, !missing_history), "normal open writes no history or temps");
        if (!missing_history)
            CHECK(file_io(history_path, history_after, sizeof(history_after), 0) &&
                memcmp(history_bytes, history_after, sizeof(history_bytes)) == 0, "normal open leaves even corrupt history untouched");
        CHECK(obscura64_force_reinitialize(utf8, &context) == expected,
              "Force applies managed-set rules before state/history writes");
        if (expected != OBSCURA64_OK) {
            CHECK(context == NULL, "failed Force publishes no Context");
            CHECK(file_io(current_path, current_after, sizeof(current_after), 0) &&
                memcmp(current_bytes, current_after, sizeof(current_bytes)) == 0, "rejected Force current bytes unchanged");
            if (!missing_history)
                CHECK(file_io(history_path, history_after, sizeof(history_after), 0) &&
                    memcmp(history_bytes, history_after, sizeof(history_bytes)) == 0, "rejected Force history bytes unchanged");
            CHECK(directory_clean(directory, !missing_history), "rejected Force no repair or orphan lock/temp");
        } else {
            CHECK(context != NULL && memcmp(context->profile, current.profile, OBSCURA64_PROFILE_SIZE) != 0,
                  "successful Force new Profile differs");
            obscura64_context_destroy(context); context = NULL;
            CHECK(file_io(current_path, current_after, sizeof(current_after), 0) &&
                obscura64_state_deserialize(current_after, sizeof(current_after), &after_current) == OBSCURA64_CORE_STATUS_SUCCESS &&
                after_current.generation == current.generation + 1U &&
                memcmp(after_current.project_id, current.project_id, OBSCURA64_PROJECT_ID_SIZE) == 0,
                "successful Force generation and identity");
            CHECK(file_io(history_path, history_after, sizeof(history_after), 0) &&
                obscura64_history_deserialize(history_after, sizeof(history_after), &after_history) == OBSCURA64_CORE_STATUS_SUCCESS &&
                obscura64_managed_state_validate(&after_current, &after_history, NULL) == OBSCURA64_OK,
                "successful Force results in complete canonical steady window");
            CHECK(after_history.entries[0].generation == current.generation &&
                memcmp(after_history.entries[0].profile, current.profile, OBSCURA64_PROFILE_SIZE) == 0,
                "old current retained exactly once");
            CHECK(directory_clean(directory, 1), "successful Force only expected managed files");
        }
    }
    CHECK(DeleteFileW(current_path) && DeleteFileW(history_path) && DeleteFileW(lock_path) && remove_owned_directory(directory),
          "reset owned fixture for real initialization");
    CHECK(obscura64_open(utf8, &context) == OBSCURA64_OK, "initialize real managed project");
    obscura64_context_destroy(context); context = NULL;
    CHECK(obscura64_force_reinitialize(utf8, &context) == OBSCURA64_OK,
          "real managed project reaches generation two");
    {
        const unsigned char sample[] = {0, 1, 0x80, 0xFF};
        char *encoded = NULL;
        void *decoded = NULL;
        size_t encoded_size = 0, decoded_size = 0;
        CHECK(obscura64_encode_alloc(context, sample, sizeof(sample), &encoded, &encoded_size) == OBSCURA64_OK,
              "encode current project data before history corruption");
        obscura64_context_destroy(context); context = NULL;
        CHECK(file_io(current_path, current_bytes, sizeof(current_bytes), 0) &&
            obscura64_state_deserialize(current_bytes, sizeof(current_bytes), &current) == OBSCURA64_CORE_STATUS_SUCCESS &&
            current.generation == 2, "real generation two current snapshot");
        CHECK(file_io(history_path, history_bytes, sizeof(history_bytes), 0), "real history snapshot");
        history_bytes[80] ^= 1;
        CHECK(file_io(history_path, history_bytes, sizeof(history_bytes), 1), "corrupt only owned real history");
        CHECK(obscura64_open_with_provider(utf8, obscura64_builtin_provider(), &context) == OBSCURA64_OK,
              "provider normal open with corrupt real history");
        CHECK(obscura64_decode_alloc(context, encoded, encoded_size, &decoded, &decoded_size) == OBSCURA64_OK &&
            decoded_size == sizeof(sample) && memcmp(decoded, sample, sizeof(sample)) == 0,
            "corrupt history cannot block current data decoding");
        obscura64_free(decoded); decoded = NULL;
        obscura64_context_destroy(context); context = NULL;
        CHECK(obscura64_force_reinitialize(utf8, &context) == OBSCURA64_STATE_CORRUPT && context == NULL,
              "Force rejects corrupt real history");
        CHECK(file_io(current_path, current_after, sizeof(current_after), 0) &&
            memcmp(current_after, current_bytes, sizeof(current_bytes)) == 0 &&
            file_io(history_path, history_after, sizeof(history_after), 0) &&
            memcmp(history_after, history_bytes, sizeof(history_bytes)) == 0 && directory_clean(directory, 1),
            "corrupt real history left untouched without repair or temp artifacts");
        CHECK(DeleteFileW(history_path), "delete only owned real history for missing test");
        CHECK(obscura64_open_with_provider(utf8, obscura64_builtin_provider(), &context) == OBSCURA64_OK,
              "provider normal open with missing history at generation two");
        CHECK(obscura64_decode_alloc(context, encoded, encoded_size, &decoded, &decoded_size) == OBSCURA64_OK &&
            decoded_size == sizeof(sample) && memcmp(decoded, sample, sizeof(sample)) == 0,
            "missing history cannot block current data decoding");
        obscura64_free(decoded);
        obscura64_free(encoded);
        obscura64_context_destroy(context); context = NULL;
        CHECK(obscura64_force_reinitialize(utf8, &context) == OBSCURA64_STATE_MISSING && context == NULL,
              "Force rejects missing real history at generation two");
        CHECK(file_io(current_path, current_after, sizeof(current_after), 0) &&
            memcmp(current_after, current_bytes, sizeof(current_bytes)) == 0 && directory_clean(directory, 0),
            "missing real history not synthesized and current unchanged");
    }
    CHECK(DeleteFileW(current_path) && DeleteFileW(lock_path) && remove_owned_directory(directory) && remove_owned_directory(project),
          "cleanup only owned real project");
    return 0;
}

int main(void)
{
    if (direct_checks() || project_checks()) return 1;
    printf("PASS: %u managed-state checks\n", checks);
    return 0;
}
