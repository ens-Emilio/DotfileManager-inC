#ifndef DOTMGR_H
#define DOTMGR_H

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

typedef enum {
    CMD_INSTALL,
    CMD_UNINSTALL,
    CMD_STATUS,
    CMD_COLLECT
} CommandType;

typedef enum {
    CONFLICT_BACKUP,
    CONFLICT_INTERACTIVE,
    CONFLICT_FORCE
} ConflictMode;

typedef struct {
    char source_path[PATH_MAX];
    char target_path[PATH_MAX];
    bool is_directory;
} DotfileEntry;

typedef struct {
    DotfileEntry *entries;
    size_t count;
} DotfileConfig;

typedef struct {
    char config_path[PATH_MAX];
    char repo_path[PATH_MAX];
    char project_root[PATH_MAX];
    char machine_name[64];
    ConflictMode conflict_mode;
    bool dry_run;
    bool verbose;
    bool git_auto;
    char git_message[256];
    bool config_explicit;
    CommandType command;
} AppOptions;

#endif
