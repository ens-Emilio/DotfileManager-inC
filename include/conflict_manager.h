#ifndef DOTMGR_CONFLICT_MANAGER_H
#define DOTMGR_CONFLICT_MANAGER_H

#include "dotmgr.h"

typedef enum {
    CONFLICT_OK,
    CONFLICT_SKIP,
    CONFLICT_ERROR
} ConflictOutcome;

ConflictOutcome resolve_conflict(const AppOptions *opts, const char *target_path);

#endif
