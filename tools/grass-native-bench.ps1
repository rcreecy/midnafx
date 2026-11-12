param(
    [Parameter(Mandatory=$true)][string]$ReferencePackage,
    [string]$CandidatePackage,
    [Parameter(Mandatory=$true)][string]$HostPath,
    [Parameter(Mandatory=$true)][string]$GamePath,
    [Parameter(Mandatory=$true)][string]$UserConfig,
    [Parameter(Mandatory=$true)][string]$OutputDirectory,
    [string]$Scene = 'F_SP121,6,2,4',
    [ValidateRange(1,10)][int]$PairCount = 3,
    [ValidateRange(0,320)][int]$WarmupSamples = 128,
    [ValidateRange(30,55)][int]$Seconds = 40
)
$ErrorActionPreference = 'Stop'
$reference = (Resolve-Path -LiteralPath $ReferencePackage).Path
$candidate = if ($CandidatePackage) { (Resolve-Path -LiteralPath $CandidatePackage).Path } else { $null }
$hostExe = (Resolve-Path -LiteralPath $HostPath).Path
$game = (Resolve-Path -LiteralPath $GamePath).Path
$config = (Resolve-Path -LiteralPath $UserConfig).Path
$outputRoot = [System.IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $outputRoot) { throw 'Use a new output directory to preserve existing trials.' }
New-Item -ItemType Directory -Path $outputRoot | Out-Null
$metadata = [ordered]@{
    schema=1; scene=$Scene; backend='d3d12'; seconds=$Seconds; warmup_samples=$WarmupSamples
    host_sha256=(Get-FileHash -LiteralPath $hostExe -Algorithm SHA256).Hash
    config_sha256=(Get-FileHash -LiteralPath $config -Algorithm SHA256).Hash
    reference_sha256=(Get-FileHash -LiteralPath $reference -Algorithm SHA256).Hash
    candidate_sha256=$(if ($candidate) { (Get-FileHash -LiteralPath $candidate -Algorithm SHA256).Hash } else { $null })
    cpu=(Get-CimInstance Win32_Processor).Name
    gpu=@(Get-CimInstance Win32_VideoController | Select-Object Name,DriverVersion)
    power_policy=(powercfg /getactivescheme); runs=@()
}
$order = if ($candidate) {
    @(for ($pair=0; $pair -lt $PairCount; ++$pair) { if ($pair%2) { 'B'; 'A' } else { 'A'; 'B' } })
} else { @('A') }
foreach ($variant in $order) {
    $number = $metadata.runs.Count
    $runRoot = Join-Path $outputRoot ('{0:d2}-{1}' -f $number,$variant)
    New-Item -ItemType Directory -Path "$runRoot/user","$runRoot/mods","$runRoot/logs" | Out-Null
    Copy-Item -LiteralPath $config -Destination "$runRoot/user/config.json"
    $package = if ($variant -eq 'A') { $reference } else { $candidate }
    Copy-Item -LiteralPath $package -Destination "$runRoot/mods/midnafx.dusk"
    $gameArgs = @('--backend','d3d12','--log-level','info','--log-dir',('"'+$runRoot+'/logs"'),
        '--user-dir',('"'+$runRoot+'/user"'),'--mods',('"'+$runRoot+'/mods"'),'--stage',$Scene,
        '--cvar','mod.com_midnafx_midnafx.diagnostics=true',
        '--cvar','mod.com_midnafx_midnafx.topology_diagnostics=false',
        '--cvar','mod.com_midnafx_midnafx.water_classification_diagnostic=false',
        '--cvar','mod.com_midnafx_midnafx.water_scene_capture_diagnostic=false',('"'+$game+'"'))
    $started = [DateTime]::UtcNow
    $process = Start-Process -FilePath $hostExe -ArgumentList $gameArgs -WorkingDirectory (Split-Path $hostExe) `
        -WindowStyle Hidden -RedirectStandardOutput "$runRoot/stdout.log" -RedirectStandardError "$runRoot/stderr.log" -PassThru
    $record = [ordered]@{variant=$variant; directory=$runRoot; pid=$process.Id; started_utc=$started.ToString('o');
        close_requested=$false; forced=$false; exit_code=$null; elapsed_seconds=$null; profile_samples=0}
    try {
        Start-Sleep -Seconds $Seconds
        $process.Refresh()
        if (!$process.HasExited) {
            $record.close_requested = $process.CloseMainWindow()
            if (!$process.WaitForExit(10000)) {
                # Terminate only the process created by this trial; preserve its failed capture.
                $record.forced=$true
                $process.Kill(); $process.WaitForExit()
            }
        }
        $record.exit_code=$process.ExitCode
        $record.profile_samples = @(Select-String -LiteralPath "$runRoot/stdout.log" -SimpleMatch 'Grass CPU: ').Count
    } finally {
        $record.elapsed_seconds=([DateTime]::UtcNow-$started).TotalSeconds
        $metadata.runs += $record
        $metadata | ConvertTo-Json -Depth 8 | Set-Content -Encoding utf8 (Join-Path $outputRoot 'manifest.json')
    }
    Write-Output "Trial $number $variant exit=$($record.exit_code) forced=$($record.forced): $runRoot"
    if ($record.forced -or $record.exit_code -ne 0) { throw 'Native trial failed; partial evidence retained.' }
    if ($record.profile_samples -ne 384) { throw 'Incomplete CPU capture; trial retained. Increase the fixed run window.' }
}
