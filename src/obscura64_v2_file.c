#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "obscura64_internal.h"

#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

typedef struct file_paths {
    wchar_t *target;
    wchar_t *backup;
    wchar_t *lock;
} file_paths;

typedef struct file_copy {
    unsigned char *bytes;
    size_t length;
    obscura64_status status;
} file_copy;

static int suffix_is(const wchar_t *name, const wchar_t *suffix)
{
    size_t n = wcslen(name), s = wcslen(suffix), i;
    if (n < s) return 0;
    for (i = 0; i < s; ++i)
        if (towlower(name[n - s + i]) != towlower(suffix[i])) return 0;
    return 1;
}

static int reserved_name(const wchar_t *path)
{
    const wchar_t *name = path, *p;
    size_t n;
    for (p = path; *p != L'\0'; ++p)
        if (*p == L'\\' || *p == L'/') name = p + 1;
    if (suffix_is(name, L".ob64.bak") || suffix_is(name, L".ob64.lock")) return 1;
    n = wcslen(name);
    if (n >= 21) {
        const wchar_t *tail = name + n - 21;
        size_t i;
        if (towlower(tail[0]) == L'.' && towlower(tail[1]) == L't' &&
            towlower(tail[2]) == L'm' && towlower(tail[3]) == L'p' &&
            tail[4] == L'.') {
            for (i = 5; i < 21; ++i) {
                wchar_t c = towlower(tail[i]);
                if (!((c >= L'0' && c <= L'9') || (c >= L'a' && c <= L'f'))) break;
            }
            if (i == 21) return 1;
        }
    }
    return 0;
}

static int absolute_path(const wchar_t *p)
{
    if (p[0] == L'\\' && p[1] == L'\\' && p[2] == L'.' &&
        p[3] == L'\\') return 0; /* Win32 device namespace. */
    return (((p[0] >= L'A' && p[0] <= L'Z') ||
            (p[0] >= L'a' && p[0] <= L'z')) && p[1] == L':' &&
            (p[2] == L'\\' || p[2] == L'/')) ||
           (p[0] == L'\\' && p[1] == L'\\' && p[2] != L'\0');
}

static int dos_device_name(const wchar_t *part, size_t length)
{
    size_t stem = 0;
    wchar_t a, b, c;
    while (stem < length && part[stem] != L'.') ++stem;
    if (stem == 6 && towupper(part[0]) == L'C' &&
        towupper(part[1]) == L'O' && towupper(part[2]) == L'N' &&
        towupper(part[3]) == L'I' && towupper(part[4]) == L'N' &&
        part[5] == L'$') return 1;
    if (stem == 7 && towupper(part[0]) == L'C' &&
        towupper(part[1]) == L'O' && towupper(part[2]) == L'N' &&
        towupper(part[3]) == L'O' && towupper(part[4]) == L'U' &&
        towupper(part[5]) == L'T' && part[6] == L'$') return 1;
    if (stem != 3 && stem != 4) return 0;
    a = towupper(part[0]); b = towupper(part[1]); c = towupper(part[2]);
    if (stem == 3) return (a == L'C' && b == L'O' && c == L'N') ||
        (a == L'P' && b == L'R' && c == L'N') ||
        (a == L'A' && b == L'U' && c == L'X') ||
        (a == L'N' && b == L'U' && c == L'L');
    return ((a == L'C' && b == L'O' && c == L'M') ||
            (a == L'L' && b == L'P' && c == L'T')) &&
           ((part[3] >= L'1' && part[3] <= L'9') ||
            part[3] == 0x00b9 || part[3] == 0x00b2 || part[3] == 0x00b3);
}

static int ambiguous_components(const wchar_t *path)
{
    const wchar_t *start = path, *p;
    size_t i = 0;
    for (p = path; ; ++p, ++i) {
        if (*p == L':' && i != 1 && !(i == 5 &&
            wcsncmp(path, L"\\\\?\\", 4) == 0)) return 1; /* No ADS. */
        if (*p == L'\\' || *p == L'/' || *p == L'\0') {
            size_t len = (size_t)(p - start);
            if (len != 0 && (start[len - 1] == L' ' ||
                start[len - 1] == L'.')) return 1;
            if (len != 0 && dos_device_name(start, len)) return 1;
            if (*p == L'\0') break;
            start = p + 1;
        }
    }
    return 0;
}

static wchar_t *add_suffix(const wchar_t *base, const wchar_t *suffix)
{
    size_t a = wcslen(base), b = wcslen(suffix);
    wchar_t *result;
    if (a > SIZE_MAX / sizeof(wchar_t) - b - 1U) return NULL;
    result = (wchar_t *)malloc((a + b + 1U) * sizeof(wchar_t));
    if (result != NULL) {
        memcpy(result, base, a * sizeof(wchar_t));
        memcpy(result + a, suffix, (b + 1U) * sizeof(wchar_t));
    }
    return result;
}

static void paths_free(file_paths *p)
{
    free(p->target); free(p->backup); free(p->lock);
}

