#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "obscura64_internal.h"
#include <stdlib.h>
#include <string.h>

static obscura64_status verify_current(const unsigned char *bytes, size_t n, void *out)
{
    obscura64_project_state s;
    if (obscura64_state_deserialize(bytes, n, &s) != OBSCURA64_CORE_STATUS_SUCCESS) return OBSCURA64_STATE_CORRUPT;
    if (out != NULL) *(obscura64_project_state *)out = s;
    return OBSCURA64_OK;
}
static obscura64_status verify_history(const unsigned char *bytes, size_t n, void *out)
{
    obscura64_project_history h;
    if (obscura64_history_deserialize(bytes, n, &h) != OBSCURA64_CORE_STATUS_SUCCESS) return OBSCURA64_STATE_CORRUPT;
    if (out != NULL) *(obscura64_project_history *)out = h;
    return OBSCURA64_OK;
}
obscura64_status obscura64_snapshot_read_current(const wchar_t *p, obscura64_project_state *s)
{
    unsigned char bytes[OBSCURA64_STATE_V1_SIZE];
    return obscura64_persistence_read_verified(p, bytes, sizeof(bytes), verify_current, s);
}
obscura64_status obscura64_snapshot_read_history(const wchar_t *p, obscura64_project_history *h)
{
    unsigned char bytes[OBSCURA64_HISTORY_V1_SIZE];
    return obscura64_persistence_read_verified(p, bytes, sizeof(bytes), verify_history, h);
}
static obscura64_status write_snapshot(const wchar_t *path, const unsigned char *bytes,
    size_t n, obscura64_persistence_verify_fn verify)
{
    DWORD attr = GetFileAttributesW(path);
    unsigned char previous[OBSCURA64_HISTORY_V1_SIZE];
    obscura64_status status;
    if (attr == INVALID_FILE_ATTRIBUTES) {
        DWORD error = GetLastError();
        if (error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND) return OBSCURA64_IO_ERROR;
        return obscura64_persistence_create_new_verified(path, bytes, n, verify, NULL);
    }
    status = obscura64_persistence_read_verified(path, previous, n, verify, NULL);
    /* Replacing corrupt evidence is allowed only after recovery chose a validated
       candidate. No parser failure can manufacture a candidate here. */
    if (status != OBSCURA64_OK && status != OBSCURA64_STATE_CORRUPT) return status;
    return obscura64_persistence_replace_verified(path, bytes, n, verify, NULL,
        status == OBSCURA64_OK ? previous : NULL);
}
obscura64_status obscura64_snapshot_write_current(const wchar_t *p, const obscura64_project_state *s)
{
    unsigned char bytes[OBSCURA64_STATE_V1_SIZE];
    if (obscura64_state_serialize(s, bytes) != OBSCURA64_CORE_STATUS_SUCCESS) return OBSCURA64_STATE_CORRUPT;
    return write_snapshot(p, bytes, sizeof(bytes), verify_current);
}
obscura64_status obscura64_snapshot_write_history(const wchar_t *p, const obscura64_project_history *h)
{
    unsigned char bytes[OBSCURA64_HISTORY_V1_SIZE];
    if (obscura64_history_serialize(h, bytes) != OBSCURA64_CORE_STATUS_SUCCESS) return OBSCURA64_STATE_CORRUPT;
    return write_snapshot(p, bytes, sizeof(bytes), verify_history);
}

