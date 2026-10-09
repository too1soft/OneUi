param(
    [string]$Version = '0.1.0-preview.1'
)
$ErrorActionPreference = 'Stop'
$source = Split-Path -Parent $PSScriptRoot
$website = Join-Path $source 'website'
$output = Join-Path $source 'dist/oneui-website'
$cache = Join-Path $source 'out/portal-build'
$reports = Join-Path $source 'out/native-release-review'
if (-not (Test-Path -LiteralPath (Join-Path $output 'releases.json'))) {
    throw 'Package and index the SDK release first. This build does not compile OneUI or its dependencies.'
}
New-Item -ItemType Directory -Force -Path $cache, $reports | Out-Null
$oldGoCache, $oldGoOS, $oldGoArch, $oldCGO = $env:GOCACHE, $env:GOOS, $env:GOARCH, $env:CGO_ENABLED
$oldTemp, $oldTmp = $env:TEMP, $env:TMP
$oldDelivery = $env:ONEUI_DELIVERY_DIR
try {
    $env:TEMP = Join-Path $cache 'tmp'
    $env:TMP = $env:TEMP
    New-Item -ItemType Directory -Force -Path $env:TEMP | Out-Null
    & python -B (Join-Path $PSScriptRoot 'prepare-website-dlls.py') --directory $output
    if ($LASTEXITCODE -ne 0) { throw 'DLL extraction/verification failed' }
    Push-Location $website
    try {
        & npm.cmd run generate *> (Join-Path $reports 'portal-nuxt-build.log')
        if ($LASTEXITCODE -ne 0) { throw 'Nuxt generation failed; see portal-nuxt-build.log' }
    } finally { Pop-Location }
    $env:GOCACHE = Join-Path $cache 'go-cache'
    $env:CGO_ENABLED = '0'
    $env:GOARCH = 'amd64'
    Push-Location (Join-Path $website 'server')
    try {
        $env:GOOS = 'windows'
        $env:ONEUI_DELIVERY_DIR = $output
        & go test -v ./... *> (Join-Path $reports 'portal-go-tests.log')
        if ($LASTEXITCODE -ne 0) { throw 'Go tests failed' }
        & go build -trimpath -ldflags "-s -w -X main.buildVersion=$Version" -o (Join-Path $output 'oneui-site-windows-amd64.exe') .
        if ($LASTEXITCODE -ne 0) { throw 'Windows server build failed' }
        $env:GOOS = 'linux'
        & go build -trimpath -ldflags "-s -w -X main.buildVersion=$Version" -o (Join-Path $output 'oneui-site-linux-amd64') .
        if ($LASTEXITCODE -ne 0) { throw 'Linux server build failed' }
    } finally { Pop-Location }
    & (Join-Path $output 'oneui-site-windows-amd64.exe') --check
    if ($LASTEXITCODE -ne 0) { throw 'Release catalog integrity check failed' }
    Copy-Item -LiteralPath (Join-Path $website 'deploy/README.txt'), (Join-Path $website 'deploy/oneui-site.service'), (Join-Path $website 'deploy/oneui.too1.net.nginx.conf') -Destination $output -Force
    Copy-Item -LiteralPath (Join-Path $source 'docs/portal-deployment.md') -Destination (Join-Path $output 'DEPLOYMENT.md') -Force
    $hashes = Get-ChildItem -LiteralPath $output -File | Where-Object Name -ne 'SHA256SUMS' | Sort-Object Name | ForEach-Object {
        '{0}  {1}' -f (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant(), $_.Name
    }
    [IO.File]::WriteAllLines((Join-Path $output 'SHA256SUMS'), $hashes, [Text.UTF8Encoding]::new($false))
    Write-Output "Website delivery: $output"
} finally {
    $env:GOCACHE, $env:GOOS, $env:GOARCH, $env:CGO_ENABLED = $oldGoCache, $oldGoOS, $oldGoArch, $oldCGO
    $env:TEMP, $env:TMP = $oldTemp, $oldTmp
    $env:ONEUI_DELIVERY_DIR = $oldDelivery
}