static obscura64_status paths_make(const char *utf8, file_paths *paths)
{
    obscura64_status status;
    wchar_t *normalized;
    DWORD needed;
    memset(paths, 0, sizeof(*paths));
    status = obscura64_path_from_utf8(utf8, &paths->target);
    if (status != OBSCURA64_OK) return status;
    if (!absolute_path(paths->target) || ambiguous_components(paths->target)) {
        paths_free(paths); return OBSCURA64_INVALID_ARGUMENT;
    }
    needed = GetFullPathNameW(paths->target, 0, NULL, NULL);
    if (needed == 0) {
        paths_free(paths); return OBSCURA64_IO_ERROR;
    }
#if SIZE_MAX <= UINT32_MAX
    if (needed > SIZE_MAX / sizeof(wchar_t)) {
        paths_free(paths); return OBSCURA64_SIZE_OVERFLOW;
    }
#endif
    normalized = (wchar_t *)malloc((size_t)needed * sizeof(wchar_t));
    if (normalized == NULL) { paths_free(paths); return OBSCURA64_OUT_OF_MEMORY; }
    {
    DWORD written = GetFullPathNameW(paths->target, needed, normalized, NULL);
    if (written == 0 || written >= needed) {
        free(normalized); paths_free(paths); return OBSCURA64_IO_ERROR;
    }
    }
    free(paths->target);
    paths->target = normalized;
    if (reserved_name(paths->target)) {
        paths_free(paths); return OBSCURA64_INVALID_ARGUMENT;
    }
    paths->backup = add_suffix(paths->target, L".ob64.bak");
    paths->lock = add_suffix(paths->target, L".ob64.lock");
    if (paths->backup == NULL || paths->lock == NULL) {
        paths_free(paths); return OBSCURA64_OUT_OF_MEMORY;
    }
    return OBSCURA64_OK;
}

int obscura64_file_read_fallback_eligible(obscura64_status status)
{
    switch (status) {
    case OBSCURA64_FILE_NOT_FOUND:
    case OBSCURA64_ENVELOPE_CORRUPT:
    case OBSCURA64_CASUAL_CORRUPT:
    case OBSCURA64_PROTECTED_CORRUPT:
    case OBSCURA64_UNRECOGNIZED_DATA:
    case OBSCURA64_PROTECTION_FAILURE:
    case OBSCURA64_LEGACY_AMBIGUOUS:
        return 1;
    default: return 0;
    }
}

int obscura64_file_write_replaceable_failure(obscura64_status status)
{
    switch (status) {
    case OBSCURA64_FILE_NOT_FOUND:
    case OBSCURA64_ENVELOPE_CORRUPT:
    case OBSCURA64_CASUAL_CORRUPT:
    case OBSCURA64_PROTECTED_CORRUPT:
    case OBSCURA64_UNRECOGNIZED_DATA:
        return 1;
    default:
        return 0;
    }
}

static obscura64_status validate_bytes(const unsigned char *bytes, size_t length,
    void **plain, size_t *plain_len, obscura64_data_format *format)
{
    return obscura64_unprotect_alloc_ex(bytes, length, plain, plain_len, format);
}

static obscura64_status verify_protected(const unsigned char *bytes,
    size_t length, void *unused)
{
    void *plain = NULL;
    size_t plain_len = 0;
    obscura64_status status;
    (void)unused;
    status = validate_bytes(bytes, length, &plain, &plain_len, NULL);
    obscura64_free(plain);
    return status;
}

static void copy_load(const wchar_t *path, file_copy *copy)
{
    void *plain = NULL;
    size_t plain_len = 0;
    memset(copy, 0, sizeof(*copy));
    copy->status = obscura64_file_read_all(path, &copy->bytes, &copy->length);
    if (copy->status == OBSCURA64_OK) {
        copy->status = validate_bytes(copy->bytes, copy->length,
            &plain, &plain_len, NULL);
        obscura64_free(plain);
    }
}

static obscura64_status release_with(obscura64_lock *lock,
    obscura64_status status)
{
    obscura64_status released = obscura64_lock_release(lock);
    return status == OBSCURA64_OK ? released : status;
}

