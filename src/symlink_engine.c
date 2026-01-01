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
#ifndef SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE
#define SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE 0x2
#endif
#endif

static bool create_symlink(const DotfileEntry *entry, bool dry_run) {
#ifdef _WIN32
    log_info("[dry-run] (Windows) ln -s %s %s", entry->source_path, entry->target_path);
    if (dry_run) {
        return true;
    }
    wchar_t target_w[PATH_MAX];
    wchar_t link_w[PATH_MAX];
    if (!utf8_to_wide(entry->source_path, target_w, ARRAYSIZE(target_w)) ||
        !utf8_to_wide(entry->target_path, link_w, ARRAYSIZE(link_w))) {
        log_error("Falha ao converter caminhos para UTF-16 ao criar symlink");
        return false;
    }
    DWORD flags = entry->is_directory ? SYMBOLIC_LINK_FLAG_DIRECTORY : 0;
#ifdef SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE
    flags |= SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE;
#endif
    if (!CreateSymbolicLinkW(link_w, target_w, flags)) {
        DWORD err = GetLastError();
#ifdef SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE
        if ((flags & SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE) && err == ERROR_INVALID_PARAMETER) {
            flags &= ~SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE;
            if (CreateSymbolicLinkW(link_w, target_w, flags)) {
                log_info("Link criado: %s -> %s", entry->target_path, entry->source_path);
                return true;
            }
            err = GetLastError();
        }
#endif
        log_error("Falha ao criar symlink %s -> %s (erro %lu)", entry->target_path, entry->source_path, err);
        log_warn("Verifique se tem privilégio para criar symlinks ou habilite 'Developer Mode'");
        return false;
    }
    log_info("Link criado: %s -> %s", entry->target_path, entry->source_path);
    return true;
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

static bool remove_symlink(const DotfileEntry *entry, bool dry_run) {
#ifdef _WIN32
    log_info("[dry-run] (Windows) unlink %s", entry->target_path);
    if (dry_run) {
        return true;
    }
    wchar_t link_w[PATH_MAX];
    if (!utf8_to_wide(entry->target_path, link_w, ARRAYSIZE(link_w))) {
        log_error("Falha ao converter caminho para remover symlink");
        return false;
    }
    BOOL ok;
    if (entry->is_directory) {
        ok = RemoveDirectoryW(link_w);
    } else {
        ok = DeleteFileW(link_w);
    }
    if (!ok) {
        DWORD err = GetLastError();
        log_error("Falha ao remover symlink '%s' (erro %lu)", entry->target_path, err);
        return false;
    }
    return true;
#else
    if (dry_run) {
        log_info("[dry-run] unlink %s", entry->target_path);
        return true;
    }
    if (unlink(entry->target_path) != 0) {
        log_error("Falha ao remover symlink '%s': %s", entry->target_path, strerror(errno));
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
    DWORD attrs = GetFileAttributesA(entry->target_path);
    if (attrs != INVALID_FILE_ATTRIBUTES) {
        ConflictOutcome outcome = resolve_conflict(opts, entry->target_path);
        if (outcome == CONFLICT_SKIP) {
            log_warn("Pulando %s", entry->target_path);
            return true;
        }
        if (outcome == CONFLICT_ERROR) {
            return false;
        }
    }
#else
    StatBuffer st;
    if (LSTAT(entry->target_path, &st) == 0) {
        if (S_ISLNK(st.st_mode)) {
            if (is_same_symlink_target(entry->target_path, entry->source_path)) {
                if (opts->verbose) {
                    log_info("Symlink já atualizado: %s", entry->target_path);
                }
                return true;
            }
        }
        ConflictOutcome outcome = resolve_conflict(opts, entry->target_path);
        if (outcome == CONFLICT_SKIP) {
            log_warn("Pulando %s", entry->target_path);
            return true;
        }
        if (outcome == CONFLICT_ERROR) {
            return false;
        }
    }
#endif

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
    DWORD attrs = GetFileAttributesA(entry->target_path);
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        if (opts->verbose) {
            log_info("Destino inexistente: %s", entry->target_path);
        }
        return true;
    }
    char target_check[PATH_MAX];
    if (!read_symlink_target(entry->target_path, target_check, sizeof(target_check))) {
        log_warn("Destino %s não é symlink, pulando", entry->target_path);
        return true;
    }
    if (!is_same_symlink_target(entry->target_path, entry->source_path)) {
        log_warn("Symlink %s aponta para outro destino, pulando", entry->target_path);
        return true;
    }
    return remove_symlink(entry, opts->dry_run);
#else
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
    if (!S_ISLNK(st.st_mode)) {
        log_warn("Destino %s não é symlink, pulando", entry->target_path);
        return true;
    }
    if (!is_same_symlink_target(entry->target_path, entry->source_path)) {
        log_warn("Symlink %s aponta para outro target, pulando", entry->target_path);
        return true;
    }
    return remove_symlink(entry, opts->dry_run);
#endif
}

bool status_entry(const AppOptions *opts, const DotfileEntry *entry) {
    if (!opts || !entry) {
        return false;
    }
#ifdef _WIN32
    char target_buf[PATH_MAX];
    if (!read_symlink_target(entry->target_path, target_buf, sizeof(target_buf))) {
        DWORD attrs = GetFileAttributesA(entry->target_path);
        if (attrs == INVALID_FILE_ATTRIBUTES) {
            log_warn("[MISSING] %s", entry->target_path);
            return true;
        }
        log_warn("[CONFLICT] %s existe mas não é symlink", entry->target_path);
        return true;
    }
    if (!is_same_symlink_target(entry->target_path, entry->source_path)) {
        log_warn("[DIVERGENT] %s aponta para %s", entry->target_path, target_buf);
        return true;
    }
    log_info("[OK] %s", entry->target_path);
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
