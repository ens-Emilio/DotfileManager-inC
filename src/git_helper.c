#include "git_helper.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"

static bool run_git(const AppOptions *opts, const char *args) {
    char command[1024];
    snprintf(command, sizeof(command), "cd '%s' && git %s", opts->project_root, args);
    log_info("Executando: %s", command);
    int rc = system(command);
    if (rc != 0) {
        log_warn("Comando git falhou (%d): %s", rc, args);
        return false;
    }
    return true;
}

bool git_auto_sync(const AppOptions *opts) {
    if (!run_git(opts, "status --short")) {
        return false;
    }
    if (!run_git(opts, "add .")) {
        return false;
    }
    char commit_cmd[1024];
    snprintf(commit_cmd, sizeof(commit_cmd), "commit -m \"%s\"", opts->git_message);
    if (!run_git(opts, commit_cmd)) {
        return false;
    }
    return run_git(opts, "push");
}