static wchar_t *file_path(const wchar_t *dir, const wchar_t *file)
{
    size_t a = wcslen(dir), b = wcslen(file);
    wchar_t *p;
    if (a > SIZE_MAX / sizeof(*p) - b - 2U) return NULL;
    p = (wchar_t *)malloc((a+b+2U)*sizeof(*p));
    if (p != NULL) { wcscpy(p, dir); wcscat(p, L"\\"); wcscat(p, file); }
    return p;
}
static int usable_error(obscura64_status s)
{ return s == OBSCURA64_STATE_MISSING || s == OBSCURA64_STATE_CORRUPT; }
static obscura64_status no_source(obscura64_status current, obscura64_status history)
{
    if (current == OBSCURA64_OUT_OF_MEMORY || history == OBSCURA64_OUT_OF_MEMORY) return OBSCURA64_OUT_OF_MEMORY;
    if (current == OBSCURA64_IO_ERROR || history == OBSCURA64_IO_ERROR) return OBSCURA64_IO_ERROR;
    return OBSCURA64_UNRECOVERABLE;
}
static int same_id(const unsigned char *a, const unsigned char *b)
{ return memcmp(a, b, OBSCURA64_PROJECT_ID_SIZE) == 0; }
static int same_current(const obscura64_project_state *a, const obscura64_project_state *b)
{
    return same_id(a->project_id,b->project_id) && a->generation == b->generation &&
        memcmp(a->profile,b->profile,OBSCURA64_PROFILE_SIZE) == 0;
}
static int promoted(const obscura64_project_history *h, obscura64_project_state *s)
{
    if (h->count == 0) return 0;
    memset(s, 0, sizeof(*s)); memcpy(s->project_id, h->project_id, 16);
    s->generation = h->entries[0].generation; memcpy(s->profile, h->entries[0].profile, 64);
    return obscura64_managed_state_validate(s,h,NULL) == OBSCURA64_OK;
}
static int repair_history(const obscura64_project_state *main,
    const obscura64_project_state *backup, const obscura64_project_history *old,
    obscura64_project_history *result)
{
    size_t i;
    obscura64_managed_state_kind kind;
    if (!same_id(main->project_id,backup->project_id) ||
        obscura64_managed_state_validate(backup,old,&kind) != OBSCURA64_OK) return 0;
    if (same_current(main,backup)) { *result = *old; return 1; }
    if (main->generation <= 1 || backup->generation != main->generation-1U) return 0;
    memset(result,0,sizeof(*result)); memcpy(result->project_id,main->project_id,16);
    result->count = 1; result->entries[0].generation = backup->generation;
    memcpy(result->entries[0].profile,backup->profile,64);
    for (i = kind == OBSCURA64_MANAGED_OVERLAP ? 1U : 0U; i < old->count && result->count < 3; ++i)
        result->entries[result->count++] = old->entries[i];
    return obscura64_managed_state_validate(main,result,NULL) == OBSCURA64_OK;
}

