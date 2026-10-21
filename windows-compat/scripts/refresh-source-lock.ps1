[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$ProjectLock,
    [Parameter(Mandatory=$true)][string]$PreviousSHA256,
    [string]$NativeInputsFile = ''
)
$ErrorActionPreference='Stop'
$foundation=[IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
$lockPath=(Resolve-Path -LiteralPath $ProjectLock).Path
if ((Get-FileHash -LiteralPath $lockPath -Algorithm SHA256).Hash -ne $PreviousSHA256) { throw 'Lock changed since review; refusing to replace it.' }
$value=Get-Content -LiteralPath $lockPath -Raw | ConvertFrom-Json -AsHashtable
function Get-LockedFiles([string]$directory,[bool]$sourceOnly) {
    $base=(Resolve-Path -LiteralPath $directory).Path.TrimEnd('\')
    $entries=@()
    foreach($file in Get-ChildItem -LiteralPath $base -File -Recurse -Force) {
        $relative=$file.FullName.Substring($base.Length+1).Replace('\','/')
        if ($sourceOnly -and $relative -match '(^|/)(\.git|__pycache__|target|\.pytest_cache)/') {continue}
        if ($file.Attributes -band [IO.FileAttributes]::ReparsePoint) {throw "Linked input: $relative"}
        $entries+=@{path=$relative;size=$file.Length;sha256=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()}
    }
    return @($entries | Sort-Object -Property path)
}
$value.foundation.files=Get-LockedFiles $foundation $true
if ($NativeInputsFile) {
    $inputs=Get-Content -LiteralPath $NativeInputsFile -Raw | ConvertFrom-Json -AsHashtable
    $required=@('rust-1.91.0','msvc-14.44.35207','winsdk-include-10.0.22621.0','winsdk-lib-10.0.22621.0','winsdk-bin-10.0.22621.0')
    foreach($key in $required) {
        if (!$inputs[$key] -or ![IO.Path]::IsPathRooted($inputs[$key])) {throw "Missing explicit input: $key"}
        $value.inputs[$key]=@{status='locked';files=(Get-LockedFiles $inputs[$key] $false);acceptance='hash-locked-toolchain-not-product-acceptance'}
    }
}
# This generator changes only content-derived hashes/entries in the named lock.
# It does not mark tests passed or modify the input toolchain installations.
$json=$value | ConvertTo-Json -Depth 100
[IO.File]::WriteAllText($lockPath,$json+"`n",[Text.UTF8Encoding]::new($false))
Write-Output "Updated reviewed source lock: $lockPath"
Write-Output (Get-FileHash -LiteralPath $lockPath -Algorithm SHA256).Hash.ToLowerInvariant()
