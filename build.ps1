param([switch]$Upload)
$ErrorActionPreference = 'Stop'
$taskRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$createdAlias = $false
$previousCore = $env:PLATFORMIO_CORE_DIR
try {
    if (Test-Path 'R:\') {
        $mapping = subst | Where-Object { $_ -match '^R:\\:' }
        if (-not $mapping -or -not $mapping.EndsWith($taskRoot, [StringComparison]::OrdinalIgnoreCase)) {
            throw 'Drive R: is in use. Choose another unused drive letter in this script.'
        }
    } else {
        subst R: $taskRoot
        if ($LASTEXITCODE -ne 0) { throw 'Could not create a short build path.' }
        $createdAlias = $true
    }
    $env:PLATFORMIO_CORE_DIR = 'R:\work\platformio'
    $buildArgs = @('-m', 'platformio', 'run', '--project-dir', 'R:\outputs\cyd-mesh-terminal')
    if ($Upload) { $buildArgs += @('--target', 'upload') }
    & 'R:\work\venv\Scripts\python.exe' @buildArgs
    if ($LASTEXITCODE -ne 0) { throw 'Build or upload failed.' }
} finally {
    $env:PLATFORMIO_CORE_DIR = $previousCore
    if ($createdAlias) { subst R: /D }
}
