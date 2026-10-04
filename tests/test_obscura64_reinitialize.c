#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include "obscura64_internal.h"
#include "obscura64_profiles_v1.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

static unsigned int history_checks, force_checks;
#define HCHECK(c, name) do { ++history_checks; if (!(c)) { fprintf(stderr, "FAIL history: %s\n", name); return 1; } } while (0)
#define RCHECK(c, name) do { ++force_checks; if (!(c)) { fprintf(stderr, "FAIL force: %s\n", name); return 1; } } while (0)

static int rehash(unsigned char *bytes, ULONG length)
{
    BCRYPT_ALG_HANDLE algorithm = NULL;
    BCRYPT_HASH_HANDLE hash = NULL;
    NTSTATUS status = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, NULL, 0);
    if (!BCRYPT_SUCCESS(status)) return 0;
    status = BCryptCreateHash(algorithm, &hash, NULL, 0, NULL, 0, 0);
    if (BCRYPT_SUCCESS(status)) status = BCryptHashData(hash, bytes, length, 0);
    if (BCRYPT_SUCCESS(status)) status = BCryptFinishHash(hash, bytes + length, 32, 0);
    if (hash != NULL) (void)BCryptDestroyHash(hash);
    (void)BCryptCloseAlgorithmProvider(algorithm, 0);
    return BCRYPT_SUCCESS(status);
}

static int rejected_history_preserves(const unsigned char *bytes, size_t length)
{
    obscura64_project_history out;
    unsigned char before[sizeof(out)];
    memset(&out, 0xA5, sizeof(out));
    memcpy(before, &out, sizeof(out));
    return obscura64_history_deserialize(bytes, length, &out) != OBSCURA64_CORE_STATUS_SUCCESS &&
        memcmp(before, &out, sizeof(out)) == 0;
}

static void test_put_generation(unsigned char *bytes, uint64_t value)
{
    size_t i;
    for (i = 0; i < 8; ++i) bytes[i] = (unsigned char)(value >> (8U * i));
}

