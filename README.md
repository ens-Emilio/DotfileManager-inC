# Dotfiles Manager in C

Gerenciador de dotfiles inspirado no GNU Stow, implementado em C para oferecer controle direto sobre operações POSIX. Ele lê um arquivo de configuração declarativo, resolve conflitos e cria links simbólicos entre um repositório central e os locais esperados no sistema.

## Estrutura atual

```
├─ configs/            # Arquivos de configuração declarativos
├─ docs/               # Documentação técnica
├─ include/            # Headers públicos
├─ src/                # Implementação em C
├─ dotfiles_repo/      # Repositório modelo de dotfiles
└─ tests/              # Casos de teste (futuros)
```

## Build

### Windows (PowerShell + MSYS2)

1. Instale MSYS2 e os pacotes necessários:
   ```powershell
   pacman -S --needed base-devel mingw-w64-x86_64-toolchain
   ```
2. Use o script auxiliar:
   ```powershell
   powershell -ExecutionPolicy Bypass -File .\build.ps1 -Action build
   ```
   Ações válidas: `build`, `clean`, `run`, `status`, `install`, `uninstall`.  
   Exemplos:
   ```powershell
   .\build.ps1 -Action status -ExtraArgs "--dry-run --verbose"
   .\build.ps1 -Action install -ExtraArgs "--dry-run"
   ```

### POSIX / MSYS shell direto

```bash
make
```

O binário `dotmgr` será gerado em `build/dotmgr`. Para limpar artefatos:

```bash
make clean
```

## Uso rápido

```
./dotmgr install --config configs/dotfiles.conf --repo dotfiles_repo --mode backup
./dotmgr status
./dotmgr uninstall --dry-run
```

## Próximos passos

- Expandir modo `collect` para copiar diretórios completos.
- Integrar testes automatizados (cmocka ou similar).
- Adicionar integração Git opcional.
- Documentar workflows específicos por máquina (multi-config).
