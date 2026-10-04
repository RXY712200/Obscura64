#include "obscura64_internal.h"
#include <string.h>

obscura64_status obscura64_managed_state_validate(
    const obscura64_project_state *current,
    const obscura64_project_history *history,
    obscura64_managed_state_kind *kind)
{
    uint64_t newest;
    uint16_t expected_count;
    size_t i;
    obscura64_managed_state_kind result = OBSCURA64_MANAGED_STEADY;
    if (current == NULL) return OBSCURA64_INVALID_ARGUMENT;
    if (!obscura64_state_validate(current)) return OBSCURA64_STATE_CORRUPT;
    if (history == NULL) {
        if (current->generation != 1) return OBSCURA64_STATE_MISSING;
        if (kind != NULL) *kind = result;
        return OBSCURA64_OK;
    }
    if (!obscura64_history_validate(history) ||
        memcmp(current->project_id, history->project_id, OBSCURA64_PROJECT_ID_SIZE) != 0)
        return OBSCURA64_STATE_CORRUPT;

    /* A history-first commit can leave entry 0 overlapping current. Its Profile
     * must match exactly; all retained generations must still be contiguous. */
    if (history->count != 0 && history->entries[0].generation == current->generation) {
        if (memcmp(history->entries[0].profile, current->profile, OBSCURA64_PROFILE_SIZE) != 0)
            return OBSCURA64_STATE_CORRUPT;
        newest = current->generation;
        result = OBSCURA64_MANAGED_OVERLAP;
    } else {
        newest = current->generation - 1U;
    }
    expected_count = newest < OBSCURA64_HISTORY_CAPACITY ?
        (uint16_t)newest : OBSCURA64_HISTORY_CAPACITY;
    if (history->count != expected_count) return OBSCURA64_STATE_CORRUPT;
    for (i = 0; i < history->count; ++i) {
        if (history->entries[i].generation != newest - i)
            return OBSCURA64_STATE_CORRUPT;
    }
    if (kind != NULL) *kind = result;
    return OBSCURA64_OK;
}
