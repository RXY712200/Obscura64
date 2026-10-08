#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "obscura64.h"
#include <stdio.h>
#include <string.h>

#define THREAD_COUNT 6U
#define ITERATIONS 40U

typedef struct worker {
    const obscura64_context *context;
    obscura64_protection protection;
    unsigned int index;
} worker;

static DWORD WINAPI run_worker(void *argument)
{
    const worker *job = (const worker *)argument;
    unsigned int i;
    for (i = 0; i < ITERATIONS; ++i) {
        unsigned char input[5] = {0, 1, 255,
            (unsigned char)job->index, (unsigned char)i};
        void *protected_data = NULL, *decoded = NULL;
        char *encoded = NULL;
        size_t protected_len = 0, decoded_len = 0;
        size_t encoded_len = 0;
        obscura64_status status = obscura64_protect_alloc(job->protection,
            input, sizeof(input), &protected_data, &protected_len);
        if (status == OBSCURA64_OK)
            status = obscura64_unprotect_alloc(protected_data, protected_len,
                &decoded, &decoded_len);
        if (status != OBSCURA64_OK || decoded_len != sizeof(input) ||
            memcmp(decoded, input, sizeof(input)) != 0) {
            obscura64_free(decoded); obscura64_free(protected_data); return 1;
        }
        obscura64_free(decoded); decoded = NULL;
        obscura64_free(protected_data);
        status = obscura64_encode_alloc(job->context, input, sizeof(input),
            &encoded, &encoded_len);
        if (status == OBSCURA64_OK)
            status = obscura64_decode_alloc(job->context, encoded, encoded_len,
                &decoded, &decoded_len);
        if (status != OBSCURA64_OK || decoded_len != sizeof(input) ||
            memcmp(decoded, input, sizeof(input)) != 0) {
            obscura64_free(encoded); obscura64_free(decoded); return 2;
        }
        obscura64_free(encoded); obscura64_free(decoded);
    }
    return 0;
}

int main(void)
{
    static const char profile[] =
        "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnpqrstuvwxyz23456789@#$%&!?~";
    obscura64_context *context = NULL;
    worker jobs[THREAD_COUNT];
    HANDLE threads[THREAD_COUNT] = {0};
    unsigned int i;
    int success = 1;
    if (obscura64_context_create_from_profile(profile, 64, &context) !=
        OBSCURA64_OK) return 1;
    for (i = 0; i < THREAD_COUNT; ++i) {
        jobs[i].context = context;
        jobs[i].protection = (obscura64_protection)(i % 3U);
        jobs[i].index = i;
        threads[i] = CreateThread(NULL, 0, run_worker, &jobs[i], 0, NULL);
        if (threads[i] == NULL) { success = 0; break; }
    }
    for (i = 0; i < THREAD_COUNT; ++i) {
        DWORD code = 0;
        if (threads[i] == NULL) continue;
        if (WaitForSingleObject(threads[i], INFINITE) != WAIT_OBJECT_0 ||
            !GetExitCodeThread(threads[i], &code) || code != 0) success = 0;
        CloseHandle(threads[i]);
    }
    obscura64_context_destroy(context);
    if (!success) return 2;
    printf("thread workers=%u iterations=%u\n", THREAD_COUNT, ITERATIONS);
    return 0;
}
