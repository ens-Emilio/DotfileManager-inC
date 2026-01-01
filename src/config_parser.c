#include "config_parser.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"

static char *trim_whitespace(char *str) {
    if (!str) {
        return str;
    }
    while (*str && isspace((unsigned char)*str)) {
        ++str;
    }
    if (*str == '\0') {
        return str;
    }
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) {
        *end-- = '\0';
    }
    return str;
}

static bool expand_target(const char *raw, char *output, size_t len) {
    if (!raw || !output) {
        return false;
    }
    if (raw[0] == '~') {
        return expand_home(raw, output, len);
    }
    if (is_absolute_path(raw)) {
        return snprintf(output, len, "%s", raw) < (int)len;
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
    return join_paths(home, raw, output, len);
}

static bool detect_directory(const char *path) {
    if (!path || !*path) {
        return false;
    }
    size_t len = strlen(path);
    if (len == 0) {
        return false;
    }
    return path[len - 1] == '/' || path[len - 1] == '\\';
}

bool load_config(const AppOptions *opts, DotfileConfig *config) {
    if (!opts || !config) {
        return false;
    }
    memset(config, 0, sizeof(*config));

    FILE *fp = fopen(opts->config_path, "r");
    if (!fp) {
        log_error("Não foi possível abrir config '%s': %s", opts->config_path, strerror(errno));
        return false;
    }

    size_t capacity = 8;
    config->entries = calloc(capacity, sizeof(DotfileEntry));
    if (!config->entries) {
        fclose(fp);
        return false;
    }

    char line[1024];
    size_t line_number = 0;
    while (fgets(line, sizeof(line), fp)) {
        ++line_number;
        char *hash = strchr(line, '#');
        if (hash) {
            *hash = '\0';
        }
        char *trimmed = trim_whitespace(line);
        if (*trimmed == '\0') {
            continue;
        }
        char *arrow = strstr(trimmed, "->");
        if (!arrow) {
            log_warn("Config linha %zu inválida: '%s'", line_number, trimmed);
            continue;
        }
        *arrow = '\0';
        char *source_raw = trim_whitespace(trimmed);
        char *target_raw = trim_whitespace(arrow + 2);
        if (*source_raw == '\0' || *target_raw == '\0') {
            log_warn("Config linha %zu incompleta", line_number);
            continue;
        }

        DotfileEntry entry;
        memset(&entry, 0, sizeof(entry));

        char joined[PATH_MAX];
        if (!join_paths(opts->repo_path, source_raw, joined, sizeof(joined))) {
            log_error("Caminho de origem muito longo em linha %zu", line_number);
            continue;
        }
        if (!normalize_path(joined, entry.source_path, sizeof(entry.source_path))) {
            strncpy(entry.source_path, joined, sizeof(entry.source_path) - 1);
        }

        char target_buffer[PATH_MAX];
        if (!expand_target(target_raw, target_buffer, sizeof(target_buffer))) {
            log_error("Não foi possível resolver destino na linha %zu", line_number);
            continue;
        }
        if (!normalize_path(target_buffer, entry.target_path, sizeof(entry.target_path))) {
            strncpy(entry.target_path, target_buffer, sizeof(entry.target_path) - 1);
        }

        entry.is_directory = detect_directory(source_raw) || detect_directory(target_raw);

        if (config->count == capacity) {
            capacity *= 2;
            DotfileEntry *tmp = realloc(config->entries, capacity * sizeof(DotfileEntry));
            if (!tmp) {
                log_error("Memória insuficiente ao carregar config");
                free_config(config);
                fclose(fp);
                return false;
            }
            config->entries = tmp;
        }

        config->entries[config->count++] = entry;
    }

    fclose(fp);
    if (config->count == 0) {
        log_warn("Nenhuma entrada carregada do arquivo de configuração");
    }
    return true;
}

void free_config(DotfileConfig *config) {
    if (!config) {
        return;
    }
    free(config->entries);
    config->entries = NULL;
    config->count = 0;
}
