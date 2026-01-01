#include "symlink_engine.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "conflict_manager.h"
#include "utils.h"

#ifndef _WIN32
#include <sys/stat.h>
#include <unistd.h>
typedef struct stat StatBuffer;
#define LSTAT lstat
#else
#include <windows.h>
#include <sys/stat.h>
typedef struct _stat64i32 StatBuffer;
#define LSTAT _stat64i32
#endif

static bool create_symlink(const DotfileEntry *entry, bool dry_run) {
#ifdef _WIN32
    log_info("[dry-run] (Windows) ln -s %s %s", entry->source_path, entry->target_path);
    if (dry_run) {
        return true;
    }
    log_error("Criação real de symlink indisponível no Windows nesta implementação. Execute com --dry-run ou em ambiente POSIX.");
    return false;
#else
    if (dry_run) {
        log_info("[dry-run] ln -s %s %s", entry->source_path, entry->target_path);
        return true;
    }
    if (symlink(entry->source_path, entry->target_path) != 0) {
        log_error("Falha ao criar symlink %s -> %s: %s", entry->target_path, entry->source_path, strerror(errno));
        return false;
    }
    log_info("Link criado: %s -> %s", entry->target_path, entry->source_path);
    return true;
#endif
}

static bool remove_symlink(const char *path, bool dry_run) {
#ifdef _WIN32
    log_info("[dry-run] (Windows) unlink %s", path);
    if (dry_run) {
        return true;
    }
    log_error("Remoção real de symlink indisponível no Windows nesta implementação.");
    return false;
#else
    if (dry_run) {
        log_info("[dry-run] unlink %s", path);
        return true;
    }
    if (unlink(path) != 0) {
        log_error("Falha ao remover symlink '%s': %s", path, strerror(errno));
        return false;
    }
    return true;
#endif
}

bool install_entry(const AppOptions *opts, const DotfileEntry *entry) {
    if (!opts || !entry) {
        return false;
    }
#ifdef _WIN32
    if (!opts->dry_run) {
        log_error("Install real não suportado no Windows. Utilize --dry-run ou execute em ambiente POSIX.");
        return false;
    }
#endif
    StatBuffer st;
    if (LSTAT(entry->target_path, &st) == 0) {
#ifndef _WIN32
        if (S_ISLNK(st.st_mode)) {
            if (is_same_symlink_target(entry->target_path, entry->source_path)) {
                if (opts->verbose) {
                    log_info("Symlink já atualizado: %s", entry->target_path);
                }
                return true;
            }
        }
#else
        if (opts->verbose) {
            log_warn("(Windows) destino já existe: %s", entry->target_path);
        }
#endif
        ConflictOutcome outcome = resolve_conflict(opts, entry->target_path);
        if (outcome == CONFLICT_SKIP) {
            log_warn("Pulando %s", entry->target_path);
            return true;
        }
        if (outcome == CONFLICT_ERROR) {
            return false;
        }
    }

    if (!ensure_parent_dirs(entry->target_path, opts->dry_run)) {
        return false;
    }

    return create_symlink(entry, opts->dry_run);
}

bool uninstall_entry(const AppOptions *opts, const DotfileEntry *entry) {
    if (!opts || !entry) {
        return false;
    }
#ifdef _WIN32
    if (!opts->dry_run) {
        log_error("Uninstall real não suportado no Windows. Utilize --dry-run ou ambiente POSIX.");
        return false;
    }
#endif
    StatBuffer st;
    if (LSTAT(entry->target_path, &st) != 0) {
        if (errno == ENOENT) {
            if (opts->verbose) {
                log_info("Destino inexistente: %s", entry->target_path);
            }
            return true;
        }
        log_error("Erro ao ler '%s': %s", entry->target_path, strerror(errno));
        return false;
    }
#ifndef _WIN32
    if (!S_ISLNK(st.st_mode)) {
        log_warn("Destino %s não é symlink, pulando", entry->target_path);
        return true;
    }
    if (!is_same_symlink_target(entry->target_path, entry->source_path)) {
        log_warn("Symlink %s aponta para outro target, pulando", entry->target_path);
        return true;
    }
    return remove_symlink(entry->target_path, opts->dry_run);
#else
    log_warn("(Windows) validação limitada para %s", entry->target_path);
    return remove_symlink(entry->target_path, opts->dry_run);
#endif
}

bool status_entry(const AppOptions *opts, const DotfileEntry *entry) {
    if (!opts || !entry) {
        return false;
    }
#ifdef _WIN32
    DWORD attrs = GetFileAttributesA(entry->target_path);
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        log_warn("[MISSING] %s", entry->target_path);
        return true;
    }
    if (attrs & FILE_ATTRIBUTE_REPARSE_POINT) {
        log_info("[INFO] %s é um reparse point (verificação limitada no Windows)", entry->target_path);
        return true;
    }
    log_warn("[CONFLICT] %s existe mas não é symlink (verificação limitada no Windows)", entry->target_path);
    return true;
#else
    StatBuffer st;
    if (LSTAT(entry->target_path, &st) != 0) {
        if (errno == ENOENT) {
            log_warn("[MISSING] %s", entry->target_path);
            return true;
        }
        log_error("Erro ao checar '%s': %s", entry->target_path, strerror(errno));
        return false;
    }
    if (!S_ISLNK(st.st_mode)) {
        log_warn("[CONFLICT] %s existe mas não é symlink", entry->target_path);
        return true;
    }
    if (!is_same_symlink_target(entry->target_path, entry->source_path)) {
        log_warn("[DIVERGENT] %s aponta para outro destino", entry->target_path);
        return true;
    }
    log_info("[OK] %s", entry->target_path);
    return true;
#endif
}
