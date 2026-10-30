param([string]$Name='native-final',[switch]$WaitForExit)
$ErrorActionPreference='Stop'
$trial="C:/dev/midnafx/build/local-lights/$Name"
New-Item -ItemType Directory -Path "$trial/mods","$trial/user","$trial/logs" -Force | Out-Null
$trialConfig=Get-Content -Raw 'C:/dev/midnafx/build/surface-lighting/native-compare/user/config.json' | ConvertFrom-Json -AsHashtable
foreach($key in @('local_lights','ambient_occlusion','contact_shadows','color_bounce','camera_enabled','movement_camera','anti_aliasing','enhanced_water','water_scene_reflections','adaptive_exposure','bloom','atmosphere_enabled','sunlight_shafts','geometry_smoothing','geometry_skinned_smoothing','geometry_diagnostics','topology_diagnostics','grading_enabled','detail_enabled')) {
    $trialConfig["mod.com_midnafx_midnafx.$key"]=$false
}
$trialConfig['mod.com_midnafx_midnafx.motion_clarity']=$true
$trialConfig['mod.com_midnafx_midnafx.local_lights_strength']=15
$trialConfig['backend.enableAdvancedSettings']=$true
$trialConfig['game.enableTextureReplacements']=$false
$trialConfig | ConvertTo-Json -Depth 8 | Set-Content "$trial/user/config.json"
Copy-Item -LiteralPath 'C:/dev/midnafx/build/windows-release/mods/midnafx.dusk' -Destination "$trial/mods/midnafx.dusk"
Copy-Item -LiteralPath 'C:/dev/midnafx/build/local-lights/driver-build/mods/local_light_audit.dusk' -Destination "$trial/mods/local_light_audit.dusk"
$trialArgs=@('--backend','d3d12','--log-level','debug','--log-dir',('"'+$trial+'/logs"'),'--user-dir',('"'+$trial+'/user"'),'--mods',('"'+$trial+'/mods"'),'--stage','F_SP103,0,27,0','"C:/dev/Dusklight_Runtime/Legend of Zelda, The - Twilight Princess (USA).rvz"')
$trialProcess=Start-Process 'C:/dev/midnafx/build/frame-timing/hosts/baseline/dusklight.exe' -ArgumentList $trialArgs -WorkingDirectory $trial -RedirectStandardOutput "$trial/stdout.log" -RedirectStandardError "$trial/stderr.log" -PassThru
@{pid=$trialProcess.Id;start=(Get-Date).ToUniversalTime().ToString('o');arguments=$trialArgs;package_sha256=(Get-FileHash "$trial/mods/midnafx.dusk").Hash;fixture_sha256=(Get-FileHash "$trial/mods/local_light_audit.dusk").Hash;host_sha256=(Get-FileHash 'C:/dev/midnafx/build/frame-timing/hosts/baseline/dusklight.exe').Hash} | ConvertTo-Json -Depth 5 | Set-Content "$trial/session.json"
$trialProcess.Id
if ($WaitForExit) {
    $trialHandle=$trialProcess.Handle
    $trialProcess.WaitForExit()
    @{exit_code=$trialProcess.ExitCode} | ConvertTo-Json | Set-Content "$trial/exit.json"
    "Exit=$($trialProcess.ExitCode)"
}