obscura64_status obscura64_recovery_ensure(const wchar_t *project, const wchar_t *directory,
    int require_managed, obscura64_project_state *out)
{
    wchar_t *cp = file_path(directory,L"current.state"), *hp = file_path(directory,L"history.state");
    obscura64_snapshot_paths paths = {0};
    obscura64_project_state main, backup, candidate;
    obscura64_project_history local, mirror, chosen;
    obscura64_status cs, hs, bs = OBSCURA64_STATE_MISSING, bh = OBSCURA64_STATE_MISSING, status;
    int write_history = 0, has_chosen = 0;
    if (cp == NULL || hp == NULL) { status = OBSCURA64_OUT_OF_MEMORY; goto done; }
    cs = obscura64_snapshot_read_current(cp,&main);
    if (cs != OBSCURA64_OK && !usable_error(cs)) { status = cs; goto done; }
    hs = obscura64_snapshot_read_history(hp,&local);
    /* Ordinary primary filesystem errors are never mislabeled UNRECOVERABLE. */
    if (hs != OBSCURA64_OK && !usable_error(hs)) {
        status = cs == OBSCURA64_OK && !require_managed ? OBSCURA64_OK : hs;
        if (status == OBSCURA64_OK) *out = main;
        goto done;
    }
    if (cs == OBSCURA64_OK && obscura64_managed_state_validate(&main,
        hs == OBSCURA64_OK ? &local : NULL,NULL) == OBSCURA64_OK) {
        *out = main; status = OBSCURA64_OK; goto done;
    }
    if (obscura64_redundancy_paths(project,0,&paths) == OBSCURA64_OK) {
        bs = obscura64_snapshot_read_current(paths.current,&backup);
        bh = obscura64_snapshot_read_history(paths.history,&mirror);
    }
    if (cs == OBSCURA64_OK) {
        /* Valid main is immutable authority. Only a validated matching backup
           snapshot can reconstruct its missing/noncanonical history. */
        if (bs == OBSCURA64_OK && bh == OBSCURA64_OK && repair_history(&main,&backup,&mirror,&chosen)) {
            status = obscura64_snapshot_write_history(hp,&chosen);
            if (status == OBSCURA64_OK) {
                status = obscura64_snapshot_read_history(hp,&local);
                if (status == OBSCURA64_OK) status = obscura64_managed_state_validate(&main,&local,NULL);
            }
            if (status != OBSCURA64_OK && require_managed) goto done;
        } else if (require_managed) { status = no_source(bs,bh); goto done; }
        *out = main; status = OBSCURA64_OK; goto done;
    }

    /* Structurally valid project history anchors identity, even if its window
       cannot be promoted. Otherwise incompatible external identities are ambiguous. */
    if (hs == OBSCURA64_OK) {
        if (bs == OBSCURA64_OK && !same_id(local.project_id,backup.project_id)) bs = OBSCURA64_STATE_CORRUPT;
        if (bh == OBSCURA64_OK && !same_id(local.project_id,mirror.project_id)) bh = OBSCURA64_STATE_CORRUPT;
    } else if (bs == OBSCURA64_OK && bh == OBSCURA64_OK && !same_id(backup.project_id,mirror.project_id)) {
        status = OBSCURA64_UNRECOVERABLE; goto done;
    }
    if (bs == OBSCURA64_OK) {
        candidate = backup;
        if (hs == OBSCURA64_OK && obscura64_managed_state_validate(&candidate,&local,NULL) == OBSCURA64_OK) {
            chosen = local; has_chosen = 1;
        } else if (bh == OBSCURA64_OK && obscura64_managed_state_validate(&candidate,&mirror,NULL) == OBSCURA64_OK) {
            chosen = mirror; has_chosen = 1; write_history = 1;
        } else if (candidate.generation == 1 && hs == OBSCURA64_STATE_MISSING) {
            /* A generation-1 project need not have a local history. */
        } else if (require_managed) { status = no_source(bs,bh); goto done; }
        /* Without compatible history, exact current is still sufficient for
           normal runtime. Force must reject this partial recovery. */
    } else if (hs == OBSCURA64_OK && promoted(&local,&candidate)) {
        chosen = local; has_chosen = 1; /* Keep project history byte-for-byte. */
    } else if (bh == OBSCURA64_OK && promoted(&mirror,&candidate)) {
        chosen = mirror; has_chosen = 1; write_history = 1;
    } else { status = no_source(bs,bh); goto done; }

    if (has_chosen && obscura64_managed_state_validate(&candidate,&chosen,NULL) != OBSCURA64_OK) {
        status = OBSCURA64_UNRECOVERABLE; goto done;
    }
    if (write_history) {
        status = obscura64_snapshot_write_history(hp,&chosen);
        if (status != OBSCURA64_OK) goto done;
    }
    status = obscura64_snapshot_write_current(cp,&candidate);
    if (status != OBSCURA64_OK) goto done;
    status = obscura64_snapshot_read_current(cp,&main);
    if (status != OBSCURA64_OK || !same_current(&main,&candidate)) { status = OBSCURA64_STATE_CORRUPT; goto done; }
    if (has_chosen) {
        status = obscura64_snapshot_read_history(hp,&local);
        if (status == OBSCURA64_OK) status = obscura64_managed_state_validate(&main,&local,NULL);
        if (status != OBSCURA64_OK) goto done;
    }
    *out = main; status = OBSCURA64_OK;
done:
    obscura64_snapshot_paths_free(&paths); free(cp); free(hp);
    return status;
}

void obscura64_recovery_maintain(const wchar_t *project, const wchar_t *directory)
{
    obscura64_project_state s;
    obscura64_project_history h;
    wchar_t *hp;
    obscura64_status hs;
    if (obscura64_recovery_ensure(project,directory,0,&s) != OBSCURA64_OK) return;
    hp = file_path(directory,L"history.state");
    if (hp == NULL) return;
    hs = obscura64_snapshot_read_history(hp,&h);
    if (hs == OBSCURA64_OK || (hs == OBSCURA64_STATE_MISSING && s.generation == 1))
        (void)obscura64_redundancy_sync(project,&s,hs == OBSCURA64_OK ? &h : NULL);
    free(hp);
}
