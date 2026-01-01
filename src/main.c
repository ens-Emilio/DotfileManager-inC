#include "collect.h"
#include "config_parser.h"
#include "git_helper.h"
#include "symlink_engine.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_usage(const char *prog) {
    printf("Uso: %s <comando> [opções]\n", prog);
    printf("Comandos:\n");
    printf("  install     Criar symlinks seguindo o arquivo de configuração\n");
    printf("  uninstall   Remover symlinks criados\n");
    printf("  status      Mostrar situação dos symlinks\n");
    printf("  collect     Copiar arquivos do sistema para o repositório antes de linkar\n");
    printf("Opções:\n");
    printf("  --config <arquivo>   Caminho para arquivo de configuração (default configs/dotfiles.conf)\n");
    printf("  --repo <dir>         Diretório raiz do repositório de dotfiles (default dotfiles_repo)\n");
    printf("  --machine <nome>     Força uso de configuração específica da máquina\n");
    printf("  --mode <backup|force|interactive>  Estratégia de conflito (default backup)\n");
    printf("  --dry-run            Apenas simula operações\n");
    printf("  --verbose            Saída detalhada\n");
    printf("  --git-auto           Executa git add/commit após operações\n");
    printf("  --git-message <msg>  Mensagem para git commit (com --git-auto)\n");
}

static bool parse_command(const char *value, CommandType *cmd) {
    if (strcmp(value, "install") == 0) {
        *cmd = CMD_INSTALL;
        return true;
    }
    if (strcmp(value, "uninstall") == 0) {
        *cmd = CMD_UNINSTALL;
        return true;
    }
    if (strcmp(value, "status") == 0) {
        *cmd = CMD_STATUS;
        return true;
    }
    if (strcmp(value, "collect") == 0) {
        *cmd = CMD_COLLECT;
        return true;
    }
    return false;
}

static bool parse_mode(const char *value, ConflictMode *mode) {
    if (strcmp(value, "backup") == 0) {
        *mode = CONFLICT_BACKUP;
        return true;
    }
    if (strcmp(value, "force") == 0) {
        *mode = CONFLICT_FORCE;
        return true;
    }
    if (strcmp(value, "interactive") == 0) {
        *mode = CONFLICT_INTERACTIVE;
        return true;
    }
    return false;
}

static bool parse_arguments(int argc, char **argv, AppOptions *opts) {
    if (argc < 2) {
        print_usage(argv[0]);
        return false;
    }
    if (!parse_command(argv[1], &opts->command)) {
        log_error("Comando desconhecido: %s", argv[1]);
        print_usage(argv[0]);
        return false;
    }
    // Defaults
    if (!get_current_directory(opts->project_root, sizeof(opts->project_root))) {
        log_error("Não foi possível obter diretório atual");
        return false;
    }
    if (!get_machine_name(opts->machine_name, sizeof(opts->machine_name))) {
        opts->machine_name[0] = '\0';
    }
    snprintf(opts->config_path, sizeof(opts->config_path), "configs/dotfiles.conf");
    snprintf(opts->repo_path, sizeof(opts->repo_path), "dotfiles_repo");
    opts->conflict_mode = CONFLICT_BACKUP;
    opts->dry_run = false;
    opts->verbose = false;
    opts->git_auto = false;
    snprintf(opts->git_message, sizeof(opts->git_message), "dotmgr sync");
    opts->config_explicit = false;

    for (int i = 2; i < argc; ++i) {
        const char *arg = argv[i];
        if (strcmp(arg, "--config") == 0) {
            if (i + 1 >= argc) {
                log_error("--config requer um valor");
                return false;
            }
            snprintf(opts->config_path, sizeof(opts->config_path), "%s", argv[++i]);
            opts->config_explicit = true;
            continue;
        }
        if (strcmp(arg, "--repo") == 0) {
            if (i + 1 >= argc) {
                log_error("--repo requer um valor");
                return false;
            }
            snprintf(opts->repo_path, sizeof(opts->repo_path), "%s", argv[++i]);
            continue;
        }
        if (strcmp(arg, "--mode") == 0) {
            if (i + 1 >= argc) {
                log_error("--mode requer um valor");
                return false;
            }
            if (!parse_mode(argv[i + 1], &opts->conflict_mode)) {
                log_error("Modo inválido: %s", argv[i + 1]);
                return false;
            }
            ++i;
            continue;
        }
        if (strcmp(arg, "--machine") == 0) {
            if (i + 1 >= argc) {
                log_error("--machine requer um valor");
                return false;
            }
            snprintf(opts->machine_name, sizeof(opts->machine_name), "%s", argv[++i]);
            continue;
        }
        if (strcmp(arg, "--dry-run") == 0) {
            opts->dry_run = true;
            continue;
        }
        if (strcmp(arg, "--verbose") == 0) {
            opts->verbose = true;
            continue;
        }
        if (strcmp(arg, "--git-auto") == 0) {
            opts->git_auto = true;
            continue;
        }
        if (strcmp(arg, "--git-message") == 0) {
            if (i + 1 >= argc) {
                log_error("--git-message requer um valor");
                return false;
            }
            snprintf(opts->git_message, sizeof(opts->git_message), "%s", argv[++i]);
            continue;
        }
        log_error("Opção desconhecida: %s", arg);
        print_usage(argv[0]);
        return false;
    }

    if (!opts->config_explicit && opts->machine_name[0]) {
        char candidate[PATH_MAX];
        snprintf(candidate, sizeof(candidate), "configs/%s.conf", opts->machine_name);
        if (path_exists(candidate)) {
            log_info("Usando configuração específica da máquina: %s", candidate);
            snprintf(opts->config_path, sizeof(opts->config_path), "%s", candidate);
        }
    }

    return true;
}

static bool run_command(const AppOptions *opts, const DotfileConfig *config) {
    bool success = true;
    for (size_t i = 0; i < config->count; ++i) {
        const DotfileEntry *entry = &config->entries[i];
        bool result = false;
        switch (opts->command) {
            case CMD_INSTALL:
                result = install_entry(opts, entry);
                break;
            case CMD_UNINSTALL:
                result = uninstall_entry(opts, entry);
                break;
            case CMD_STATUS:
                result = status_entry(opts, entry);
                break;
            case CMD_COLLECT:
                result = collect_entry(opts, entry);
                break;
            default:
                result = false;
                break;
        }
        if (!result) {
            success = false;
            if (!opts->verbose) {
                log_error("Falha ao processar entrada %zu", i + 1);
            }
        }
    }
    return success;
}

int main(int argc, char **argv) {
    AppOptions opts;
    memset(&opts, 0, sizeof(opts));
    if (!parse_arguments(argc, argv, &opts)) {
        return EXIT_FAILURE;
    }

    DotfileConfig config;
    if (!load_config(&opts, &config)) {
        return EXIT_FAILURE;
    }

    bool ok = run_command(&opts, &config);
    free_config(&config);

    if (ok && opts.git_auto) {
        if (!git_auto_sync(&opts)) {
            log_warn("Git auto-commit falhou");
        }
    }

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
