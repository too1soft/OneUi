param(
    [Parameter(Mandatory=$true)][string]$BuildDir,
    [Parameter(Mandatory=$true)][string]$OutputDirectory,
    [Parameter(Mandatory=$true)][string]$WindowsReference,
    [ValidateSet('x86','x64')][string]$Arch = 'x64'
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'windows-build-profile.ps1')
$root = Split-Path -Parent $PSScriptRoot
$build = Resolve-OneUIBuildPath $BuildDir
$output = Resolve-OneUIBuildPath $OutputDirectory
$zip = $output + '.zip'
if ((Test-Path -LiteralPath $output) -or (Test-Path -LiteralPath $zip)) {
    throw 'Use a new output directory; existing SDKs and archives are never replaced.'
}
$cache = Get-Content -LiteralPath (Join-Path $build 'CMakeCache.txt') -Raw
if ($cache -notmatch '(?m)^ONEUI_WINDOWS_BASELINE:STRING=win7\r?$') { throw 'Not a Win7 build' }
if ($cache -notmatch '(?m)^ONEUI_ENABLE_TEST_FRAME_CAPTURE:BOOL=OFF\r?$') { throw 'QA frame capture must be disabled in SDK deliverables' }
# PE export names are case-sensitive; PowerShell's JSON object conversion is
# not (e.g. FWClosePolicyStore/FwClosePolicyStore). Parse using the auditor's
# Python runtime rather than dropping distinct exports or requiring PS 7.
$referenceArch = & python -c "import json,sys; print(json.load(open(sys.argv[1],encoding='utf-8'))['architecture'])" $WindowsReference
if ($LASTEXITCODE -ne 0 -or $referenceArch -ne $Arch) { throw 'Windows reference architecture does not match package' }
$dll = Join-Path $build 'oneui.dll'
$gallery = Join-Path $build 'examples/gallery/oneui_gallery.exe'
& python (Join-Path $PSScriptRoot 'audit-windows-runtime.py') --binary $dll --binary $gallery --runtime-dir $build --reference $WindowsReference
if ($LASTEXITCODE -ne 0) { throw 'Win7 runtime import audit failed; no package created' }
foreach ($folder in @('bin','lib','include','cmake','docs','verification','licenses')) {
    New-Item -ItemType Directory -Path (Join-Path $output $folder) -Force | Out-Null
}
Copy-Item -LiteralPath $dll,$gallery -Destination (Join-Path $output 'bin')
Copy-Item -LiteralPath (Join-Path $build 'oneui.lib') -Destination (Join-Path $output 'lib')
Get-ChildItem -LiteralPath (Join-Path $root 'include') | Copy-Item -Destination (Join-Path $output 'include') -Recurse
Copy-Item -LiteralPath (Join-Path $root 'cmake/OneUIConfig.cmake'),(Join-Path $root 'cmake/OneUITargets.cmake') -Destination (Join-Path $output 'cmake')
Copy-Item -LiteralPath (Join-Path $root 'cmake/windows-build-profiles.json') -Destination (Join-Path $output 'verification')
Copy-Item -LiteralPath (Join-Path $root 'docs/24-windows7-compatibility.md') -Destination (Join-Path $output 'docs')
foreach ($name in @('LICENSE','THIRD_PARTY_NOTICES.md')) {
    $file = Join-Path $root $name
    if (Test-Path -LiteralPath $file) { Copy-Item -LiteralPath $file -Destination $output }
}
# Ship the actual reviewed dependency notices, including the FTL attribution.
# Missing license inputs are a packaging failure, not silently omitted files.
$licenseFiles = @(
    @('LICENSE', 'oneui-MIT.txt'),
    @('third_party/skia/LICENSE', 'skia-BSD.txt'),
    @('third_party/skia/third_party/externals/icu/LICENSE', 'icu.txt'),
    @('third_party/skia/third_party/externals/harfbuzz/COPYING', 'harfbuzz.txt'),
    @('third_party/skia/third_party/externals/harfbuzz/src/ms-use/COPYING', 'harfbuzz-ms-use.txt'),
    @('third_party/skia/third_party/externals/freetype/LICENSE.TXT', 'freetype-license-selection.txt'),
    @('third_party/skia/third_party/externals/freetype/docs/FTL.TXT', 'freetype-FTL.txt'),
    @('third_party/skia/third_party/externals/freetype/src/bdf/README', 'freetype-bdf.txt'),
    @('third_party/skia/third_party/externals/freetype/src/pcf/README', 'freetype-pcf.txt'),
    @('third_party/skia/third_party/externals/freetype/src/gzip/zlib.h', 'freetype-gzip-notice.txt'),
    @('third_party/skia/third_party/externals/libjpeg-turbo/LICENSE.md', 'libjpeg-turbo.md'),
    @('third_party/skia/third_party/externals/libjpeg-turbo/README.ijg', 'libjpeg-IJG.txt'),
    @('third_party/skia/third_party/externals/libpng/LICENSE', 'libpng.txt'),
    @('third_party/skia/third_party/externals/libwebp/COPYING', 'libwebp.txt'),
    @('third_party/skia/third_party/externals/libwebp/PATENTS', 'libwebp-patents.txt'),
    @('third_party/skia/third_party/externals/zlib/LICENSE', 'zlib.txt'),
    @('third_party/skia/third_party/externals/expat/expat/COPYING', 'expat.txt')
)
foreach ($entry in $licenseFiles) {
    Copy-Item -LiteralPath (Join-Path $root $entry[0]) -Destination (Join-Path $output ('licenses/' + $entry[1]))
}
@'
OneUI Win7 compatibility SDK - third-party notices
=================================================
This software uses the FreeType Project (https://freetype.org), distributed
under the FreeType License (FTL), not the alternative GPL. Portions of this
software are copyright The FreeType Project. All rights reserved.

The SDK includes statically linked Skia, ICU, HarfBuzz, FreeType, libjpeg-turbo,
libpng, libwebp, zlib and Expat code. Their notices are in licenses/.
Skia and ICU contain OneUI-maintained fixes documented in the source patch
directory. Upstream copyrights and licenses remain in effect.
Keep these notices and the FreeType acknowledgment in redistributed products.
No Microsoft system DLL or test font is included in this SDK.
'@ | Set-Content -LiteralPath (Join-Path $output 'THIRD_PARTY_NOTICES.txt') -Encoding ASCII
& python (Join-Path $PSScriptRoot 'audit-windows-runtime.py') --directory (Join-Path $output 'bin') --reference $WindowsReference --output (Join-Path $output 'verification/import-audit.json')
if ($LASTEXITCODE -ne 0) { throw 'Packaged import audit failed; archive not created' }
@"
OneUI Windows 7 SP1 $Arch compatibility SDK (candidate)
===================================================
DLL/import-library SDK. The original SDK has NOT been replaced.
Keep bin/oneui.dll beside your application executable.
Use include/ + lib/oneui.lib, or find_package(OneUI CONFIG).
Build the final EXE with the pinned v143 /MT profile; audit the complete final
application, not only this DLL. Go and updater runtime support is independent.
No VC redistributable, MSYS2 or UCRT deployment is required by these binaries.

verification/import-audit.json proves static import compatibility only.
It does NOT certify every Win7 GPU, driver, input method or application.
Read docs/24-windows7-compatibility.md and the accompanying acceptance report.
This package does not include static implementation archives. Applications
requiring static linkage should use the pinned source build and its full
reviewed Skia/text dependency closure.
"@ | Set-Content -LiteralPath (Join-Path $output 'README.txt') -Encoding ASCII
Get-ChildItem -LiteralPath $output -Recurse -File | Sort-Object FullName | ForEach-Object {
    $hash = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    $relative = $_.FullName.Substring($output.Length + 1).Replace('\','/')
    "$hash  $relative"
} | Set-Content -LiteralPath (Join-Path $output 'SHA256SUMS') -Encoding ASCII
Compress-Archive -Path (Join-Path $output '*') -DestinationPath $zip
Get-FileHash -LiteralPath $zip -Algorithm SHA256 | Format-List
