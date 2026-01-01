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
   > **Nota:** Para criar/remover symlinks reais no Windows é necessário executar o terminal com privilégios elevados **ou** habilitar o *Developer Mode* (Configurações → Atualização e Segurança → Para Desenvolvedores). Sem isso, utilize `--dry-run` ou rode em um ambiente POSIX.

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
./dotmgr collect --dry-run --verbose
```

### Flags avançadas

- `--machine <nome>`: força o carregamento de `configs/<nome>.conf`. Se não informado, o programa detecta o hostname e usa o arquivo correspondente automaticamente (por exemplo, `configs/work.conf`, `configs/home.conf`).
- `--git-auto` + `--git-message "<msg>"`: após concluir o comando (`install`, `collect`, etc.), executa `git add`, `git commit` e `git push` dentro do repositório indicado.
- `collect`: copia os arquivos já existentes no sistema para o repositório antes de criar os links, preservando personalizações locais.

### Workflow multi-máquina

1. Crie configs específicas (já existem exemplos em `configs/work.conf` e `configs/home.conf`).
2. No PC do trabalho:
   ```bash
   ./dotmgr collect --machine work --git-auto --git-message "work tweaks"
   ```
   Isso coleta as mudanças locais, cria symlinks e registra o commit automaticamente.
3. Em casa:
   ```bash
   ./dotmgr install --machine home --git-auto --git-message "sync home"
   ```
   O programa usará `configs/home.conf`, instalará os links e sincronizará via Git.

## Próximos passos

- Expandir modo `collect` para copiar diretórios completos.
- Integrar testes automatizados (cmocka ou similar).
- Adicionar integração Git opcional.
- Documentar workflows específicos por máquina (multi-config).
