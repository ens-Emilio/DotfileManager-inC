#include "conflict_manager.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "utils.h"

#ifndef _WIN32
#include <sys/stat.h>
#include <unistd.h>
#else
#include <direct.h>
#include <io.h>
#include <sys/stat.h>
#define lstat _stat64i32
#define S_ISLNK(mode) 0
#endif

#ifdef _WIN32
typedef struct _stat64i32 StatBuffer;
#else
typedef struct stat StatBuffer;
#endif

static bool remove_path(const char *path, bool dry_run) {
    if (dry_run) {
        log_info("[dry-run] remover %s", path);
        return true;
    }
#ifndef _WIN32
    if (unlink(path) == 0) {
        return true;
    }
    if (errno == EPERM || errno == EISDIR) {
        if (rmdir(path) == 0) {
            return true;
        }
    }
#else
    if (remove(path) == 0) {
        return true;
    }
    if (_rmdir(path) == 0) {
        return true;
    }
#endif
    log_error("Falha ao remover '%s': %s", path, strerror(errno));
    return false;
}

static bool backup_path(const char *path, bool dry_run) {
    char backup_path[PATH_MAX];
    if (snprintf(backup_path, sizeof(backup_path), "%s.bak", path) >= (int)sizeof(backup_path)) {
        log_error("Caminho de backup muito longo para '%s'", path);
        return false;
    }
    if (path_exists(backup_path)) {
        log_warn("Backup já existe para '%s', pulando", path);
        return false;
    }
    if (dry_run) {
        log_info("[dry-run] mover %s -> %s", path, backup_path);
        return true;
    }
    if (rename(path, backup_path) != 0) {
        log_error("Não foi possível criar backup de '%s': %s", path, strerror(errno));
        return false;
    }
    log_info("Backup criado: %s", backup_path);
    return true;
}

static ConflictOutcome prompt_user(const char *path) {
    printf("Encontrado conflito em '%s'. (b)ackup, (s)obrescrever, (p)ular? ", path);
    fflush(stdout);
    char answer[8];
    if (!fgets(answer, sizeof(answer), stdin)) {
        return CONFLICT_ERROR;
    }
    switch (answer[0]) {
        case 'b':
        case 'B':
            return CONFLICT_OK;
        case 's':
        case 'S':
            return CONFLICT_OK;
        case 'p':
        case 'P':
            return CONFLICT_SKIP;
        default:
            printf("Opção inválida. Pulando entrada.\n");
            return CONFLICT_SKIP;
    }
}

ConflictOutcome resolve_conflict(const AppOptions *opts, const char *target_path) {
    if (!opts || !target_path) {
        return CONFLICT_ERROR;
    }
    StatBuffer st;
    if (lstat(target_path, &st) != 0) {
        if (errno == ENOENT) {
            return CONFLICT_OK;
        }
        log_error("Erro ao verificar '%s': %s", target_path, strerror(errno));
        return CONFLICT_ERROR;
    }

    switch (opts->conflict_mode) {
        case CONFLICT_BACKUP:
            if (backup_path(target_path, opts->dry_run)) {
                return CONFLICT_OK;
            }
            return CONFLICT_ERROR;
        case CONFLICT_FORCE:
            if (remove_path(target_path, opts->dry_run)) {
                return CONFLICT_OK;
            }
            return CONFLICT_ERROR;
        case CONFLICT_INTERACTIVE: {
            ConflictOutcome choice = prompt_user(target_path);
            if (choice == CONFLICT_SKIP) {
                return CONFLICT_SKIP;
            }
            if (choice == CONFLICT_OK) {
                printf("Executando estratégia default (backup).\n");
                if (backup_path(target_path, opts->dry_run)) {
                    return CONFLICT_OK;
                }
                if (remove_path(target_path, opts->dry_run)) {
                    return CONFLICT_OK;
                }
            }
            return CONFLICT_ERROR;
        }
        default:
            log_error("Modo de conflito desconhecido");
            return CONFLICT_ERROR;
    }
}
