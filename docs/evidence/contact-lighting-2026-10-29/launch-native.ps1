param([string]$Name='native-compare',[switch]$WaitForExit)
$trial="C:/dev/midnafx/build/surface-lighting/$Name"
New-Item -ItemType Directory -Path "$trial/mods","$trial/user","$trial/logs" -Force | Out-Null
$trialConfig=Get-Content -Raw 'C:/dev/midnafx/build/rigid-shading/final-resident/user/config.json' | ConvertFrom-Json -AsHashtable
foreach($key in @('camera_enabled','movement_camera','anti_aliasing','enhanced_water','water_scene_reflections','adaptive_exposure','bloom','atmosphere_enabled','sunlight_shafts','geometry_smoothing','geometry_skinned_smoothing','geometry_diagnostics','topology_diagnostics','grading_enabled','detail_enabled')) {
    $trialConfig["mod.com_midnafx_midnafx.$key"]=$false
}
foreach($key in @('ambient_occlusion','contact_shadows','color_bounce','motion_clarity')) {
    $trialConfig["mod.com_midnafx_midnafx.$key"]=$true
}
$trialConfig['backend.enableAdvancedSettings']=$true
$trialConfig['game.enableTextureReplacements']=$false
$trialConfig | ConvertTo-Json -Depth 8 | Set-Content "$trial/user/config.json"
$baselinePackage='C:/dev/midnafx/build/releases/v1.15.0/midnafx-windows-amd64.dusk'
Copy-Item -LiteralPath $baselinePackage -Destination "$trial/mods/midnafx.dusk"
$trialArgs=@('--backend','d3d12','--log-level','debug','--log-dir',('"'+$trial+'/logs"'),'--user-dir',('"'+$trial+'/user"'),'--mods',('"'+$trial+'/mods"'),'--stage','F_SP103,0,27,0','"C:/dev/Dusklight_Runtime/Legend of Zelda, The - Twilight Princess (USA).rvz"')
$trialProcess=Start-Process 'C:/dev/midnafx/build/frame-timing/hosts/baseline/dusklight.exe' -ArgumentList $trialArgs -WorkingDirectory $trial -RedirectStandardOutput "$trial/stdout.log" -RedirectStandardError "$trial/stderr.log" -PassThru
@{pid=$trialProcess.Id;start=(Get-Date).ToUniversalTime().ToString('o');arguments=$trialArgs;baseline_package_sha256=(Get-FileHash $baselinePackage).Hash;host_sha256=(Get-FileHash 'C:/dev/midnafx/build/frame-timing/hosts/baseline/dusklight.exe').Hash} | ConvertTo-Json -Depth 5 | Set-Content "$trial/session.json"
$trialProcess.Id
if ($WaitForExit) {
    $trialHandle=$trialProcess.Handle
    $trialProcess.WaitForExit()
    @{exit_code=$trialProcess.ExitCode} | ConvertTo-Json | Set-Content "$trial/exit.json"
    "Exit=$($trialProcess.ExitCode)"
}
