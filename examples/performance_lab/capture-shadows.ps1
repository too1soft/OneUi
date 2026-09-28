param([string]$Output=(Join-Path $PSScriptRoot 'results/shadow-pixels'),[string]$Python='python')
$ErrorActionPreference='Stop'
$Output=[IO.Path]::GetFullPath($Output)
$exe=Join-Path $PSScriptRoot 'build-current/bin/oneui-effects-tests.exe'
$before=$env:ONEUI_SHADOW_REFERENCE
try {
    foreach($mode in 'reference','optimized'){
        if($mode -eq 'reference'){$env:ONEUI_SHADOW_REFERENCE='1'}else{Remove-Item Env:ONEUI_SHADOW_REFERENCE -ErrorAction SilentlyContinue}
        foreach($backend in 'cpu','gpu'){
            & $exe "--$backend" (Join-Path $Output $mode)
            if($LASTEXITCODE){throw "Native capture failed: $mode/$backend"}
        }
    }
    & $Python (Join-Path $PSScriptRoot 'check-shadow-pixels.py') $Output
    if($LASTEXITCODE){throw 'Shadow pixel comparison failed'}
} finally {
    if($null -eq $before){Remove-Item Env:ONEUI_SHADOW_REFERENCE -ErrorAction SilentlyContinue}else{$env:ONEUI_SHADOW_REFERENCE=$before}
}
