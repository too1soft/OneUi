param([switch]$Build, [switch]$Test, [switch]$Editor, [switch]$Connections, [switch]$Components, [switch]$Compact, [switch]$Dev, [ValidateSet('code','template')][string]$Entry='code', [ValidateSet('light','medium','heavy')][string]$Load='medium', [ValidateSet('auto','gpu','cpu')][string]$Renderer='auto', [ValidateSet('reference','precomputed','batch','combined')][string]$ParticleMode='combined')
$ErrorActionPreference = 'Stop'
$exe = Join-Path $PSScriptRoot 'build-current/bin/oneui-performance-lab.exe'
if ($Build -or $Test -or !(Test-Path -LiteralPath $exe)) { & (Join-Path $PSScriptRoot 'build.ps1') -Test:$Test }
$view = if ($Components) { 'components' } elseif ($Connections) { 'connections' } elseif ($Editor) { 'editor' } else { 'overview' }
$devArg = if($Dev){' --dev'}else{''}
if($Compact){$devArg+=' --compact'}
Start-Process -FilePath $exe -ArgumentList "--renderer $Renderer --particle-mode $ParticleMode --load $Load --view $view --entry $Entry$devArg" -WorkingDirectory $PSScriptRoot -WindowStyle Hidden
