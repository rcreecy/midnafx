param([string]$Name,[bool]$Reflections=$true,[switch]$WaitForExit)
$trial="C:/dev/midnafx/build/water-reflections/$Name"
New-Item -ItemType Directory -Path "$trial/mods","$trial/user","$trial/logs" -Force | Out-Null
$trialConfig=Get-Content -Raw 'C:/dev/midnafx/build/movement-camera/route-on/user/config.json' | ConvertFrom-Json -AsHashtable
foreach($key in @('camera_enabled','movement_camera','contact_shadows','color_bounce','anti_aliasing')) {
    $trialConfig["mod.com_midnafx_midnafx.$key"]=$false
}
$trialConfig['mod.com_midnafx_midnafx.motion_clarity']=$true
$trialConfig['mod.com_midnafx_midnafx.enhanced_water']=$true
$trialConfig['mod.com_midnafx_midnafx.water_scene_reflections']=$Reflections
$trialConfig['game.enableTouchControls']=$false
$trialConfig['game.enableTextureReplacements']=$false
$trialConfig | ConvertTo-Json -Depth 8 | Set-Content "$trial/user/config.json"
Copy-Item 'C:/dev/midnafx/build/windows-release/mods/midnafx.dusk' "$trial/mods/midnafx.dusk"
Copy-Item 'C:/dev/midnafx/build/movement-camera/driver-build/mods/movement_route.dusk' "$trial/mods/movement_route.dusk"
$trialArgs=@('--backend','d3d12','--log-level','debug','--log-dir',('"'+$trial+'/logs"'),'--user-dir',('"'+$trial+'/user"'),'--mods',('"'+$trial+'/mods"'),'--stage','F_SP103,0,27,0','"C:/dev/Dusklight_Runtime/Legend of Zelda, The - Twilight Princess (USA).rvz"')
$priorRoute=$env:MIDNAFX_MOVEMENT_ROUTE_FILE
try {
    $env:MIDNAFX_MOVEMENT_ROUTE_FILE="$trial/route.csv"
    $trialProcess=Start-Process 'C:/dev/midnafx/build/frame-timing/hosts/baseline/dusklight.exe' -ArgumentList $trialArgs -WorkingDirectory $trial -RedirectStandardOutput "$trial/stdout.log" -RedirectStandardError "$trial/stderr.log" -PassThru
} finally { $env:MIDNAFX_MOVEMENT_ROUTE_FILE=$priorRoute }
@{pid=$trialProcess.Id;start=(Get-Date).ToUniversalTime().ToString('o');arguments=$trialArgs;package_sha256=(Get-FileHash "$trial/mods/midnafx.dusk").Hash;driver_sha256=(Get-FileHash "$trial/mods/movement_route.dusk").Hash;host_sha256=(Get-FileHash 'C:/dev/midnafx/build/frame-timing/hosts/baseline/dusklight.exe').Hash} | ConvertTo-Json -Depth 5 | Set-Content "$trial/session.json"
$trialProcess.Id
if ($WaitForExit) {
    $trialHandle=$trialProcess.Handle
    $trialProcess.WaitForExit()
    @{exit_code=$trialProcess.ExitCode} | ConvertTo-Json | Set-Content "$trial/exit.json"
    "Exit=$($trialProcess.ExitCode)"
}
