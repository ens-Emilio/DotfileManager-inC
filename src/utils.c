#include "utils.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <windows.h>
#include <winioctl.h>
#ifndef ARRAYSIZE
#define ARRAYSIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif
typedef struct {
    DWORD ReparseTag;
    WORD ReparseDataLength;
    WORD Reserved;
    union {
        struct {
            WORD SubstituteNameOffset;
            WORD SubstituteNameLength;
            WORD PrintNameOffset;
            WORD PrintNameLength;
            ULONG Flags;
            WCHAR PathBuffer[1];
        } SymbolicLinkReparseBuffer;
        struct {
            WORD SubstituteNameOffset;
            WORD SubstituteNameLength;
            WORD PrintNameOffset;
            WORD PrintNameLength;
            WCHAR PathBuffer[1];
        } MountPointReparseBuffer;
    } ReparseBuffer;
} REPARSE_DATA_BUFFER_LOCAL;

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
#define MAX_REPARSE_DATA_BUFFER  (16 * 1024)
    wchar_t wpath[PATH_MAX];
    if (!utf8_to_wide(link_path, wpath, ARRAYSIZE(wpath))) {
        return false;
    }

    DWORD attrs = GetFileAttributesW(wpath);
    DWORD flags = FILE_FLAG_OPEN_REPARSE_POINT;
    if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        flags |= FILE_FLAG_BACKUP_SEMANTICS;
    }

    HANDLE handle = CreateFileW(
        wpath,
        0,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        flags,
        NULL);
    if (handle == INVALID_HANDLE_VALUE) {
        return false;
    }

    BYTE raw_buffer[MAX_REPARSE_DATA_BUFFER];
    DWORD bytes_returned = 0;
    BOOL ok = DeviceIoControl(
        handle,
        FSCTL_GET_REPARSE_POINT,
        NULL,
        0,
        raw_buffer,
        sizeof(raw_buffer),
        &bytes_returned,
        NULL);
    CloseHandle(handle);
    if (!ok) {
        return false;
    }

    REPARSE_DATA_BUFFER_LOCAL *reparse = (REPARSE_DATA_BUFFER_LOCAL *)raw_buffer;
    const WCHAR *start = NULL;
    size_t wchar_len = 0;

    if (reparse->ReparseTag == IO_REPARSE_TAG_SYMLINK) {
        start = reparse->ReparseBuffer.SymbolicLinkReparseBuffer.PathBuffer +
            (reparse->ReparseBuffer.SymbolicLinkReparseBuffer.SubstituteNameOffset / sizeof(WCHAR));
        wchar_len = reparse->ReparseBuffer.SymbolicLinkReparseBuffer.SubstituteNameLength / sizeof(WCHAR);
    } else if (reparse->ReparseTag == IO_REPARSE_TAG_MOUNT_POINT) {
        start = reparse->ReparseBuffer.MountPointReparseBuffer.PathBuffer +
            (reparse->ReparseBuffer.MountPointReparseBuffer.SubstituteNameOffset / sizeof(WCHAR));
        wchar_len = reparse->ReparseBuffer.MountPointReparseBuffer.SubstituteNameLength / sizeof(WCHAR);
    } else {
        return false;
    }

    if (!start || wchar_len == 0) {
        return false;
    }

    wchar_t target_w[PATH_MAX];
    if (wchar_len >= ARRAYSIZE(target_w)) {
        wchar_len = ARRAYSIZE(target_w) - 1;
    }
    wcsncpy(target_w, start, wchar_len);
    target_w[wchar_len] = L'\0';

    /* Remover prefixos \\?\ ou \??\ */
    if (wcsncmp(target_w, L"\\??\\", 4) == 0) {
        memmove(target_w, target_w + 4, (wcslen(target_w + 4) + 1) * sizeof(wchar_t));
    } else if (wcsncmp(target_w, L"\\\\?\\", 4) == 0) {
        if (wcsncmp(target_w + 4, L"UNC\\", 4) == 0) {
            target_w[0] = L'\\';
            target_w[1] = L'\\';
            wcscpy(target_w + 2, target_w + 8);
        } else {
            memmove(target_w, target_w + 4, (wcslen(target_w + 4) + 1) * sizeof(wchar_t));
        }
    }

    if (!buffer || len == 0) {
        return true;
    }

    if (!wide_to_utf8(target_w, buffer, len)) {
        return false;
    }
    return true;
#endif
}

#ifdef _WIN32
bool get_current_directory(char *output, size_t len) {
    if (!output || len == 0) {
        return false;
    }
    if (_getcwd(output, (int)len) == NULL) {
        return false;
    }
    return true;
}

bool get_machine_name(char *output, size_t len) {
    if (!output || len == 0) {
        return false;
    }
    DWORD size = (DWORD)len;
    if (!GetComputerNameA(output, &size)) {
        return false;
    }
    return true;
}

bool utf8_to_wide(const char *input, wchar_t *output, size_t len) {
    if (!input || !output || len == 0) {
        return false;
    }
    int needed = MultiByteToWideChar(CP_UTF8, 0, input, -1, NULL, 0);
    if (needed <= 0 || (size_t)needed > len) {
        return false;
    }
    int written = MultiByteToWideChar(CP_UTF8, 0, input, -1, output, (int)len);
    return written > 0;
}

bool wide_to_utf8(const wchar_t *input, char *output, size_t len) {
    if (!input || !output || len == 0) {
        return false;
    }
    int needed = WideCharToMultiByte(CP_UTF8, 0, input, -1, NULL, 0, NULL, NULL);
    if (needed <= 0 || (size_t)needed > len) {
        return false;
    }
    int written = WideCharToMultiByte(CP_UTF8, 0, input, -1, output, (int)len, NULL, NULL);
    return written > 0;
}
#endif

#ifndef _WIN32
bool get_current_directory(char *output, size_t len) {
    if (!output || len == 0) {
        return false;
    }
    if (!getcwd(output, len)) {
        return false;
    }
    return true;
}

bool get_machine_name(char *output, size_t len) {
    if (!output || len == 0) {
        return false;
    }
    if (gethostname(output, len) != 0) {
        return false;
    }
    output[len - 1] = '\0';
    return true;
}
#endif

bool is_same_symlink_target(const char *link_path, const char *target) {
#ifndef _WIN32
    char buffer[PATH_MAX];
    if (!read_symlink_target(link_path, buffer, sizeof(buffer))) {
        return false;
    }
    return strcmp(buffer, target) == 0;
#else
    char actual[PATH_MAX];
    if (!read_symlink_target(link_path, actual, sizeof(actual))) {
        return false;
    }
    char actual_norm[PATH_MAX];
    char target_norm[PATH_MAX];
    if (!normalize_path(actual, actual_norm, sizeof(actual_norm))) {
        strncpy(actual_norm, actual, sizeof(actual_norm) - 1);
        actual_norm[sizeof(actual_norm) - 1] = '\0';
    }
    if (!normalize_path(target, target_norm, sizeof(target_norm))) {
        strncpy(target_norm, target, sizeof(target_norm) - 1);
        target_norm[sizeof(target_norm) - 1] = '\0';
    }
    return _stricmp(actual_norm, target_norm) == 0;
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
