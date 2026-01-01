#include "utils.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#define ACCESS _access
#define MKDIR(path) _mkdir(path)
#define PATH_SEP '\\'
#else
#include <libgen.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#define ACCESS access
#define MKDIR(path) mkdir(path, 0755)
#define PATH_SEP '/'
#endif

static void vlog_with_color(const char *color, const char *fmt, va_list args) {
    if (color) {
        fprintf(stderr, "%s", color);
    }
    vfprintf(stderr, fmt, args);
    if (color) {
        fprintf(stderr, "%s", LOG_COLOR_RESET);
    }
    fprintf(stderr, "\n");
}

void log_info(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vlog_with_color(LOG_COLOR_INFO, fmt, args);
    va_end(args);
}

void log_warn(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vlog_with_color(LOG_COLOR_WARN, fmt, args);
    va_end(args);
}

void log_error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vlog_with_color(LOG_COLOR_ERROR, fmt, args);
    va_end(args);
}

bool expand_home(const char *input, char *output, size_t len) {
    if (!input || !output) {
        return false;
    }
    if (input[0] != '~') {
        return snprintf(output, len, "%s", input) < (int)len;
    }
    const char *home = getenv("HOME");
#ifdef _WIN32
    if (!home) {
        home = getenv("USERPROFILE");
    }
#endif
    if (!home) {
        return false;
    }
    if (input[1] == '\0') {
        return snprintf(output, len, "%s", home) < (int)len;
    }
    if (input[1] == '/' || input[1] == '\\') {
        return snprintf(output, len, "%s%c%s", home, PATH_SEP, input + 2) < (int)len;
    }
    return false;
}

bool is_absolute_path(const char *path) {
    if (!path || !path[0]) {
        return false;
    }
#ifdef _WIN32
    if ((strlen(path) > 2) && path[1] == ':' && (path[2] == '/' || path[2] == '\\')) {
        return true;
    }
    return path[0] == '/' || path[0] == '\\';
#else
    return path[0] == '/';
#endif
}

bool join_paths(const char *base, const char *relative, char *output, size_t len) {
    if (!relative || !output) {
        return false;
    }
    if (is_absolute_path(relative)) {
        return snprintf(output, len, "%s", relative) < (int)len;
    }
    if (!base) {
        base = ".";
    }
    size_t base_len = strlen(base);
    bool separator_needed = base_len > 0 && base[base_len - 1] != '/' && base[base_len - 1] != '\\';
    if (separator_needed) {
        return snprintf(output, len, "%s%c%s", base, PATH_SEP, relative) < (int)len;
    }
    return snprintf(output, len, "%s%s", base, relative) < (int)len;
}

bool path_exists(const char *path) {
    if (!path) {
        return false;
    }
    return ACCESS(path, 0) == 0;
}

bool read_symlink_target(const char *link_path, char *buffer, size_t len) {
#ifndef _WIN32
    ssize_t read_len = readlink(link_path, buffer, len - 1);
    if (read_len == -1) {
        return false;
    }
    buffer[read_len] = '\0';
    return true;
#else
    (void)link_path;
    (void)buffer;
    (void)len;
    return false;
#endif
}

bool is_same_symlink_target(const char *link_path, const char *target) {
#ifndef _WIN32
    char buffer[PATH_MAX];
    if (!read_symlink_target(link_path, buffer, sizeof(buffer))) {
        return false;
    }
    return strcmp(buffer, target) == 0;
#else
    (void)link_path;
    (void)target;
    return false;
#endif
}

static bool create_dir_if_missing(const char *path) {
    if (path_exists(path)) {
        return true;
    }
    if (MKDIR(path) == 0) {
        return true;
    }
    if (errno == EEXIST) {
        return true;
    }
    log_error("Não foi possível criar diretório '%s': %s", path, strerror(errno));
    return false;
}

bool ensure_parent_dirs(const char *path, bool dry_run) {
    if (!path) {
        return false;
    }
    char buffer[PATH_MAX];
    if (strlen(path) >= sizeof(buffer)) {
        return false;
    }
    strcpy(buffer, path);
    for (size_t i = 1; buffer[i]; ++i) {
        if (buffer[i] == '/' || buffer[i] == '\\') {
            char saved = buffer[i];
            buffer[i] = '\0';
            if (!path_exists(buffer)) {
                if (dry_run) {
                    log_info("[dry-run] mkdir %s", buffer);
                } else if (!create_dir_if_missing(buffer)) {
                    return false;
                }
            }
            buffer[i] = saved;
        }
    }
    return true;
}

bool normalize_path(const char *path, char *output, size_t len) {
    if (!path || !output) {
        return false;
    }
#ifdef _WIN32
    if (_fullpath(output, path, len) == NULL) {
        return false;
    }
    return true;
#else
    if (!realpath(path, output)) {
        return false;
    }
    (void)len;
    return true;
#endif
}
