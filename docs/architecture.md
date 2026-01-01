# Dotfiles Manager Architecture

## Visão Geral

O projeto segue uma abordagem modular para manter o código simples de testar e estender. Cada módulo encapsula uma responsabilidade clara:

1. **Config Parser** (`config_parser`) – interpreta arquivos declarativos e gera uma lista de dotfiles.
2. **Discovery** (`discovery`) – percorre o repositório para detectar arquivos automaticamente (modo opcional futuro).
3. **Symlink Engine** (`symlink_engine`) – cria, atualiza e remove links simbólicos com validações.
4. **Conflict Manager** (`conflict_manager`) – aplica políticas (backup, força, interativo) quando já existe algo no destino.
5. **Utils** (`utils`) – utilidades de caminhos, expansão de `~`, logging colorido e helpers para diretórios.
6. **CLI** (`main.c`) – interpreta comandos (`install`, `uninstall`, `status`) e orquestra os módulos.

```
┌─────────────┐  entries   ┌─────────────────┐
│ config file │──────────▶│ config_parser   │
└─────────────┘           └──────┬───────────┘
                                 │ DotfileConfig
                           ┌─────▼─────────┐
                           │ symlink_engine│
                           └─────┬─────────┘
                    conflicts   │        discovery (opcional)
                           ┌────▼─────┐
                           │conflicts│
                           └─────────┘
```

## Estruturas

```c
typedef struct {
    char source_path[PATH_MAX];
    char target_path[PATH_MAX];
    bool is_directory;
} DotfileEntry;

typedef struct {
    DotfileEntry *entries;
    size_t count;
} DotfileConfig;
```

## Fluxos

### Install
1. Parse config.
2. Validar existência do alvo no repositório.
3. Resolver conflitos no destino (`conflict_manager`).
4. Criar diretórios pais (`utils`).
5. Criar symlink (`symlink_engine`).

### Collect (futuro)
1. Copiar destino → repositório.
2. Executar fluxo Install.

### Update
1. Identificar links quebrados (`status`).
2. Reinstalar entradas inconsistentes.

### Uninstall
1. Conferir se o destino é symlink apontando para o repositório.
2. Remover com `unlink()` e restaurar backups (se existirem).

### Status
1. Verificar se symlink existe e aponta para o target correto.
2. Detectar arquivos conflitantes ou links quebrados.
3. Exibir relatório com cores/legendas.

## Extensões Futuras
- Hooks pré/pós-instalação.
- Suporte multi-config (por hostname).
- Modo `dry-run` (já previsto nos helpers, aplicar ao resto do fluxo).
- Integração Git (`git_integration.c`).
- Testes unitários em `tests/` com `cmocka`.
