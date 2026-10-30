param([string]$Name='native-final',[switch]$WaitForExit)
$ErrorActionPreference='Stop'
$trial="C:/dev/midnafx/build/multiscale-bloom/$Name"
New-Item -ItemType Directory -Path "$trial/mods","$trial/user","$trial/logs" -Force | Out-Null
$trialConfig=Get-Content -Raw 'C:/dev/midnafx/build/lantern-light/native-final/user/config.json' | ConvertFrom-Json -AsHashtable
$trialConfig['mod.com_midnafx_midnafx.local_lights']=$false
$trialConfig['mod.com_midnafx_midnafx.motion_clarity']=$false
$trialConfig['mod.com_midnafx_midnafx.bloom']=$true
$trialConfig['mod.com_midnafx_midnafx.bloom_strength']=20
$trialConfig['mod.com_midnafx_midnafx.profile_custom_data']=''
$trialConfig['mod.com_midnafx_midnafx.diagnostics']=$false
$trialConfig | ConvertTo-Json -Depth 8 | Set-Content "$trial/user/config.json"
Copy-Item -LiteralPath 'C:/dev/midnafx/build/windows-release/mods/midnafx.dusk' -Destination "$trial/mods/midnafx.dusk"
$trialArgs=@('--backend','d3d12','--log-level','debug','--log-dir',('"'+$trial+'/logs"'),'--user-dir',('"'+$trial+'/user"'),'--mods',('"'+$trial+'/mods"'),'--stage','F_SP103,0,27,0','"C:/dev/Dusklight_Runtime/Legend of Zelda, The - Twilight Princess (USA).rvz"')
$trialProcess=Start-Process 'C:/dev/midnafx/build/frame-timing/hosts/baseline/dusklight.exe' -ArgumentList $trialArgs -WorkingDirectory $trial -RedirectStandardOutput "$trial/stdout.log" -RedirectStandardError "$trial/stderr.log" -PassThru
@{pid=$trialProcess.Id;start=(Get-Date).ToUniversalTime().ToString('o');arguments=$trialArgs;initial_config=$trialConfig;package_sha256=(Get-FileHash "$trial/mods/midnafx.dusk").Hash;host_sha256=(Get-FileHash 'C:/dev/midnafx/build/frame-timing/hosts/baseline/dusklight.exe').Hash} | ConvertTo-Json -Depth 8 | Set-Content "$trial/session.json"
$trialProcess.Id
if ($WaitForExit) {
    $trialHandle=$trialProcess.Handle
    $trialProcess.WaitForExit()
    @{exit_code=$trialProcess.ExitCode} | ConvertTo-Json | Set-Content "$trial/exit.json"
    "Exit=$($trialProcess.ExitCode)"
}
