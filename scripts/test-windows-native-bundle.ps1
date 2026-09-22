param(
    [string]$Bundle = (Split-Path -Parent $MyInvocation.MyCommand.Path),
    [string]$OutputDirectory = "",
    [switch]$CaptureFrames
)

# Deliberately compatible with the PowerShell 2.0 shipped with Windows 7 SP1.
# Run inside a disposable, logged-in test guest; clipboard/tray tests affect
# that test desktop. This is not a script for a customer's active desktop.
$ErrorActionPreference = "Stop"
if (!$OutputDirectory) { $OutputDirectory = Join-Path $Bundle ("results-" + (Get-Date -Format yyyyMMdd-HHmmss)) }
if (Test-Path -LiteralPath $OutputDirectory) { throw "Refusing to overwrite acceptance results: $OutputDirectory" }
New-Item -ItemType Directory -Path $OutputDirectory | Out-Null
$env:ONEUI_TEST_ASSET_ROOT = Join-Path $Bundle "assets"
$os = Get-WmiObject Win32_OperatingSystem
@("Caption=$($os.Caption)", "Version=$($os.Version)", "ServicePack=$($os.ServicePackMajorVersion)",
  "Architecture=$($os.OSArchitecture)", "Started=$(Get-Date -Format o)") |
    Out-File (Join-Path $OutputDirectory "environment.txt") -Encoding utf8
$script:results = @()

function Run-Test($Name, $Executable, $Arguments = "") {
    $stdout = Join-Path $OutputDirectory ($Name + ".stdout.txt")
    $stderr = Join-Path $OutputDirectory ($Name + ".stderr.txt")
    $elapsed = [Diagnostics.Stopwatch]::StartNew()
    $exitCode = -1
    try {
        $options = @{ FilePath=$Executable; WorkingDirectory=$Bundle; PassThru=$true;
            RedirectStandardOutput=$stdout; RedirectStandardError=$stderr }
        if ($Arguments) { $options.ArgumentList = $Arguments }
        $process = Start-Process @options
        # Keep the process handle open so ExitCode remains available after exit.
        $processHandle = $process.Handle
        if (!$process.WaitForExit(60000)) {
            $process.Kill()
            $process.WaitForExit()
            $status = "TIMEOUT"
        } else {
            $exitCode = $process.ExitCode
            $status = if ($exitCode -eq 0) { "PASS" } elseif ($exitCode -eq 77) { "SKIP" } else { "FAIL" }
        }
        $process.Dispose()
    } catch {
        $status = "ERROR"
        $_ | Out-File $stderr -Encoding utf8
    }
    $elapsed.Stop()
    $script:results += New-Object PSObject -Property @{ Test=$Name; Status=$status; ExitCode=$exitCode; Milliseconds=$elapsed.ElapsedMilliseconds }
    $script:results | Select-Object Test,Status,ExitCode,Milliseconds |
        Export-Csv (Join-Path $OutputDirectory "results.csv") -NoTypeInformation -Encoding UTF8
    Write-Output "$Name : $status ($exitCode)"
}

$tests = @(Get-ChildItem -LiteralPath $Bundle | Where-Object { $_.Name -match '^oneui_.*tests\.exe$' } | Sort-Object Name)
if ($tests.Count -lt 20) { throw "Acceptance bundle is incomplete: only $($tests.Count) test executables" }
foreach ($test in $tests) { Run-Test $test.BaseName $test.FullName }
Run-Test "system-clipboard" (Join-Path $Bundle "oneui_backend_contract_tests.exe") "--clipboard"
Run-Test "gallery-smoke" (Join-Path $Bundle "oneui_gallery.exe") "--smoke-test"
if ($CaptureFrames) {
    foreach ($scale in @("1", "1.25", "1.5", "2")) {
        foreach ($renderer in @("default", "raster")) {
            $env:ONEUI_TEST_DPI_SCALE = $scale
            $env:ONEUI_ENABLE_GPU = if ($renderer -eq "raster") { "0" } else { "1" }
            $name = "gallery-$renderer-$scale"
            Run-Test $name (Join-Path $Bundle "oneui_gallery.exe") "--capture-frame"
            $frame = Join-Path $Bundle "oneui-gallery.png"
            if (Test-Path -LiteralPath $frame) {
                Move-Item -LiteralPath $frame -Destination (Join-Path $OutputDirectory ($name + ".png"))
            } else {
                $script:results += New-Object PSObject -Property @{ Test=($name + "-image"); Status="FAIL"; ExitCode=-1; Milliseconds=0 }
            }
        }
    }
}
$script:results | Select-Object Test,Status,ExitCode,Milliseconds |
    Export-Csv (Join-Path $OutputDirectory "results.csv") -NoTypeInformation -Encoding UTF8
$finalExitCode = if (@($script:results | Where-Object { $_.Status -ne "PASS" }).Count -gt 0) { 1 } else { 0 }
Write-Output "ONEUI_ACCEPTANCE_COMPLETE exit=$finalExitCode count=$($script:results.Count)"
# PS 2.0 can retain Start-Process redirected-stream worker threads after all
# child handles are disposed. The CSV and streams above have already closed;
# terminate this dedicated test-runner process rather than hang guestcontrol.
[Environment]::Exit($finalExitCode)
