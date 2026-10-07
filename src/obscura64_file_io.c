#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "obscura64_internal.h"
#include <stdint.h>
#include <stdlib.h>

obscura64_status obscura64_file_read_all(const wchar_t *path,
    unsigned char **output, size_t *length)
{
    HANDLE file;
    LARGE_INTEGER size;
    unsigned char *bytes = NULL, extra;
    size_t position = 0, count_size = 0;
    DWORD count;
    obscura64_status status = OBSCURA64_IO_ERROR;
    if (output != NULL) *output = NULL;
    if (length != NULL) *length = 0;
    if (path == NULL || path[0] == L'\0' || output == NULL || length == NULL)
        return OBSCURA64_INVALID_ARGUMENT;
    file = CreateFileW(path, GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND ?
            OBSCURA64_FILE_NOT_FOUND : OBSCURA64_IO_ERROR;
    }
    if (GetFileType(file) != FILE_TYPE_DISK || !GetFileSizeEx(file, &size)) goto done;
    if (size.QuadPart < 0 || (uint64_t)size.QuadPart > (uint64_t)SIZE_MAX) {
        status = OBSCURA64_SIZE_OVERFLOW; goto done;
    }
    count_size = (size_t)size.QuadPart;
    bytes = (unsigned char *)malloc(count_size == 0 ? 1 : count_size);
    if (bytes == NULL) { status = OBSCURA64_OUT_OF_MEMORY; goto done; }
    while (position < count_size) {
        size_t remaining = count_size - position;
        DWORD request = remaining > MAXDWORD ? MAXDWORD : (DWORD)remaining;
        if (!ReadFile(file, bytes + position, request, &count, NULL)) goto done;
        if (count == 0 || count > request) goto done;
        position += count;
    }
    if (!ReadFile(file, &extra, 1, &count, NULL) || count != 0) goto done;
    status = OBSCURA64_OK;
done:
    if (!CloseHandle(file)) status = OBSCURA64_IO_ERROR;
    if (status == OBSCURA64_OK) { *output = bytes; *length = count_size; }
    else free(bytes);
    return status;
}
