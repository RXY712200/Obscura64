#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "obscura64_internal.h"
#include <stdint.h>
#include <stdlib.h>

obscura64_status obscura64_path_from_utf8(const char *path_utf8, wchar_t **path_out)
{
    int required;
    wchar_t *path;
    if (path_out != NULL) *path_out = NULL;
    if (path_utf8 == NULL || path_out == NULL || path_utf8[0] == '\0')
        return OBSCURA64_INVALID_ARGUMENT;
    required = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        path_utf8, -1, NULL, 0);
    if (required == 0) return OBSCURA64_INVALID_ARGUMENT;
    if ((size_t)required > SIZE_MAX / sizeof(*path)) return OBSCURA64_SIZE_OVERFLOW;
    path = (wchar_t *)malloc((size_t)required * sizeof(*path));
    if (path == NULL) return OBSCURA64_OUT_OF_MEMORY;
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        path_utf8, -1, path, required) != required) {
        free(path);
        return OBSCURA64_INVALID_ARGUMENT;
    }
    *path_out = path;
    return OBSCURA64_OK;
}
