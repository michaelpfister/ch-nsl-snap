# Replaces the Snapslash folder with another version once the app has quit
# (docs/02-decisions.md, D-037). UpdateInstaller starts it hidden; it works
# with Windows PowerShell 5.1, which every Windows 10 and 11 has.
# Keep this file ASCII: PowerShell 5.1 reads files without a BOM as ANSI.
param(
    [Parameter(Mandatory = $true)] [int] $ProcessId,
    [Parameter(Mandatory = $true)] [string] $AppDir,
    [Parameter(Mandatory = $true)] [string] $NewDir,
    [string] $Stage = '',
    [string] $Exe = ''
)

$ErrorActionPreference = 'Stop'
$log = Join-Path $env:TEMP 'snapslash-update.log'
function Write-Log([string] $text) {
    Add-Content -LiteralPath $log -Value ('{0:s} {1}' -f (Get-Date), $text)
}
function Start-App([string] $dir) {
    if ($Exe) { Start-Process -FilePath (Join-Path $dir $Exe) }
}

# Wait until the app has quit, so none of its files is in use any more.
try { Wait-Process -Id $ProcessId -Timeout 60 } catch { }

$old = "$AppDir.old"
if (Test-Path -LiteralPath $old) { Remove-Item -LiteralPath $old -Recurse -Force }

# A virus scanner may hold a file for a moment after the process ended.
$moved = $false
for ($i = 0; $i -lt 30 -and -not $moved; $i++) {
    try {
        Move-Item -LiteralPath $AppDir -Destination $old
        $moved = $true
    } catch {
        Start-Sleep -Milliseconds 500
    }
}
if (-not $moved) {
    Write-Log "Could not move $AppDir aside; the installed version stays."
    Start-App $AppDir
    exit 1
}

try {
    Move-Item -LiteralPath $NewDir -Destination $AppDir
} catch {
    Write-Log "Could not move $NewDir into place ($_); the installed version is restored."
    Move-Item -LiteralPath $old -Destination $AppDir
    Start-App $AppDir
    exit 1
}

Write-Log "Installed $NewDir as $AppDir."
Start-App $AppDir
Remove-Item -LiteralPath $old -Recurse -Force -ErrorAction SilentlyContinue
if ($Stage) { Remove-Item -LiteralPath $Stage -Recurse -Force -ErrorAction SilentlyContinue }
exit 0
