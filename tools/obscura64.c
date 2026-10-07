#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "obscura64.h"
#include "obscura64_internal.h"
#include "obscura64_v2_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

static const char *format_name(obscura64_data_format format)
{
    return format == OBSCURA64_FORMAT_V2 ? "V2" :
        format == OBSCURA64_FORMAT_V1_LEGACY_UPGRADE_RECOMMENDED ?
        "V1 legacy" : "unrecognized";
}

static const char *protection_name(unsigned int kind)
{
    switch (kind) {
    case OBSCURA64_PROTECTION_NONE: return "NONE";
    case OBSCURA64_PROTECTION_CASUAL: return "CASUAL";
    case OBSCURA64_PROTECTION_CURRENT_USER: return "CURRENT_USER";
    default: return "unsupported/unknown";
    }
}

static int target_from_name(const wchar_t *name, obscura64_protection *target)
{
    if (wcscmp(name, L"none") == 0) *target = OBSCURA64_PROTECTION_NONE;
    else if (wcscmp(name, L"casual") == 0) *target = OBSCURA64_PROTECTION_CASUAL;
    else if (wcscmp(name, L"current-user") == 0)
        *target = OBSCURA64_PROTECTION_CURRENT_USER;
    else return 0;
    return 1;
}

/* Resolve CLI-relative UTF-16 input without changing the library path contract. */
static obscura64_status cli_path(const wchar_t *argument,
    wchar_t **wide_path, char **utf8_path)
{
    DWORD needed, written;
    int count;
    wchar_t *wide;
    char *utf8;
    *wide_path = NULL; *utf8_path = NULL;
    if (argument == NULL || argument[0] == L'\0') return OBSCURA64_INVALID_ARGUMENT;
    needed = GetFullPathNameW(argument, 0, NULL, NULL);
    if (needed == 0) return OBSCURA64_IO_ERROR;
#if SIZE_MAX <= UINT32_MAX
    if (needed > SIZE_MAX / sizeof(wchar_t)) return OBSCURA64_SIZE_OVERFLOW;
#endif
    wide = (wchar_t *)malloc((size_t)needed * sizeof(wchar_t));
    if (wide == NULL) return OBSCURA64_OUT_OF_MEMORY;
    written = GetFullPathNameW(argument, needed, wide, NULL);
    if (written == 0 || written >= needed) { free(wide); return OBSCURA64_IO_ERROR; }
    count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
        wide, -1, NULL, 0, NULL, NULL);
    if (count == 0) { free(wide); return OBSCURA64_INVALID_ARGUMENT; }
    utf8 = (char *)malloc((size_t)count);
    if (utf8 == NULL) { free(wide); return OBSCURA64_OUT_OF_MEMORY; }
    if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
        wide, -1, utf8, count, NULL, NULL)) {
        free(wide); free(utf8); return OBSCURA64_INVALID_ARGUMENT;
    }
    *wide_path = wide; *utf8_path = utf8;
    return OBSCURA64_OK;
}

static int usage(void)
{
    fprintf(stderr, "usage: obscura64 inspect <file> | verify <file> | "
        "upgrade <file> --to none|casual|current-user\n");
    return 2;
}

int wmain(int argc, wchar_t **argv)
{
    enum { INSPECT, VERIFY, UPGRADE } command;
    wchar_t *wide = NULL;
    char *utf8 = NULL;
    unsigned char *bytes = NULL;
    size_t length = 0, plain_len = 0;
    void *plain = NULL;
    obscura64_status status;
    obscura64_protection target = OBSCURA64_PROTECTION_NONE;
    int exit_code = 1;
    if (argc < 2) return usage();
    if (wcscmp(argv[1], L"inspect") == 0) command = INSPECT;
    else if (wcscmp(argv[1], L"verify") == 0) command = VERIFY;
    else if (wcscmp(argv[1], L"upgrade") == 0) command = UPGRADE;
    else return usage();
    if ((command != UPGRADE && argc != 3) ||
        (command == UPGRADE && (argc != 5 ||
        wcscmp(argv[3], L"--to") != 0 ||
        !target_from_name(argv[4], &target)))) return usage();
    status = cli_path(argv[2], &wide, &utf8);
    if (status != OBSCURA64_OK) goto fail;
    if (command == UPGRADE) {
        obscura64_upgrade_result result;
        status = obscura64_upgrade_file_ex(utf8, target, &result);
        if (status != OBSCURA64_OK) goto fail;
        printf("source: %s\nformat: %s\n",
            result.source == OBSCURA64_FILE_SOURCE_PRIMARY ? "PRIMARY" : "BACKUP",
            format_name(result.format));
        if (result.format == OBSCURA64_FORMAT_V2)
            printf("source protection: %s\n",
                protection_name(result.source_protection));
        printf("target protection: %s\nchanged: %s\n",
            protection_name(result.target_protection), result.changed ? "yes" : "no");
    } else {
        status = obscura64_file_read_all(wide, &bytes, &length);
        if (status != OBSCURA64_OK) goto fail;
        if (command == INSPECT) {
            obscura64_inspection info;
            status = obscura64_inspect_bytes(bytes, length, &info);
            printf("format: %s\nprotected bytes: %zu\n",
                format_name(info.format), length);
            if (info.format == OBSCURA64_FORMAT_V2) {
                printf("envelope version: %u\n", info.version);
                if (status == OBSCURA64_OK ||
                    status == OBSCURA64_UNSUPPORTED_PROTECTION)
                    printf("protection: %s\n", protection_name(info.protection_kind));
                printf("supported current representation: %s\n",
                    info.current_representation ? "yes" : "no");
            } else if (status == OBSCURA64_OK) {
                printf("upgrade recommended: yes\n");
            }
            printf("structural status: %s\n", obscura64_status_string(status));
            if (status != OBSCURA64_OK) goto fail;
        } else {
            obscura64_data_format format = OBSCURA64_FORMAT_UNKNOWN;
            status = obscura64_unprotect_alloc_ex(bytes, length,
                &plain, &plain_len, &format);
            if (status != OBSCURA64_OK) goto fail;
            printf("format: %s\n", format_name(format));
            if (format == OBSCURA64_FORMAT_V2) {
                obscura64_inspection info;
                status = obscura64_inspect_bytes(bytes, length, &info);
                if (status != OBSCURA64_OK) goto fail;
                printf("protection: %s\n", protection_name(info.protection_kind));
                switch (info.protection_kind) {
                case OBSCURA64_PROTECTION_NONE:
                    printf("verification: structurally valid V2 NONE "
                        "(canonical envelope and exact length)\n"
                        "integrity: not provided by NONE\n");
                    break;
                case OBSCURA64_PROTECTION_CASUAL:
                    printf("verification: Casual format/digest valid\n"
                        "authentication: not provided\n");
                    break;
                case OBSCURA64_PROTECTION_CURRENT_USER:
                    printf("verification: valid under current Windows DPAPI context\n"
                        "inner record: valid\n");
                    break;
                default:
                    status = OBSCURA64_UNSUPPORTED_PROTECTION;
                    goto fail;
                }
            } else {
                printf("verification: V1 Managed format/digest valid\n"
                    "authenticity: not cryptographically established\n");
            }
        }
    }
    exit_code = 0;
    goto done;
fail:
    fprintf(stderr, "Obscura64: %s\n", obscura64_status_string(status));
done:
    obscura64_free(plain);
    free(bytes); free(wide); free(utf8);
    return exit_code;
}