static int test_history_format(void)
{
    obscura64_project_history input, output;
    unsigned char blob[OBSCURA64_HISTORY_V1_SIZE], altered[OBSCURA64_HISTORY_V1_SIZE];
    size_t i, j;
    uint16_t count, id;
    char outside[64];
    int outside_found = 0;
    static const unsigned char expected_header[32] = {
        'O','B','6','4','H','I','0','1', 1,0,64,0,80,1,0,0,
        1,0,0,0,80,0,3,0,1,0,0,0,0,0,0,0
    };
    for (count = 0; count <= 3; ++count) {
        memset(&input, 0, sizeof(input));
        input.project_id[0] = 7;
        input.count = count;
        for (i = 0; i < count; ++i) {
            input.entries[i].generation = 3 - i;
            memcpy(input.entries[i].profile, obscura64_profiles_v1[i], 64);
        }
        HCHECK(obscura64_history_validate(&input), "logical count 0 through 3 valid");
        HCHECK(obscura64_history_serialize(&input, blob) == OBSCURA64_CORE_STATUS_SUCCESS, "serialize count 0 through 3");
        HCHECK(obscura64_history_deserialize(blob, sizeof(blob), &output) == OBSCURA64_CORE_STATUS_SUCCESS, "deserialize count 0 through 3");
        HCHECK(memcmp(input.project_id, output.project_id, 16) == 0 && output.count == input.count, "roundtrip identity/count");
        for (i = 0; i < count; ++i)
            HCHECK(input.entries[i].generation == output.entries[i].generation &&
                memcmp(input.entries[i].profile, output.entries[i].profile, 64) == 0, "roundtrip entries");
    }
    memset(&input, 0, sizeof(input));
    input.project_id[0] = 7; input.count = 1; input.entries[0].generation = UINT64_C(0x0102030405060708);
    memcpy(input.entries[0].profile, obscura64_profiles_v1[0], 64);
    HCHECK(obscura64_history_serialize(&input, blob) == OBSCURA64_CORE_STATUS_SUCCESS, "layout source");
    HCHECK(sizeof(blob) == 336 && memcmp(blob, expected_header, sizeof(expected_header)) == 0, "all fixed header fields exact little endian");
    HCHECK(blob[32] == 7, "project ID offset");
    for (i = 33; i < 64; ++i) HCHECK(blob[i] == 0, "ID remainder and reserved zero");
    for (i = 0; i < 8; ++i) HCHECK(blob[64 + i] == (unsigned char)(8 - i), "generation little endian");
    HCHECK(memcmp(blob + 72, input.entries[0].profile, 64) == 0, "profile offset");
    for (i = 136; i < 304; ++i) HCHECK(blob[i] == 0, "entry reserved and unused entries zero");
    memcpy(altered, blob, sizeof(blob));
    HCHECK(rehash(altered, 304) && memcmp(altered + 304, blob + 304, 32) == 0, "SHA covers exactly 304 bytes");
    HCHECK(rejected_history_preserves(blob, 335), "short input atomic");
    HCHECK(rejected_history_preserves(blob, 337), "long input atomic");
    HCHECK(rejected_history_preserves(NULL, 336), "null input atomic");
    HCHECK(obscura64_history_deserialize(blob, 336, NULL) == OBSCURA64_CORE_STATUS_INVALID_ARGUMENT, "null output");
    {
        const size_t offsets[] = {0,8,10,12,16,20,22,24,26,28,48,136,144};
        for (i = 0; i < sizeof(offsets) / sizeof(offsets[0]); ++i) {
            memcpy(altered, blob, sizeof(blob));
            altered[offsets[i]] ^= offsets[i] == 24 ? 5 : 1;
            HCHECK(rehash(altered, 304), "rehash malformed structure");
            HCHECK(rejected_history_preserves(altered, 336), "malformed field/reserved/unused entry rejected atomically");
        }
    }
    memcpy(altered, blob, sizeof(blob)); altered[304] ^= 1;
    HCHECK(rejected_history_preserves(altered, 336), "bad digest");
    memcpy(altered, blob, sizeof(blob)); altered[32] ^= 1;
    HCHECK(rejected_history_preserves(altered, 336), "identity corruption without rehash");
    memcpy(altered, blob, sizeof(blob)); memset(altered + 32, 0, 16);
    HCHECK(rehash(altered, 304) && rejected_history_preserves(altered, 336), "zero identity with correct hash");
    memcpy(altered, blob, sizeof(blob)); memset(altered + 64, 0, 8);
    HCHECK(rehash(altered, 304) && rejected_history_preserves(altered, 336), "zero entry generation with correct hash");
    memcpy(altered, blob, sizeof(blob)); altered[72] = '=';
    HCHECK(rehash(altered, 304) && rejected_history_preserves(altered, 336), "invalid profile character");
    memcpy(altered, blob, sizeof(blob)); altered[73] = altered[72];
    HCHECK(rehash(altered, 304) && rejected_history_preserves(altered, 336), "duplicate profile character");
    for (i = 0; i < 64 && !outside_found; ++i) {
        for (j = 0; j < 64; ++j) outside[j] = obscura64_v1_character_pool[(j + i) % 64];
        if (obscura64_profile_is_valid(outside, 64) &&
            !obscura64_profile_library_find((const unsigned char *)outside, &id)) outside_found = 1;
    }
    HCHECK(outside_found, "deterministic legal profile outside frozen library");
    memcpy(altered, blob, sizeof(blob)); memcpy(altered + 72, outside, 64);
    HCHECK(rehash(altered, 304) && rejected_history_preserves(altered, 336), "legal non-library profile rejected");
    input.count = 4;
    memset(blob, 0x5A, sizeof(blob)); memcpy(altered, blob, sizeof(blob));
    HCHECK(obscura64_history_serialize(&input, blob) != OBSCURA64_CORE_STATUS_SUCCESS &&
        memcmp(blob, altered, sizeof(blob)) == 0, "serialize invalid count atomic");
    HCHECK(obscura64_history_serialize(NULL, blob) == OBSCURA64_CORE_STATUS_INVALID_ARGUMENT &&
        memcmp(blob, altered, sizeof(blob)) == 0, "serialize null state atomic");
    input.count = 1; input.entries[1].generation = 1;
    HCHECK(!obscura64_history_validate(&input), "unused logical entry rejected");
    input.entries[1].generation = 0; input.entries[0].generation = 0;
    HCHECK(obscura64_history_serialize(&input, blob) != OBSCURA64_CORE_STATUS_SUCCESS &&
        memcmp(blob, altered, sizeof(blob)) == 0, "serialize invalid generation atomic");
    memset(&input, 0, sizeof(input));
    input.project_id[0] = 7; input.count = 2;
    input.entries[0].generation = 10; input.entries[1].generation = 7;
    memcpy(input.entries[0].profile, obscura64_profiles_v1[0], 64);
    memcpy(input.entries[1].profile, obscura64_profiles_v1[0], 64);
    HCHECK(obscura64_history_serialize(&input, blob) == OBSCURA64_CORE_STATUS_SUCCESS &&
        obscura64_history_deserialize(blob, 336, &output) == OBSCURA64_CORE_STATUS_SUCCESS,
        "nonconsecutive descending generations and repeated Profile accepted");
    {
        const uint64_t invalid_generations[] = {10, 11};
        for (i = 0; i < 2; ++i) {
            memcpy(altered, blob, sizeof(blob));
            test_put_generation(altered + 144, invalid_generations[i]);
            HCHECK(rehash(altered, 304) && rejected_history_preserves(altered, 336),
                "duplicate/ascending generations with valid SHA rejected atomically");
            input.entries[1].generation = invalid_generations[i];
            memset(altered, 0x5A, sizeof(altered));
            HCHECK(!obscura64_history_validate(&input) &&
                obscura64_history_serialize(&input, altered) == OBSCURA64_CORE_STATUS_INVALID_DATA,
                "logical duplicate/ascending generations rejected");
            for (j = 0; j < sizeof(altered); ++j)
                if (altered[j] != 0x5A) { fprintf(stderr, "FAIL history: ordering serialize atomicity\n"); return 1; }
            HCHECK(1, "ordering serialize preserves entire output");
        }
    }
    printf("PASS: %u history format checks\n", history_checks);
    return 0;
}

