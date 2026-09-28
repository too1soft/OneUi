param([switch]$Build, [switch]$Test, [switch]$Editor, [switch]$Connections, [switch]$Details, [switch]$Components, [switch]$Effects, [switch]$Layouts, [switch]$Compact, [switch]$Dev, [switch]$DevTools, [ValidateSet('code','template')][string]$Entry='code', [ValidateSet('light','medium','heavy')][string]$Load='medium', [ValidateSet('auto','gpu','cpu')][string]$Renderer='auto', [ValidateSet('reference','precomputed','batch','combined','mesh')][string]$ParticleMode='mesh')
$ErrorActionPreference = 'Stop'
if($ParticleMode -eq 'mesh'){Write-Host 'GPU 粒子默认使用 mesh；软件或不支持的情况自动回退。可用 -ParticleMode combined 对照。'}
$exe = Join-Path $PSScriptRoot 'build-current/bin/oneui-performance-lab.exe'
if ($Build -or $Test -or !(Test-Path -LiteralPath $exe)) { & (Join-Path $PSScriptRoot 'build.ps1') -Test:$Test }
$view = if ($Components -or $Effects -or $Layouts) { 'components' } elseif ($Details) { 'details' } elseif ($Connections) { 'connections' } elseif ($Editor) { 'editor' } else { 'overview' }
$devArg = if($Dev){' --dev'}else{''}
if($Layouts){$devArg+=' --component-scene 4'}
if($Effects){$devArg+=' --component-scene 3'}
if($DevTools){$devArg+=' --devtools'}
if($Compact){$devArg+=' --compact'}
Start-Process -FilePath $exe -ArgumentList "--renderer $Renderer --particle-mode $ParticleMode --load $Load --view $view --entry $Entry$devArg" -WorkingDirectory $PSScriptRoot -WindowStyle Hidden
