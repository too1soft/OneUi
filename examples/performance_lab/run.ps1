param([switch]$Build, [switch]$Test, [switch]$Editor, [switch]$Connections, [switch]$Details, [switch]$Components, [switch]$Effects, [switch]$Layouts, [switch]$Compact, [switch]$Dev, [switch]$DevTools, [switch]$Legacy,
    [ValidatePattern('^[a-z]+/[a-z]+$')][string]$Page='layout/basic',
    [ValidateSet('code','template')][string]$Entry='code', [ValidateSet('light','medium','heavy')][string]$Load='medium', [ValidateSet('auto','gpu','cpu')][string]$Renderer='auto', [ValidateSet('reference','precomputed','batch','combined','mesh')][string]$ParticleMode='mesh')
$ErrorActionPreference = 'Stop'
$legacyMode = $Legacy -or $Editor -or $Connections -or $Details -or $Components -or $Effects -or $Layouts -or $Dev -or
    $PSBoundParameters.ContainsKey('Load') -or $PSBoundParameters.ContainsKey('ParticleMode') -or $PSBoundParameters.ContainsKey('Entry')
if ($legacyMode -and $PSBoundParameters.ContainsKey('Page')) { throw '-Page cannot be combined with legacy scene/benchmark options.' }
$exe = Join-Path $PSScriptRoot 'build-current/bin/oneui-performance-lab.exe'
if ($Build -or $Test -or !(Test-Path -LiteralPath $exe)) { & (Join-Path $PSScriptRoot 'build.ps1') -Test:$Test }
$arguments = @('--renderer', $Renderer)
if ($legacyMode) {
    $view = if ($Components -or $Effects -or $Layouts) { 'components' } elseif ($Details) { 'details' } elseif ($Connections) { 'connections' } elseif ($Editor) { 'editor' } else { 'overview' }
    $arguments += @('--particle-mode', $ParticleMode, '--load', $Load, '--view', $view, '--entry', $Entry)
    if ($Dev) { $arguments += '--dev' }
    if ($Layouts) { $arguments += @('--component-scene', '4') }
    if ($Effects) { $arguments += @('--component-scene', '3') }
} else {
    $arguments += @('--page', $Page)
}
if ($DevTools) { $arguments += '--devtools' }
if ($Compact) { $arguments += '--compact' }
Start-Process -FilePath $exe -ArgumentList $arguments -WorkingDirectory $PSScriptRoot -WindowStyle Hidden
