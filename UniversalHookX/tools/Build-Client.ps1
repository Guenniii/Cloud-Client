param(
    [ValidateSet('build', 'install')][string]$Action = 'build',
    [string]$JavaRoot,
    [string]$Output,
    [switch]$DryRun
)
$ErrorActionPreference = 'Stop'
$arguments = @('-X', 'utf8', (Join-Path $PSScriptRoot 'client_build.py'), $Action)
if ($JavaRoot) { $arguments += @('--java-root', $JavaRoot) }
if ($Output) { $arguments += @('--output', $Output) }
if ($DryRun) { $arguments += '--dry-run' }
& python @arguments
exit $LASTEXITCODE
