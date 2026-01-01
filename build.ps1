param(
    [ValidateSet("build", "clean", "run", "status", "install", "uninstall")]
    [string]$Action = "build",
    [string]$ExtraArgs = ""
)

function Convert-ToPosixPath {
    param([string]$Path)
    $fullPath = [System.IO.Path]::GetFullPath($Path)
    $drive = $fullPath.Substring(0,1).ToLower()
    $rest = $fullPath.Substring(2).Replace('\', '/')
    return "/$drive$rest"
}

function Normalize-Args {
    param([string]$Value)
    if ([string]::IsNullOrWhiteSpace($Value)) {
        return ""
    }
    $trimmed = $Value.Trim()
    if ($trimmed.Length -ge 2) {
        if (($trimmed.StartsWith('"') -and $trimmed.EndsWith('"')) -or
            ($trimmed.StartsWith("'") -and $trimmed.EndsWith("'"))) {
            $trimmed = $trimmed.Substring(1, $trimmed.Length - 2)
        }
    }
    return $trimmed
}

function Merge-Args {
    param(
        [string]$BaseCommand,
        [string]$ExtraArgs
    )
    if ([string]::IsNullOrWhiteSpace($ExtraArgs)) {
        return $BaseCommand
    }
    return "$BaseCommand $ExtraArgs"
}

$msysRoot = if ($env:MSYS2_ROOT) { $env:MSYS2_ROOT } else { "C:\msys64" }
$shellExe = Join-Path $msysRoot "msys2_shell.cmd"

if (-not (Test-Path $shellExe)) {
    Write-Error "MSYS2 shell não encontrado em $shellExe. Ajuste a variável de ambiente MSYS2_ROOT." 
    exit 1
}

$projectPosix = Convert-ToPosixPath (Get-Location)
$normalizedArgs = Normalize-Args -Value $ExtraArgs
Write-Host "[dotmgr] Args brutos: '$ExtraArgs' | normalizados: '$normalizedArgs'"
$commandMap = @{
    build     = "make"
    clean     = "make clean"
    run       = Merge-Args "./build/dotmgr" $normalizedArgs
    status    = Merge-Args "./build/dotmgr status" $normalizedArgs
    install   = Merge-Args "./build/dotmgr install" $normalizedArgs
    uninstall = Merge-Args "./build/dotmgr uninstall" $normalizedArgs
}

if (-not $commandMap.ContainsKey($Action)) {
    Write-Error "Ação desconhecida: $Action"
    exit 1
}

$msysCommand = "cd '$projectPosix' && $($commandMap[$Action])"
Write-Host "[dotmgr] Executando no MSYS2: $msysCommand"

$arguments = @(
    "-defterm",
    "-no-start",
    "-mingw64",
    "-c",
    $msysCommand
)

& $shellExe @arguments
$exitCode = $LASTEXITCODE
if ($exitCode -ne 0) {
    exit $exitCode
}
exit 0
