param([switch]$Build, [switch]$Test, [switch]$Code, [switch]$Dev, [switch]$Dark, [ValidateSet('settings','list','detail')][string]$Page='settings')
$ErrorActionPreference = 'Stop'
$exe = Join-Path $PSScriptRoot 'build/bin/oneui-declarative-demo.exe'
if ($Build -or $Test -or !(Test-Path -LiteralPath $exe)) { & "$PSScriptRoot/build.ps1" -Test:$Test }
$arguments = @('--page', $Page)
if ($Code) { $arguments += '--code' }
if ($Dev) { $arguments += @('--dev','--layout-report',('"' + (Join-Path $PSScriptRoot 'artifacts/layout-current.txt') + '"')) }
if ($Dark) { $arguments += '--dark' }
Start-Process -FilePath $exe -ArgumentList $arguments -WorkingDirectory "$PSScriptRoot" -WindowStyle Hidden
