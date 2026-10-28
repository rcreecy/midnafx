param([string]$Name)
$trial="C:/dev/midnafx/build/color-bounce/$Name"
New-Item -ItemType Directory -Path "$trial/mods","$trial/user","$trial/logs" -Force | Out-Null
$trialConfig=Get-Content -Raw 'C:/dev/midnafx/build/movement-camera/route-on/user/config.json' | ConvertFrom-Json -AsHashtable
foreach($key in @('camera_enabled','movement_camera','anti_aliasing','enhanced_water','contact_shadows','color_bounce')) {
    $trialConfig["mod.com_midnafx_midnafx.$key"]=$false
}
$trialConfig['mod.com_midnafx_midnafx.color_bounce_strength']=12
$trialConfig['game.enableTouchControls']=$false
$trialConfig['game.enableTextureReplacements']=$false
$trialConfig | ConvertTo-Json -Depth 8 | Set-Content "$trial/user/config.json"
Copy-Item 'C:/dev/midnafx/build/windows-release/mods/midnafx.dusk' "$trial/mods/midnafx.dusk"
$trialArgs=@('--backend','d3d12','--log-level','debug','--log-dir',('"'+$trial+'/logs"'),'--user-dir',('"'+$trial+'/user"'),'--mods',('"'+$trial+'/mods"'),'--stage','F_SP103,0,27,0','"C:/dev/Dusklight_Runtime/Legend of Zelda, The - Twilight Princess (USA).rvz"')
$trialProcess=Start-Process 'C:/dev/midnafx/build/frame-timing/hosts/baseline/dusklight.exe' -ArgumentList $trialArgs -WorkingDirectory $trial -RedirectStandardOutput "$trial/stdout.log" -RedirectStandardError "$trial/stderr.log" -PassThru
@{pid=$trialProcess.Id;start=(Get-Date).ToUniversalTime().ToString('o');arguments=$trialArgs;package_sha256=(Get-FileHash "$trial/mods/midnafx.dusk").Hash;host_sha256=(Get-FileHash 'C:/dev/midnafx/build/frame-timing/hosts/baseline/dusklight.exe').Hash} | ConvertTo-Json -Depth 5 | Set-Content "$trial/session.json"
$trialProcess.Id
