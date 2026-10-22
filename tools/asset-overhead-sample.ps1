# Call only after visually checking and activating the intended game window.
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$RunDirectory,
    [switch]$VisualCheckConfirmed
)
$ErrorActionPreference='Stop'
if (!$VisualCheckConfirmed) { throw 'A visual scene check is required before sampling' }
$RunDirectory=(Resolve-Path -LiteralPath $RunDirectory).Path
$session=Get-Content -LiteralPath "$RunDirectory/session.json" -Raw -Encoding utf8 | ConvertFrom-Json
if (Test-Path -LiteralPath "$RunDirectory/process-samples.json") { throw 'Samples already exist' }
if ($session.host_executable_name -ine 'dusklight.exe' -or @($session.host_images).Count -ne 1) {
    throw 'Verified host identity is required before sampling'
}
$process=Get-Process -Id $session.pid
function Get-VerifiedImages {
    $process.Refresh()
    if ($process.HasExited) { throw 'Profile host exited during collection' }
    $images=@($process.Modules | Where-Object { $_.ModuleName -match '^dusklight.*\.exe$' } | ForEach-Object {
        @{name=$_.ModuleName;sha256=(Get-FileHash -LiteralPath $_.FileName).Hash}
    })
    if ($images.Count -ne 1 -or $images[0].name -ine 'dusklight.exe' -or $images[0].sha256 -ne $session.host_sha256) {
        throw 'Loaded host identity changed; discard this trial'
    }
    return $images
}
$before=@(Get-VerifiedImages)
Start-Sleep -Seconds 20
$clock=[Diagnostics.Stopwatch]::StartNew()
$process.Refresh()
$rows=@(@{seconds=0;cpu_seconds=$process.TotalProcessorTime.TotalSeconds;private_bytes=$process.PrivateMemorySize64;working_set_bytes=$process.WorkingSet64;handles=$process.HandleCount;threads=$process.Threads.Count})
for ($index=1; $index -le 30; $index++) {
    Start-Sleep -Seconds 1
    $process.Refresh()
    if ($process.HasExited) { throw 'Profile host exited during sampling' }
    $rows+=@{seconds=$clock.Elapsed.TotalSeconds;cpu_seconds=$process.TotalProcessorTime.TotalSeconds;private_bytes=$process.PrivateMemorySize64;working_set_bytes=$process.WorkingSet64;handles=$process.HandleCount;threads=$process.Threads.Count}
}
$after=@(Get-VerifiedImages)
@{name=$session.name;variant=$session.variant;warmup_after_visual_check_seconds=20;samples=$rows;host_images_before=$before;host_images_after=$after;visual_check_confirmed=$true} | ConvertTo-Json -Depth 8 | Set-Content -Encoding utf8 -LiteralPath "$RunDirectory/process-samples.json"
Write-Output "Completed $($session.name)"
