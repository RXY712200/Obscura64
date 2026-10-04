#ifndef OBSCURA64_PROFILE_GENERATOR_INTERNAL_H
#define OBSCURA64_PROFILE_GENERATOR_INTERNAL_H

#include <stdint.h>
#include <stddef.h>

#define OBSCURA64_PROFILE_LIBRARY_COUNT 4096U
#define OBSCURA64_PROFILE_BYTES 64U
#define OBSCURA64_PROFILE_GENERATOR_RETRY_LIMIT 1024U

typedef struct obscura64_profile_generator_stats {
    size_t accepted_count;
    size_t generation_attempts;
    size_t collisions;
    size_t pair_comparisons;
} obscura64_profile_generator_stats;

typedef enum obscura64_profile_generator_status {
    OBSCURA64_PROFILE_GENERATOR_SUCCESS = 0,
    OBSCURA64_PROFILE_GENERATOR_INVALID_ARGUMENT,
    OBSCURA64_PROFILE_GENERATOR_RANDOM_FAILURE,
    OBSCURA64_PROFILE_GENERATOR_VALIDATION_FAILURE,
    OBSCURA64_PROFILE_GENERATOR_OUT_OF_MEMORY,
    OBSCURA64_PROFILE_GENERATOR_RETRIES_EXHAUSTED,
    OBSCURA64_PROFILE_GENERATOR_DUPLICATE,
    OBSCURA64_PROFILE_GENERATOR_IO_FAILURE,
    OBSCURA64_PROFILE_GENERATOR_INVALID_CANDIDATE
} obscura64_profile_generator_status;

obscura64_profile_generator_status obscura64_profile_generator_random_uniform(
    uint32_t bound,
    uint32_t *value);
obscura64_profile_generator_status obscura64_profile_generator_generate(char profile[65]);
obscura64_profile_generator_status obscura64_profile_generator_generate_unique_set(
    uint8_t *profiles,
    size_t count,
    obscura64_profile_generator_stats *stats);
obscura64_profile_generator_status obscura64_profile_generator_validate_unique_set(
    const uint8_t *profiles,
    size_t count,
    size_t *pair_comparisons);
obscura64_profile_generator_status obscura64_profile_generator_write_candidate(
    const char *path,
    const uint8_t *profiles,
    size_t count);
obscura64_profile_generator_status obscura64_profile_generator_validate_candidate(
    const char *path,
    obscura64_profile_generator_stats *stats);

#endif /* OBSCURA64_PROFILE_GENERATOR_INTERNAL_H */
