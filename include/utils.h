#ifndef DOTMGR_UTILS_H
#define DOTMGR_UTILS_H

#include <stdbool.h>
#include <stddef.h>

#include "dotmgr.h"

#define LOG_COLOR_RESET "\033[0m"
#define LOG_COLOR_INFO   "\033[32m"
#define LOG_COLOR_WARN   "\033[33m"
#define LOG_COLOR_ERROR  "\033[31m"

void log_info(const char *fmt, ...);
void log_warn(const char *fmt, ...);
void log_error(const char *fmt, ...);

bool expand_home(const char *input, char *output, size_t len);
bool join_paths(const char *base, const char *relative, char *output, size_t len);
bool is_absolute_path(const char *path);
bool ensure_parent_dirs(const char *path, bool dry_run);
bool path_exists(const char *path);
bool is_same_symlink_target(const char *link_path, const char *target);
bool read_symlink_target(const char *link_path, char *buffer, size_t len);
bool normalize_path(const char *path, char *output, size_t len);
bool get_current_directory(char *output, size_t len);
bool get_machine_name(char *output, size_t len);

#ifdef _WIN32
#include <wchar.h>
bool utf8_to_wide(const char *input, wchar_t *output, size_t len);
bool wide_to_utf8(const wchar_t *input, char *output, size_t len);
#endif

#endif
