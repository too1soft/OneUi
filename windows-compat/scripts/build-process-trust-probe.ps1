[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$InputsFile,
    [Parameter(Mandatory=$true)][string]$ProjectLock,
    [Parameter(Mandatory=$true)][string]$OutputDir,
    [ValidateSet('x86','x64')][string]$Architecture = 'x86'
)
$ErrorActionPreference='Stop'
$foundation=Split-Path -Parent $PSScriptRoot
$source=Join-Path $foundation 'runtime/native/trust'
$inputs=Get-Content -LiteralPath $InputsFile -Raw | ConvertFrom-Json
$compilerRoot=[string]$inputs.'msvc-14.44.35207'
$sdkInclude=[string]$inputs.'winsdk-include-10.0.22621.0'
$sdkLib=[string]$inputs.'winsdk-lib-10.0.22621.0'
$output=[IO.Path]::GetFullPath($OutputDir)
if (Test-Path -LiteralPath $output) {throw 'Use a new output directory; evidence must not be overwritten.'}
foreach($value in @($compilerRoot,$sdkInclude,$sdkLib)) {
    if (![IO.Path]::IsPathRooted($value) -or !(Test-Path -LiteralPath $value -PathType Container)) {throw 'Explicit locked compiler / SDK inputs are required.'}
}
if ((Split-Path -Leaf $compilerRoot) -ne '14.44.35207' -or (Split-Path -Leaf $sdkInclude) -ne '10.0.22621.0' -or (Split-Path -Leaf $sdkLib) -ne '10.0.22621.0') {throw 'Unreviewed compiler / SDK version.'}
$lock=Get-Content -LiteralPath $ProjectLock -Raw | ConvertFrom-Json
if ($lock.schemaVersion -ne 1 -or !$lock.foundation.files) {throw 'Missing foundation trust anchor.'}
$lockedPython=@($lock.foundation.files | Where-Object path -Like '*.py' | ForEach-Object {$_.path.ToLowerInvariant()})
foreach($module in Get-ChildItem -LiteralPath $foundation -Filter '*.py' -File -Recurse) {
    $relative=$module.FullName.Substring($foundation.Length+1).Replace('\','/').ToLowerInvariant()
    if ($relative -match '(^|/)(\.git|__pycache__)/') {continue}
    if ($relative -notin $lockedPython) {throw "Unlisted foundation Python module: $relative"}
}
foreach($entry in $lock.foundation.files) {
    if ($entry.path -match '(^/|\\|:|(^|/)\.\.(/|$))') {throw 'Unsafe lock path.'}
    $file=Join-Path $foundation $entry.path
    if (!(Test-Path -LiteralPath $file -PathType Leaf) -or (Get-Item -LiteralPath $file).Length -ne $entry.size -or (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant() -ne $entry.sha256) {throw "Foundation differs from lock: $($entry.path)"}
}
$verify=@((Join-Path $foundation 'compat.py'),'verify-lock','--lock',$ProjectLock,'--root',$foundation)
foreach($key in @('msvc-14.44.35207','winsdk-include-10.0.22621.0','winsdk-lib-10.0.22621.0')) {
    $verify+=@('--only',$key,'--input',"$key=$($inputs.$key)")
}
& python @verify
if ($LASTEXITCODE -ne 0) {throw 'Native toolchain content does not match the lock.'}
$roots=Get-Content -LiteralPath (Join-Path $source 'certificates/sources.json') -Raw | ConvertFrom-Json
New-Item -ItemType Directory -Path $output | Out-Null
$header=foreach($root in $roots.certificates) {
    if ($root.file -notmatch '^[a-z0-9-]+\.der$' -or $root.symbol -notmatch '^[a-z_]+$') {throw 'Invalid root metadata.'}
    $path=Join-Path $source "certificates/$($root.file)"
    if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() -ne $root.sha256) {throw 'Pinned root hash mismatch.'}
    $values=([IO.File]::ReadAllBytes($path) | ForEach-Object {'0x{0:x2}' -f $_}) -join ','
    "static const unsigned char $($root.symbol)[] = {$values};"
}
# Generated build input, not an independently edited source / trust source.
[IO.File]::WriteAllLines((Join-Path $output 'trust-roots.generated.h'),$header)
$env:INCLUDE=@((Join-Path $compilerRoot 'include'),(Join-Path $sdkInclude 'ucrt'),(Join-Path $sdkInclude 'shared'),(Join-Path $sdkInclude 'um')) -join ';'
$env:LIB=@((Join-Path $compilerRoot "lib/$Architecture"),(Join-Path $sdkLib "ucrt/$Architecture"),(Join-Path $sdkLib "um/$Architecture")) -join ';'
$env:PATH=(Join-Path $compilerRoot "bin/Hostx64/$Architecture")+';'+$env:PATH
$compiler=Join-Path $compilerRoot "bin/Hostx64/$Architecture/cl.exe"
$exe=Join-Path $output 'process-trust-probe.exe'
Push-Location $output
try {
    & $compiler /nologo /W4 /WX /O2 /MT /guard:cf /DUNICODE /D_UNICODE "/I$output" (Join-Path $source 'trust.c') (Join-Path $source 'probe.c') "/Fe:$exe" /link /Brepro /SUBSYSTEM:CONSOLE,6.01 /DYNAMICBASE /NXCOMPAT wintrust.lib crypt32.lib
    if ($LASTEXITCODE -ne 0) {throw 'Trust probe compilation failed.'}
    & (Join-Path $compilerRoot "bin/Hostx64/$Architecture/lib.exe") /nologo /Brepro /OUT:windows-compat-trust.lib trust.obj
    if ($LASTEXITCODE -ne 0) {throw 'Trust static library creation failed.'}
} finally {Pop-Location}
Copy-Item -LiteralPath (Join-Path $source 'trust.h') -Destination (Join-Path $output 'trust.h')
@{schemaVersion=1;scope='experimental-probe-not-product';architecture=$Architecture;sha256=(Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash.ToLowerInvariant();librarySHA256=(Get-FileHash -LiteralPath (Join-Path $output 'windows-compat-trust.lib') -Algorithm SHA256).Hash.ToLowerInvariant();projectLockSHA256=(Get-FileHash -LiteralPath $ProjectLock -Algorithm SHA256).Hash.ToLowerInvariant();compilerSHA256=(Get-FileHash -LiteralPath $compiler -Algorithm SHA256).Hash.ToLowerInvariant();rootBundle=$roots.bundleVersion;time=[DateTime]::UtcNow.ToString('o')} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $output 'build-evidence.json') -Encoding utf8
Write-Output $exe