static WCHAR projects[3][MAX_PATH * 2];

static int filename(WCHAR *out, const WCHAR *project, const WCHAR *suffix)
{
    size_t a = wcslen(project), b = wcslen(suffix);
    if (a + b >= MAX_PATH * 2) return 0;
    memcpy(out, project, a * sizeof(*out));
    memcpy(out + a, suffix, (b + 1) * sizeof(*out));
    return 1;
}

static void cleanup_tests(void)
{
    size_t i, j;
    const WCHAR *files[] = {L"\\.obscura64\\current.state", L"\\.obscura64\\history.state",
        L"\\.obscura64\\operation.lock", L"\\.obscura64\\current.state.tmp", L"\\.obscura64\\history.state.tmp"};
    WCHAR path[MAX_PATH * 2];
    for (i = 0; i < 3; ++i) {
        if (projects[i][0] == 0) continue;
        for (j = 0; j < sizeof(files) / sizeof(files[0]); ++j)
            if (filename(path, projects[i], files[j])) (void)DeleteFileW(path);
        if (filename(path, projects[i], L"\\.obscura64")) (void)RemoveDirectoryW(path);
        (void)RemoveDirectoryW(projects[i]);
    }
}

static int file_bytes(const WCHAR *path, unsigned char *bytes, DWORD size, int write)
{
    HANDLE file = CreateFileW(path, write ? GENERIC_WRITE : GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
        write ? CREATE_ALWAYS : OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD count = 0;
    LARGE_INTEGER actual;
    int result;
    if (file == INVALID_HANDLE_VALUE) return 0;
    if (!write && (!GetFileSizeEx(file, &actual) || actual.QuadPart != size)) { CloseHandle(file); return 0; }
    result = write ? WriteFile(file, bytes, size, &count, NULL) : ReadFile(file, bytes, size, &count, NULL);
    if (write && result) result = FlushFileBuffers(file);
    if (!CloseHandle(file)) result = 0;
    return result && count == size;
}

static obscura64_status provider_encode(void *data, const char profile[64], const void *input,
    size_t input_size, char *output, size_t capacity, size_t *length)
{
    ++*(unsigned int *)data;
    return obscura64_builtin_provider()->encode(NULL, profile, input, input_size, output, capacity, length);
}

static int test_history_relationships(const char *path_utf8)
{
    WCHAR current_path[MAX_PATH * 2], history_path[MAX_PATH * 2];
    obscura64_project_state state, current;
    obscura64_project_history history, parsed;
    obscura64_context *context = NULL;
    unsigned char current_blob[160], current_after[160], blob[336], after[336];
    unsigned int variant;
    RCHECK(filename(current_path, projects[2], L"\\.obscura64\\current.state") &&
        filename(history_path, projects[2], L"\\.obscura64\\history.state"), "relationship fixture paths");
    memset(&state, 0, sizeof(state));
    state.project_id[0] = 0x21; state.generation = 5;
    memcpy(state.profile, obscura64_profiles_v1[0], 64);
    RCHECK(obscura64_state_serialize(&state, current_blob) == OBSCURA64_CORE_STATUS_SUCCESS,
        "canonical generation five fixture");
    memset(&history, 0, sizeof(history));
    memcpy(history.project_id, state.project_id, 16);
    history.count = 2;
    history.entries[0].generation = 5;
    memcpy(history.entries[0].profile, state.profile, 64);
    history.entries[1].generation = 4;
    memcpy(history.entries[1].profile, obscura64_profiles_v1[1], 64);
    RCHECK(obscura64_history_serialize(&history, blob) == OBSCURA64_CORE_STATUS_SUCCESS &&
        file_bytes(current_path, current_blob, 160, 1) && file_bytes(history_path, blob, 336, 1),
        "synthetic Stage 4.3 current/history overlap fixture");
    RCHECK(obscura64_force_reinitialize(path_utf8, &context) == OBSCURA64_OK, "overlap Force succeeds");
    obscura64_context_destroy(context); context = NULL;
    RCHECK(file_bytes(current_path, current_after, 160, 0) &&
        obscura64_state_deserialize(current_after, 160, &current) == OBSCURA64_CORE_STATUS_SUCCESS &&
        current.generation == 6 && memcmp(current.project_id, state.project_id, 16) == 0,
        "overlap normalization advances generation five to six");
    RCHECK(file_bytes(history_path, after, 336, 0) &&
        obscura64_history_deserialize(after, 336, &parsed) == OBSCURA64_CORE_STATUS_SUCCESS &&
        parsed.count == 2 && parsed.entries[0].generation == 5 && parsed.entries[1].generation == 4 &&
        memcmp(parsed.entries[0].profile, history.entries[0].profile, 64) == 0 &&
        memcmp(parsed.entries[1].profile, history.entries[1].profile, 64) == 0,
        "synthetic overlap retained once with complete correct Profiles");

    for (variant = 0; variant < 4; ++variant) {
        history.entries[0].generation = 5;
        history.entries[1].generation = 4;
        memcpy(history.entries[0].profile, state.profile, 64);
        memcpy(history.entries[1].profile, obscura64_profiles_v1[1], 64);
        if (variant == 0) memcpy(history.entries[0].profile, obscura64_profiles_v1[1], 64);
        if (variant == 1) history.entries[0].generation = 6;
        RCHECK(obscura64_history_serialize(&history, blob) == OBSCURA64_CORE_STATUS_SUCCESS,
            "valid history structure for current-relation rejection");
        if (variant == 2) test_put_generation(blob + 64, 3); /* 3,4 */
        if (variant == 3) {
            test_put_generation(blob + 144, 5); /* 5,5 with identical Profiles */
            memcpy(blob + 152, state.profile, 64);
        }
        RCHECK(rehash(blob, 304) && file_bytes(current_path, current_blob, 160, 1) &&
            file_bytes(history_path, blob, 336, 1), "correct SHA on relation/order fixture");
        RCHECK(obscura64_force_reinitialize(path_utf8, &context) == OBSCURA64_STATE_CORRUPT && context == NULL,
            "same-generation conflict/future/bad order/generic duplicate rejected");
        RCHECK(file_bytes(current_path, current_after, 160, 0) && memcmp(current_blob, current_after, 160) == 0 &&
            file_bytes(history_path, after, 336, 0) && memcmp(blob, after, 336) == 0,
            "relationship rejection changes neither authoritative file");
    }

    memset(&history, 0, sizeof(history)); memcpy(history.project_id, state.project_id, 16);
    RCHECK(obscura64_history_serialize(&history, blob) == OBSCURA64_CORE_STATUS_SUCCESS &&
        file_bytes(current_path, current_blob, 160, 1) && file_bytes(history_path, blob, 336, 1), "existing count zero fixture");
    RCHECK(obscura64_force_reinitialize(path_utf8, &context) == OBSCURA64_OK, "count zero rolls to one");
    obscura64_context_destroy(context); context = NULL;
    RCHECK(file_bytes(history_path, after, 336, 0) && obscura64_history_deserialize(after, 336, &parsed) == OBSCURA64_CORE_STATUS_SUCCESS &&
        parsed.count == 1 && parsed.entries[0].generation == 5 && memcmp(parsed.entries[0].profile, state.profile, 64) == 0,
        "count zero retains only old current");

    history.count = 2;
    history.entries[0].generation = 4; memcpy(history.entries[0].profile, obscura64_profiles_v1[1], 64);
    history.entries[1].generation = 3; memcpy(history.entries[1].profile, state.profile, 64);
    RCHECK(obscura64_history_serialize(&history, blob) == OBSCURA64_CORE_STATUS_SUCCESS &&
        file_bytes(current_path, current_blob, 160, 1) && file_bytes(history_path, blob, 336, 1),
        "nonadjacent generations repeat Profile fixture");
    RCHECK(obscura64_force_reinitialize(path_utf8, &context) == OBSCURA64_OK, "repeated nonadjacent Profile Force succeeds");
    obscura64_context_destroy(context);
    RCHECK(file_bytes(history_path, after, 336, 0) && obscura64_history_deserialize(after, 336, &parsed) == OBSCURA64_CORE_STATUS_SUCCESS &&
        parsed.count == 3 && parsed.entries[0].generation == 5 && parsed.entries[1].generation == 4 && parsed.entries[2].generation == 3 &&
        memcmp(parsed.entries[0].profile, parsed.entries[2].profile, 64) == 0 &&
        memcmp(parsed.entries[0].profile, parsed.entries[1].profile, 64) != 0,
        "retention distinguishes generation rather than global Profile uniqueness");
    return 0;
}

static int no_replacement_temps(const WCHAR *project, const WCHAR *pattern_suffix)
{
    WCHAR pattern[MAX_PATH * 2];
    WIN32_FIND_DATAW entry;
    HANDLE search;
    if (!filename(pattern, project, pattern_suffix)) return 0;
    search = FindFirstFileW(pattern, &entry);
    if (search != INVALID_HANDLE_VALUE) { FindClose(search); return 0; }
    return GetLastError() == ERROR_FILE_NOT_FOUND;
}
static int test_force(void)
{
    WCHAR temp[MAX_PATH], unique[MAX_PATH], current_path[MAX_PATH * 2], history_path[MAX_PATH * 2];
    WCHAR lock_path[MAX_PATH * 2], extra_path[MAX_PATH * 2];
    char utf8[3][MAX_PATH * 6];
    unsigned char before[160], after[160], history_blob[336], saved_history[336];
    unsigned char old_id[16];
    obscura64_project_state old_state, state, snapshots[9];
    obscura64_project_history history;
    obscura64_context *old_context = NULL, *new_context = NULL, *reopened = NULL;
    obscura64_provider provider;
    unsigned int callbacks = 0, i;
    HANDLE held;
    const unsigned char sample[] = {0, 1, 2, 255, 31};
    char encoded[16];
    unsigned char decoded[16];
    size_t encoded_len, decoded_len;
    uint16_t id;

    RCHECK(GetTempPathW(MAX_PATH, temp) != 0 && GetTempFileNameW(temp, L"o64", 0, unique) != 0, "unique temp path");
    RCHECK(DeleteFileW(unique), "remove temporary path reservation");
    RCHECK(filename(projects[0], unique, L"-Force-测试") && filename(projects[1], unique, L"-missing") &&
        filename(projects[2], unique, L"-empty"), "test project paths");
    (void)atexit(cleanup_tests);
    for (i = 0; i < 3; ++i) {
        RCHECK(CreateDirectoryW(projects[i], NULL), "create own project root");
        RCHECK(WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, projects[i], -1, utf8[i],
            sizeof(utf8[i]), NULL, NULL) != 0, "UTF-8 project conversion");
    }
    RCHECK(filename(current_path, projects[0], L"\\.obscura64\\current.state") &&
        filename(history_path, projects[0], L"\\.obscura64\\history.state") &&
        filename(lock_path, projects[0], L"\\.obscura64\\operation.lock"), "state paths");
    RCHECK(obscura64_force_reinitialize(utf8[1], &new_context) == OBSCURA64_STATE_MISSING && new_context == NULL,
        "missing container does not initialize");
    RCHECK(filename(extra_path, projects[1], L"\\.obscura64") && GetFileAttributesW(extra_path) == INVALID_FILE_ATTRIBUTES,
        "Force leaves missing container absent");
    RCHECK(filename(extra_path, projects[2], L"\\.obscura64") && CreateDirectoryW(extra_path, NULL), "empty state container");
    RCHECK(obscura64_force_reinitialize(utf8[2], &new_context) == OBSCURA64_STATE_MISSING && new_context == NULL,
        "missing current rejected");
    RCHECK(obscura64_open(utf8[0], &old_context) == OBSCURA64_OK, "first initialization");
    RCHECK(file_bytes(current_path, before, 160, 0) &&
        obscura64_state_deserialize(before, 160, &old_state) == OBSCURA64_CORE_STATUS_SUCCESS, "read generation one");
    memcpy(old_id, old_state.project_id, 16);
    snapshots[1] = old_state;
    for (i = 0; i < 7; ++i) {
        RCHECK(obscura64_force_reinitialize(utf8[0], &new_context) == OBSCURA64_OK, "successful Force");
        RCHECK(file_bytes(current_path, after, 160, 0) &&
            obscura64_state_deserialize(after, 160, &state) == OBSCURA64_CORE_STATUS_SUCCESS, "new current valid");
        RCHECK(state.generation == (uint64_t)i + 2 && memcmp(state.project_id, old_id, 16) == 0,
            "generation 1 to 8 and stable identity");
        RCHECK(memcmp(state.profile, old_state.profile, 64) != 0 &&
            obscura64_profile_library_find((const unsigned char *)state.profile, &id), "different frozen Profile");
        RCHECK(file_bytes(history_path, history_blob, 336, 0) &&
            obscura64_history_deserialize(history_blob, 336, &history) == OBSCURA64_CORE_STATUS_SUCCESS, "history validates");
        RCHECK(history.count == (i < 3 ? i + 1U : 3U) && history.entries[0].generation == old_state.generation &&
            memcmp(history.entries[0].profile, old_state.profile, 64) == 0 &&
            memcmp(history.project_id, old_id, 16) == 0, "immediate previous stored");
        {
            size_t j;
            int zero = 1;
            for (j = 0; j < history.count; ++j) {
                uint64_t expected = state.generation - 1U - j;
                RCHECK(history.entries[j].generation == expected &&
                    memcmp(history.entries[j].profile, snapshots[expected].profile, 64) == 0,
                    "every retained generation matches its complete historical snapshot");
                if (state.generation >= 5)
                    RCHECK(history.entries[j].generation != 1, "generation one evicted by generation, not Profile");
            }
            for (j = 64 + history.count * 80U; j < 304; ++j) if (history_blob[j] != 0) zero = 0;
            RCHECK(zero, "all unused entries zero");
        }
        if (i == 0) RCHECK(memcmp(old_context->profile, old_state.profile, 64) == 0, "old context retains old Profile");
        RCHECK(obscura64_encode(new_context, sample, sizeof(sample), encoded, sizeof(encoded), &encoded_len) == OBSCURA64_OK,
            "new context encodes");
        obscura64_context_destroy(new_context); new_context = NULL;
        RCHECK(obscura64_open(utf8[0], &reopened) == OBSCURA64_OK, "reopen loads new Profile");
        RCHECK(obscura64_decode(reopened, encoded, encoded_len, decoded, sizeof(decoded), &decoded_len) == OBSCURA64_OK &&
            decoded_len == sizeof(sample) && memcmp(decoded, sample, sizeof(sample)) == 0, "reopen decodes Force data");
        obscura64_context_destroy(reopened); reopened = NULL;
        snapshots[state.generation] = state;
        old_state = state; memcpy(before, after, 160);
        RCHECK(GetFileAttributesW(lock_path) == INVALID_FILE_ATTRIBUTES, "Force releases own lock");
    }
    obscura64_context_destroy(old_context); old_context = NULL;
    provider = *obscura64_builtin_provider(); provider.user_data = &callbacks; provider.encode = provider_encode;
    RCHECK(obscura64_force_reinitialize_with_provider(utf8[0], &provider, &new_context) == OBSCURA64_OK, "custom Provider Force");
    RCHECK(obscura64_encode(new_context, sample, sizeof(sample), encoded, sizeof(encoded), &encoded_len) == OBSCURA64_OK &&
        callbacks == 1, "custom Provider callback executed");
    obscura64_context_destroy(new_context); new_context = NULL;
    RCHECK(file_bytes(current_path, before, 160, 0) && obscura64_state_deserialize(before, 160, &state) == OBSCURA64_CORE_STATUS_SUCCESS &&
        state.generation == 9 && memcmp(state.project_id, old_id, 16) == 0, "Provider does not alter identity rules");
    RCHECK(file_bytes(history_path, saved_history, 336, 0) &&
        obscura64_history_deserialize(saved_history, 336, &history) == OBSCURA64_CORE_STATUS_SUCCESS &&
        history.count == 3 && history.entries[0].generation == 8 && memcmp(history.entries[0].profile, old_state.profile, 64) == 0 &&
        history.entries[1].generation == 7 && memcmp(history.entries[1].profile, snapshots[7].profile, 64) == 0 &&
        history.entries[2].generation == 6 && memcmp(history.entries[2].profile, snapshots[6].profile, 64) == 0,
        "custom Provider preserves old history");

    held = CreateFileW(lock_path, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    RCHECK(held != INVALID_HANDLE_VALUE, "manual busy lock");
    RCHECK(obscura64_force_reinitialize(utf8[0], &new_context) == OBSCURA64_BUSY && new_context == NULL, "busy status");
    RCHECK(file_bytes(current_path, after, 160, 0) && memcmp(before, after, 160) == 0 &&
        file_bytes(history_path, history_blob, 336, 0) && memcmp(saved_history, history_blob, 336) == 0, "busy leaves both states unchanged");
    CloseHandle(held);
    RCHECK(GetFileAttributesW(lock_path) != INVALID_FILE_ATTRIBUTES && DeleteFileW(lock_path), "Force did not delete another owner's lock");
    RCHECK(obscura64_force_reinitialize(utf8[0], &new_context) == OBSCURA64_OK, "Force works after busy lock removed");
    obscura64_context_destroy(new_context); new_context = NULL;
    RCHECK(file_bytes(current_path, before, 160, 0) && file_bytes(history_path, saved_history, 336, 0), "save current/history failure baseline");

    memcpy(history_blob, saved_history, 336); history_blob[72] ^= 1;
    RCHECK(file_bytes(history_path, history_blob, 336, 1), "corrupt history fixture");
    RCHECK(obscura64_force_reinitialize(utf8[0], &new_context) == OBSCURA64_STATE_CORRUPT && new_context == NULL,
        "corrupt history rejected");
    RCHECK(file_bytes(current_path, after, 160, 0) && memcmp(before, after, 160) == 0, "corrupt history leaves current unchanged");
    RCHECK(file_bytes(history_path, history_blob, 336, 0) && history_blob[72] == (unsigned char)(saved_history[72] ^ 1),
        "corrupt history not overwritten");
    memcpy(history_blob, saved_history, 336); history_blob[32] ^= 1; history_blob[33] |= 1;
    RCHECK(rehash(history_blob, 304) && file_bytes(history_path, history_blob, 336, 1), "different valid history identity fixture");
    RCHECK(obscura64_force_reinitialize(utf8[0], &new_context) == OBSCURA64_STATE_CORRUPT && new_context == NULL,
        "different history Project ID rejected");
    RCHECK(file_bytes(current_path, after, 160, 0) && memcmp(before, after, 160) == 0, "foreign identity leaves current unchanged");
    RCHECK(file_bytes(history_path, saved_history, 336, 1), "restore test history");

    held = CreateFileW(history_path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    RCHECK(held != INVALID_HANDLE_VALUE, "hold history without delete sharing");
    RCHECK(obscura64_force_reinitialize(utf8[0], &new_context) == OBSCURA64_IO_ERROR && new_context == NULL,
        "history replacement failure returned");
    CloseHandle(held);
    RCHECK(file_bytes(current_path, after, 160, 0) && memcmp(before, after, 160) == 0 &&
        file_bytes(history_path, history_blob, 336, 0) && memcmp(saved_history, history_blob, 336) == 0,
        "history write failure leaves current and prior history untouched");
    RCHECK(GetFileAttributesW(lock_path) == INVALID_FILE_ATTRIBUTES, "history failure releases lock");
    RCHECK(no_replacement_temps(projects[0], L"\\.obscura64\\history.state.tmp.*"), "failed history temp cleaned");

    held = CreateFileW(current_path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    RCHECK(held != INVALID_HANDLE_VALUE, "hold current without delete sharing");
    RCHECK(obscura64_force_reinitialize(utf8[0], &new_context) == OBSCURA64_IO_ERROR && new_context == NULL,
        "current replacement failure returned");
    CloseHandle(held);
    RCHECK(file_bytes(current_path, after, 160, 0) && memcmp(before, after, 160) == 0, "failed current replace keeps old current");
    RCHECK(obscura64_state_deserialize(before, 160, &state) == OBSCURA64_CORE_STATUS_SUCCESS &&
        file_bytes(history_path, history_blob, 336, 0) && obscura64_history_deserialize(history_blob, 336, &history) == OBSCURA64_CORE_STATUS_SUCCESS &&
        history.entries[0].generation == state.generation && memcmp(history.entries[0].profile, state.profile, 64) == 0,
        "history first: old Profile retained when current replacement fails");
    RCHECK(GetFileAttributesW(lock_path) == INVALID_FILE_ATTRIBUTES, "current failure releases lock");
    RCHECK(no_replacement_temps(projects[0], L"\\.obscura64\\current.state.tmp.*"), "failed current temp cleaned");

    memcpy(saved_history, history_blob, 336);
    RCHECK(obscura64_force_reinitialize(utf8[0], &new_context) == OBSCURA64_OK,
        "retry real failed-current-commit successfully normalizes overlap");
    obscura64_context_destroy(new_context); new_context = NULL;
    RCHECK(file_bytes(current_path, after, 160, 0) &&
        obscura64_state_deserialize(after, 160, &old_state) == OBSCURA64_CORE_STATUS_SUCCESS &&
        old_state.generation == state.generation + 1 && memcmp(old_state.project_id, old_id, 16) == 0,
        "normalization retry advances once and preserves identity");
    RCHECK(file_bytes(history_path, history_blob, 336, 0) && memcmp(saved_history, history_blob, 336) == 0 &&
        obscura64_history_deserialize(history_blob, 336, &history) == OBSCURA64_CORE_STATUS_SUCCESS &&
        history.count == 3 && history.entries[0].generation > history.entries[1].generation &&
        history.entries[1].generation > history.entries[2].generation,
        "real failure retry keeps three distinct generations without double insertion");
    memcpy(before, after, 160);

    memcpy(after, before, 160); after[70] ^= 1;
    RCHECK(file_bytes(current_path, after, 160, 1), "corrupt current fixture");
    RCHECK(obscura64_force_reinitialize(utf8[0], &new_context) == OBSCURA64_STATE_CORRUPT && new_context == NULL,
        "corrupt current never repaired");
    RCHECK(file_bytes(current_path, after, 160, 0) && after[70] == (unsigned char)(before[70] ^ 1), "corrupt current unchanged");
    RCHECK(file_bytes(current_path, before, 160, 1), "restore current fixture");
    RCHECK(obscura64_state_deserialize(before, 160, &state) == OBSCURA64_CORE_STATUS_SUCCESS, "overflow baseline");
    state.generation = UINT64_MAX;
    RCHECK(obscura64_state_serialize(&state, before) == OBSCURA64_CORE_STATUS_SUCCESS && file_bytes(current_path, before, 160, 1) &&
        file_bytes(history_path, saved_history, 336, 0), "valid max generation fixture");
    RCHECK(obscura64_force_reinitialize(utf8[0], &new_context) == OBSCURA64_SIZE_OVERFLOW && new_context == NULL, "overflow rejected");
    RCHECK(file_bytes(current_path, after, 160, 0) && memcmp(before, after, 160) == 0 &&
        file_bytes(history_path, history_blob, 336, 0) && memcmp(saved_history, history_blob, 336) == 0, "overflow changes neither file");
    RCHECK(GetFileAttributesW(lock_path) == INVALID_FILE_ATTRIBUTES, "overflow lock cleanup");
    RCHECK(strcmp(obscura64_status_string(OBSCURA64_BUSY), "project operation busy") == 0, "BUSY status string");
    RCHECK(obscura64_force_reinitialize(NULL, &new_context) == OBSCURA64_INVALID_ARGUMENT && new_context == NULL, "null Force path");
    RCHECK(obscura64_force_reinitialize("\xC3\x28", &new_context) == OBSCURA64_INVALID_ARGUMENT && new_context == NULL, "invalid UTF-8 Force path");
    RCHECK(obscura64_force_reinitialize_with_provider(utf8[0], NULL, &new_context) == OBSCURA64_INVALID_PROVIDER && new_context == NULL,
        "invalid Provider rejected before writes");
    if (test_history_relationships(utf8[2]) != 0) return 1;
    printf("PASS: %u reinitialize checks\n", force_checks);
    return 0;
}

int main(void)
{
    if (test_history_format() != 0) return 1;
    return test_force();
}