obscura64_status obscura64_read_file_alloc_ex(const char *path_utf8,
    void **output, size_t *output_len, obscura64_file_source *source,
    obscura64_data_format *format)
{
    file_paths paths;
    obscura64_lock *lock;
    unsigned char *bytes = NULL;
    size_t length = 0;
    obscura64_status status, primary_status;
    void *plain = NULL;
    size_t plain_len = 0;
    obscura64_data_format found_format = OBSCURA64_FORMAT_UNKNOWN;
    obscura64_file_source found_source = OBSCURA64_FILE_SOURCE_UNKNOWN;
    if (output != NULL) *output = NULL;
    if (output_len != NULL) *output_len = 0;
    if (source != NULL) *source = OBSCURA64_FILE_SOURCE_UNKNOWN;
    if (format != NULL) *format = OBSCURA64_FORMAT_UNKNOWN;
    if (output == NULL || output_len == NULL) return OBSCURA64_INVALID_ARGUMENT;
    status = paths_make(path_utf8, &paths);
    if (status != OBSCURA64_OK) return status;
    status = obscura64_lock_acquire_shared(paths.lock, &lock);
    if (status != OBSCURA64_OK) {
        if (status == OBSCURA64_IO_ERROR &&
            GetFileAttributesW(paths.lock) == INVALID_FILE_ATTRIBUTES &&
            GetLastError() == ERROR_PATH_NOT_FOUND)
            status = OBSCURA64_FILE_NOT_FOUND;
        paths_free(&paths); return status;
    }
    primary_status = obscura64_file_read_all(paths.target, &bytes, &length);
    if (primary_status == OBSCURA64_OK)
        primary_status = validate_bytes(bytes, length, &plain, &plain_len, &found_format);
    free(bytes); bytes = NULL;
    if (primary_status == OBSCURA64_OK) {
        found_source = OBSCURA64_FILE_SOURCE_PRIMARY;
        status = OBSCURA64_OK;
    } else if (obscura64_file_read_fallback_eligible(primary_status)) {
        status = obscura64_file_read_all(paths.backup, &bytes, &length);
        if (status == OBSCURA64_OK)
            status = validate_bytes(bytes, length, &plain, &plain_len, &found_format);
        free(bytes);
        if (status == OBSCURA64_OK) found_source = OBSCURA64_FILE_SOURCE_BACKUP;
        else if (obscura64_file_read_fallback_eligible(status)) {
            if (primary_status != OBSCURA64_FILE_NOT_FOUND) status = primary_status;
        }
    } else status = primary_status;
    status = release_with(lock, status);
    paths_free(&paths);
    if (status == OBSCURA64_OK) {
        *output = plain; *output_len = plain_len;
        if (source != NULL) *source = found_source;
        if (format != NULL) *format = found_format;
    } else obscura64_free(plain);
    return status;
}

obscura64_status obscura64_read_file_alloc(const char *path_utf8,
    void **output, size_t *output_len)
{
    return obscura64_read_file_alloc_ex(path_utf8, output, output_len, NULL, NULL);
}

obscura64_status obscura64_write_file(const char *path_utf8,
    obscura64_protection protection, const void *input, size_t input_len)
{
    file_paths paths;
    obscura64_lock *lock;
    file_copy primary, backup;
    void *new_bytes = NULL;
    size_t new_length = 0;
    const unsigned char *recovery_bytes;
    size_t recovery_length;
    int renamed = 0;
    obscura64_status status;
    if (input == NULL && input_len != 0) return OBSCURA64_INVALID_ARGUMENT;
    status = paths_make(path_utf8, &paths);
    if (status != OBSCURA64_OK) return status;
    status = obscura64_lock_acquire_exclusive(paths.lock, &lock);
    if (status != OBSCURA64_OK) { paths_free(&paths); return status; }
    status = obscura64_protect_alloc(protection, input, input_len,
        &new_bytes, &new_length);
    if (status != OBSCURA64_OK) goto done;
    copy_load(paths.target, &primary);
    if (primary.status == OBSCURA64_OK) {
        status = obscura64_persistence_replace_blob_verified(paths.backup,
            primary.bytes, primary.length, verify_protected, NULL, &renamed);
        if (status != OBSCURA64_OK) goto done_primary;
        recovery_bytes = primary.bytes;
        recovery_length = primary.length;
    } else {
        if (!obscura64_file_write_replaceable_failure(primary.status)) {
            status = primary.status; goto done_primary;
        }
        copy_load(paths.backup, &backup);
        if (backup.status == OBSCURA64_OK) {
            recovery_bytes = backup.bytes;
            recovery_length = backup.length;
        } else if (obscura64_file_write_replaceable_failure(backup.status)) {
            status = obscura64_persistence_replace_blob_verified(paths.backup,
                (const unsigned char *)new_bytes, new_length,
                verify_protected, NULL, &renamed);
            if (status != OBSCURA64_OK) goto done_backup;
            recovery_bytes = (const unsigned char *)new_bytes;
            recovery_length = new_length;
        } else { status = backup.status; goto done_backup; }
    }
    status = obscura64_persistence_replace_blob_verified(paths.target,
        (const unsigned char *)new_bytes, new_length,
        verify_protected, NULL, &renamed);
    if (status != OBSCURA64_OK && renamed) {
        int restored = 0;
        if (obscura64_persistence_replace_blob_verified(paths.target,
            recovery_bytes, recovery_length, verify_protected, NULL,
            &restored) != OBSCURA64_OK) status = OBSCURA64_IO_ERROR;
    }
done_backup:
    if (primary.status != OBSCURA64_OK) free(backup.bytes);
done_primary:
    free(primary.bytes);
done:
    obscura64_free(new_bytes);
    status = release_with(lock, status);
    paths_free(&paths);
    return status;
}
