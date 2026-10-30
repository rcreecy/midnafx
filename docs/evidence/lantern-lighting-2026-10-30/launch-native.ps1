param([string]$Name='native-final',[switch]$WaitForExit)
$ErrorActionPreference='Stop'
$trial="C:/dev/midnafx/build/lantern-light/$Name"
New-Item -ItemType Directory -Path "$trial/mods","$trial/user","$trial/logs" -Force | Out-Null
$trialConfig=Get-Content -Raw 'C:/dev/midnafx/build/local-lights/native-final/user/config.json' | ConvertFrom-Json -AsHashtable
$trialConfig['mod.com_midnafx_midnafx.local_lights']=$true
$trialConfig['mod.com_midnafx_midnafx.local_lights_strength']=15
$trialConfig | ConvertTo-Json -Depth 8 | Set-Content "$trial/user/config.json"
Copy-Item -LiteralPath 'C:/dev/midnafx/build/windows-release/mods/midnafx.dusk' -Destination "$trial/mods/midnafx.dusk"
Copy-Item -LiteralPath 'C:/dev/midnafx/build/lantern-light/driver-build/mods/lantern_audit.dusk' -Destination "$trial/mods/lantern_audit.dusk"
$trialArgs=@('--backend','d3d12','--log-level','debug','--log-dir',('"'+$trial+'/logs"'),'--user-dir',('"'+$trial+'/user"'),'--mods',('"'+$trial+'/mods"'),'--stage','F_SP103,0,27,0','"C:/dev/Dusklight_Runtime/Legend of Zelda, The - Twilight Princess (USA).rvz"')
$env:MIDNAFX_LANTERN_AUDIT_FILE="$trial/observations.csv"
try {
    $trialProcess=Start-Process 'C:/dev/midnafx/build/frame-timing/hosts/baseline/dusklight.exe' -ArgumentList $trialArgs -WorkingDirectory $trial -RedirectStandardOutput "$trial/stdout.log" -RedirectStandardError "$trial/stderr.log" -PassThru
} finally {
    Remove-Item Env:MIDNAFX_LANTERN_AUDIT_FILE -ErrorAction SilentlyContinue
}
@{pid=$trialProcess.Id;start=(Get-Date).ToUniversalTime().ToString('o');arguments=$trialArgs;package_sha256=(Get-FileHash "$trial/mods/midnafx.dusk").Hash;fixture_sha256=(Get-FileHash "$trial/mods/lantern_audit.dusk").Hash;host_sha256=(Get-FileHash 'C:/dev/midnafx/build/frame-timing/hosts/baseline/dusklight.exe').Hash} | ConvertTo-Json -Depth 5 | Set-Content "$trial/session.json"
$trialProcess.Id
if ($WaitForExit) {
    $trialHandle=$trialProcess.Handle
    $trialProcess.WaitForExit()
    @{exit_code=$trialProcess.ExitCode} | ConvertTo-Json | Set-Content "$trial/exit.json"
    "Exit=$($trialProcess.ExitCode)"
}
