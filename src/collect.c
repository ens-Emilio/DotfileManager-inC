#include "collect.h"

#include "symlink_engine.h"
#include "utils.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#ifndef _WIN32
#include <dirent.h>
#include <unistd.h>
#else
#include <direct.h>
#include <io.h>
#endif

static bool copy_entry_recursive(const AppOptions *opts, const char *src, const char *dst);

static bool ensure_directory(const AppOptions *opts, const char *path) {
    if (path_exists(path)) {
        return true;
    }
    if (opts->dry_run) {
        log_info("[dry-run] mkdir %s", path);
        return true;
    }
#ifndef _WIN32
    if (mkdir(path, 0755) == 0) {
        return true;
    }
#else
    if (_mkdir(path) == 0) {
        return true;
    }
#endif
    if (errno == EEXIST) {
        return true;
    }
    log_error("Não foi possível criar diretório '%s': %s", path, strerror(errno));
    return false;
}

static bool copy_file_contents(const AppOptions *opts, const char *src, const char *dst) {
    if (opts->dry_run) {
        log_info("[dry-run] cp %s %s", src, dst);
        return true;
    }

    FILE *in = fopen(src, "rb");
    if (!in) {
        log_error("Não foi possível abrir '%s' para leitura: %s", src, strerror(errno));
        return false;
    }

    if (!ensure_parent_dirs(dst, false)) {
        fclose(in);
        return false;
    }

    FILE *out = fopen(dst, "wb");
    if (!out) {
        log_error("Não foi possível abrir '%s' para escrita: %s", dst, strerror(errno));
        fclose(in);
        return false;
    }

    char buffer[8192];
    size_t read_bytes;
    bool ok = true;
    while ((read_bytes = fread(buffer, 1, sizeof(buffer), in)) > 0) {
        if (fwrite(buffer, 1, read_bytes, out) != read_bytes) {
            log_error("Erro ao escrever em '%s': %s", dst, strerror(errno));
            ok = false;
            break;
        }
    }

    if (ferror(in)) {
        log_error("Erro ao ler '%s': %s", src, strerror(errno));
        ok = false;
    }

    fclose(out);
    fclose(in);
    return ok;
}

#ifdef _WIN32
static bool copy_directory_win(const AppOptions *opts, const char *src, const char *dst) {
    if (!ensure_directory(opts, dst)) {
        return false;
    }
    char pattern[PATH_MAX];
    snprintf(pattern, sizeof(pattern), "%s\\*.*", src);
    struct _finddata_t data;
    intptr_t handle = _findfirst(pattern, &data);
    if (handle == -1) {
        log_error("Não foi possível listar '%s'", src);
        return false;
    }
    bool ok = true;
    do {
        if (strcmp(data.name, ".") == 0 || strcmp(data.name, "..") == 0) {
            continue;
        }
        char src_child[PATH_MAX];
        char dst_child[PATH_MAX];
        snprintf(src_child, sizeof(src_child), "%s\\%s", src, data.name);
        snprintf(dst_child, sizeof(dst_child), "%s\\%s", dst, data.name);
        if (!copy_entry_recursive(opts, src_child, dst_child)) {
            ok = false;
            break;
        }
    } while (_findnext(handle, &data) == 0);
    _findclose(handle);
    return ok;
}
#else
static bool copy_directory_posix(const AppOptions *opts, const char *src, const char *dst) {
    if (!ensure_directory(opts, dst)) {
        return false;
    }
    DIR *dir = opendir(src);
    if (!dir) {
        log_error("Não foi possível abrir diretório '%s': %s", src, strerror(errno));
        return false;
    }
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        char src_child[PATH_MAX];
        char dst_child[PATH_MAX];
        snprintf(src_child, sizeof(src_child), "%s/%s", src, entry->d_name);
        snprintf(dst_child, sizeof(dst_child), "%s/%s", dst, entry->d_name);
        if (!copy_entry_recursive(opts, src_child, dst_child)) {
            closedir(dir);
            return false;
        }
    }
    closedir(dir);
    return true;
}
#endif

static bool copy_entry_recursive(const AppOptions *opts, const char *src, const char *dst) {
#ifdef _WIN32
    struct _stat64i32 st;
    if (_stat(src, &st) != 0) {
        log_error("Não foi possível acessar '%s': %s", src, strerror(errno));
        return false;
    }
    bool is_dir = (st.st_mode & _S_IFDIR) != 0;
#else
    struct stat st;
    if (stat(src, &st) != 0) {
        log_error("Não foi possível acessar '%s': %s", src, strerror(errno));
        return false;
    }
    bool is_dir = S_ISDIR(st.st_mode);
#endif

    if (is_dir) {
#ifdef _WIN32
        return copy_directory_win(opts, src, dst);
#else
        return copy_directory_posix(opts, src, dst);
#endif
    }

    return copy_file_contents(opts, src, dst);
}

bool collect_entry(const AppOptions *opts, const DotfileEntry *entry) {
    if (!path_exists(entry->target_path)) {
        log_warn("Destino ausente ao coletar: %s", entry->target_path);
        return true;
    }

    if (!copy_entry_recursive(opts, entry->target_path, entry->source_path)) {
        return false;
    }

    return install_entry(opts, entry);
}
