# Launch an isolated Windows observer trial without breaking native mod imports.
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][ValidatePattern('^[a-z0-9][a-z0-9-]*$')][string]$Name,
    [Parameter(Mandatory=$true)][ValidateSet('baseline','patched-off','patched-on')][string]$Variant,
    [Parameter(Mandatory=$true)][string]$HostExecutable,
    [Parameter(Mandatory=$true)][string]$ModPackage,
    [Parameter(Mandatory=$true)][string]$DiscPath,
    [Parameter(Mandatory=$true)][string]$ReplacementRoot,
    [Parameter(Mandatory=$true)][string]$OutputRoot,
    [switch]$PumpkinBodyControl,
    [switch]$DisableReplacements,
    [switch]$FrameTiming
)
$ErrorActionPreference='Stop'
# The SDK import library names dusklight.exe. An alias loads a second host image.
if ((Split-Path -Leaf $HostExecutable) -ine 'dusklight.exe') {
    throw 'Native mod profiling requires executable name dusklight.exe; use separate runtime directories for variants.'
}
if (Get-Process dusklight* -ErrorAction SilentlyContinue) { throw 'Close existing test host first' }
$HostExecutable=(Resolve-Path -LiteralPath $HostExecutable).Path
if (!(Test-Path -LiteralPath (Join-Path (Split-Path $HostExecutable) 'res/FiraSans-Regular.ttf') -PathType Leaf)) {
    throw 'Runtime assets missing; copy the complete runtime directory before profiling'
}
$ModPackage=(Resolve-Path -LiteralPath $ModPackage).Path
$DiscPath=(Resolve-Path -LiteralPath $DiscPath).Path
$ReplacementRoot=(Resolve-Path -LiteralPath $ReplacementRoot).Path
$runRoot=Join-Path ([IO.Path]::GetFullPath($OutputRoot)) $Name
if (Test-Path -LiteralPath $runRoot) { throw 'Profile run exists' }
New-Item -ItemType Directory -Path "$runRoot/user","$runRoot/mods","$runRoot/logs" -Force | Out-Null
$enabled=$Variant -eq 'patched-on'
$runConfig=@{
    'backend.isoPath'=$DiscPath; 'backend.isoVerification'=0
    'backend.wasPresetChosen'=$true; 'backend.enableAdvancedSettings'=$true
    'video.enableFullscreen'=$false; 'video.enableVsync'=$true; 'video.rememberWindowSize'=$true
    'video.lastWindowWidth'=960; 'video.lastWindowHeight'=720; 'game.pauseOnFocusLost'=$false
    'game.autoSave'=$false; 'game.enableControllerToasts'=$false; 'game.enableTextureReplacements'=!$DisableReplacements
    'mod.com_midnafx_midnafx.anti_aliasing'=$true
    'mod.com_midnafx_midnafx.asset_diagnostics'=$enabled
    'mod.com_midnafx_midnafx.asset_draw_diagnostics'=$enabled
    'mod.com_midnafx_midnafx.pumpkin_body_control'=[bool]$PumpkinBodyControl
}
$runConfig | ConvertTo-Json | Set-Content -Encoding utf8 -LiteralPath "$runRoot/user/config.json"
New-Item -ItemType Junction -Path "$runRoot/user/texture_replacements" -Target $ReplacementRoot | Out-Null
Copy-Item -LiteralPath $ModPackage -Destination "$runRoot/mods/midnafx.dusk"
# Start-Process joins this array into a Windows command line; quote path arguments.
$runArgs=@('--backend','d3d12','--log-level','debug','--log-dir',"`"$runRoot/logs`"",
           '--user-dir',"`"$runRoot/user`"",'--mods',"`"$runRoot/mods`"",'--stage','F_SP103,0,27,0',"`"$DiscPath`"")
$previousTimingFile=[Environment]::GetEnvironmentVariable('MIDNAFX_FRAME_TIMING_FILE','Process')
try {
    # Scope this opt-in to the child host. A caller's inherited value must not
    # silently turn an ordinary observer trial into a timing capture.
    $timingFile=if ($FrameTiming) { Join-Path $runRoot 'frame-timing.json' } else { $null }
    [Environment]::SetEnvironmentVariable('MIDNAFX_FRAME_TIMING_FILE',$timingFile,'Process')
    $runProcess=Start-Process -FilePath $HostExecutable -ArgumentList $runArgs -WorkingDirectory (Split-Path $HostExecutable) -WindowStyle Hidden -RedirectStandardOutput "$runRoot/stdout.log" -RedirectStandardError "$runRoot/stderr.log" -PassThru
} finally {
    [Environment]::SetEnvironmentVariable('MIDNAFX_FRAME_TIMING_FILE',$previousTimingFile,'Process')
}
$runSession=@{name=$Name; variant=$Variant; pid=$runProcess.Id; utc_start=[DateTime]::UtcNow.ToString('o')
    host_sha256=(Get-FileHash -LiteralPath $HostExecutable).Hash
    package_sha256=(Get-FileHash -LiteralPath "$runRoot/mods/midnafx.dusk").Hash
    config=$runConfig; arguments=$runArgs; host_executable_name=(Split-Path -Leaf $HostExecutable)
    frame_timing_requested=[bool]$FrameTiming; frame_timing_file=$timingFile}
$runSession | ConvertTo-Json -Depth 8 | Set-Content -Encoding utf8 -LiteralPath "$runRoot/session.json"
$ready=$false
for ($attempt=0; $attempt -lt 120; $attempt++) {
    Start-Sleep -Milliseconds 250
    $runProcess.Refresh()
    if ($runProcess.HasExited) { throw 'Profile host exited before mod initialization' }
    if (Select-String -LiteralPath "$runRoot/stdout.log" -SimpleMatch 'MidnaFX grading, detail, diagnostics, and Twilight prototype initialized' -Quiet) {
        $ready=$true
        break
    }
}
if (!$ready) { throw 'Mod readiness not confirmed; inspect owned process and logs before collecting counters' }
$images=@($runProcess.Modules | Where-Object { $_.ModuleName -match '^dusklight.*\.exe$' } | ForEach-Object {
    @{name=$_.ModuleName; sha256=(Get-FileHash -LiteralPath $_.FileName).Hash; base_address=$_.BaseAddress.ToInt64().ToString('x')}
})
$runSession.host_images=$images
$runSession | ConvertTo-Json -Depth 8 | Set-Content -Encoding utf8 -LiteralPath "$runRoot/session.json"
if ($images.Count -ne 1 -or $images[0].name -ine 'dusklight.exe' -or $images[0].sha256 -ne $runSession.host_sha256) {
    throw 'Invalid host module identity; do not collect profiling counters'
}
Write-Output "PID=$($runProcess.Id) Variant=$Variant Run=$runRoot HostImages=$($images.Count)"
