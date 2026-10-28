param([string]$Name, [bool]$Movement = $true, [bool]$Driver = $true)
$trial="C:/dev/midnafx/build/movement-camera/$Name"
New-Item -ItemType Directory -Path "$trial/mods","$trial/user","$trial/logs" -Force | Out-Null
$routeConfig=Get-Content -Raw 'C:/dev/midnafx/build/movement-camera/route-on/user/config.json' | ConvertFrom-Json -AsHashtable
$routeConfig['game.enableTouchControls']=$false
$routeConfig['mod.com_midnafx_midnafx.movement_camera']=$Movement
$routeConfig | ConvertTo-Json -Depth 8 | Set-Content "$trial/user/config.json"
Copy-Item 'C:/dev/midnafx/build/windows-release/mods/midnafx.dusk' "$trial/mods/midnafx.dusk"
if ($Driver) { Copy-Item 'C:/dev/midnafx/build/movement-camera/driver-build/mods/movement_route.dusk' "$trial/mods/movement_route.dusk" }
$priorRoute=$env:MIDNAFX_MOVEMENT_ROUTE_FILE
$env:MIDNAFX_MOVEMENT_ROUTE_FILE="$trial/route.csv"
$routeArgs=@('--backend','d3d12','--log-level','debug','--log-dir',('"'+$trial+'/logs"'),'--user-dir',('"'+$trial+'/user"'),'--mods',('"'+$trial+'/mods"'),'--stage','F_SP103,0,27,0','"C:/dev/Dusklight_Runtime/Legend of Zelda, The - Twilight Princess (USA).rvz"')
$routeProcess=Start-Process 'C:/dev/midnafx/build/frame-timing/hosts/baseline/dusklight.exe' -ArgumentList $routeArgs -WorkingDirectory $trial -RedirectStandardOutput "$trial/stdout.log" -RedirectStandardError "$trial/stderr.log" -PassThru
$env:MIDNAFX_MOVEMENT_ROUTE_FILE=$priorRoute
@{pid=$routeProcess.Id;start=(Get-Date).ToUniversalTime().ToString('o');arguments=$routeArgs;package_sha256=(Get-FileHash "$trial/mods/midnafx.dusk").Hash;driver=$Driver} | ConvertTo-Json -Depth 5 | Set-Content "$trial/session.json"
$routeProcess.Id
