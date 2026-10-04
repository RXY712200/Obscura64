#include "obscura64.h"
#include "obscura64_internal.h"

#include <stdlib.h>
#include <string.h>

static int obscura64_provider_is_valid(const obscura64_provider *provider)
{
    const size_t minimum_header_size =
        offsetof(obscura64_provider, abi_version) + sizeof(provider->abi_version);

    if (provider == NULL || provider->struct_size < minimum_header_size) {
        return 0;
    }
    if (provider->abi_version != OBSCURA64_PROVIDER_ABI_VERSION ||
        provider->struct_size < sizeof(*provider)) {
        return 0;
    }
    return provider->encoded_size != NULL &&
           provider->decoded_size != NULL &&
           provider->encode != NULL &&
           provider->decode != NULL;
}

obscura64_status obscura64_context_create_from_profile_with_provider(
    const char *profile,
    size_t profile_len,
    const obscura64_provider *provider,
    obscura64_context **out_context)
{
    obscura64_context *context;

    if (out_context == NULL) {
        return OBSCURA64_INVALID_ARGUMENT;
    }
    *out_context = NULL;
    if (profile == NULL) {
        return OBSCURA64_INVALID_ARGUMENT;
    }
    if (!obscura64_profile_is_valid(profile, profile_len)) {
        return OBSCURA64_INVALID_PROFILE;
    }
    if (!obscura64_provider_is_valid(provider)) {
        return OBSCURA64_INVALID_PROVIDER;
    }

    context = (obscura64_context *)malloc(sizeof(*context));
    if (context == NULL) {
        return OBSCURA64_OUT_OF_MEMORY;
    }
    memcpy(context->profile, profile, OBSCURA64_ALPHABET_SIZE);
    context->provider = *provider;
    *out_context = context;
    return OBSCURA64_OK;
}

obscura64_status obscura64_context_create_from_profile(
    const char *profile,
    size_t profile_len,
    obscura64_context **out_context)
{
    return obscura64_context_create_from_profile_with_provider(
        profile, profile_len, &OBSCURA64_DEFAULT_PROVIDER_SYMBOL, out_context);
}

obscura64_status obscura64_open(
    const char *project_path_utf8,
    obscura64_context **out_context)
{
    return obscura64_project_open_internal(
        project_path_utf8, &OBSCURA64_DEFAULT_PROVIDER_SYMBOL, out_context);
}

obscura64_status obscura64_open_with_provider(
    const char *project_path_utf8,
    const obscura64_provider *provider,
    obscura64_context **out_context)
{
    return obscura64_project_open_internal(project_path_utf8, provider, out_context);
}

void obscura64_context_destroy(obscura64_context *context)
{
    free(context);
}
