param([int]$Runs=3,[int]$Seconds=5)
$ErrorActionPreference='Stop'
$exe=Join-Path $PSScriptRoot 'build/bin/oneui-declarative-demo.exe'
$results=Join-Path $PSScriptRoot 'artifacts/benchmarks'
New-Item -ItemType Directory -Force $results | Out-Null
foreach($scenario in @('idle','updates')) {
    for($run=1;$run -le $Runs;$run++) {
        $entries=if($run % 2){@('template','code')}else{@('code','template')}
        foreach($entry in $entries) {
            $output=Join-Path $results "$scenario-$entry-$run.json"
            $arguments=@('--page','list','--scenario',$scenario,'--benchmark-seconds',"$Seconds",'--output',$output)
            if($entry -eq 'code'){$arguments+='--code'}
            & $exe @arguments
            if($LASTEXITCODE -ne 0){throw "Benchmark failed: $scenario $entry"}
        }
    }
}
