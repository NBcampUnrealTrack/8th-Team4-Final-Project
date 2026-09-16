param(
    [string]$EditorPath = 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe',
    # Killcam only runs where NetMode == NM_Client, so only the victim has to be a client.
    # The listen-server host can be the killer: -Roles Host,Victim runs the whole test with
    # two processes instead of three, which matters on one GPU at map load.
    [string[]]$Roles = @('Host', 'Victim', 'Killer'),
    [switch]$DryRun
)

$ErrorActionPreference = 'Stop'

# powershell.exe -File passes "Host,Victim" as one literal string instead of an array,
# so split on commas here. That keeps both invocation forms working:
#   powershell -File Start-LocalKillcamTest.ps1 -Roles Host,Victim
#   .\Start-LocalKillcamTest.ps1 -Roles Host,Victim
$KnownRoles = @('Host', 'Victim', 'Killer')
$Roles = @($Roles | ForEach-Object { $_ -split ',' } | ForEach-Object { $_.Trim() } | Where-Object { $_ })
if ($Roles.Count -eq 0) {
    throw ('No roles selected. Valid roles: {0}' -f ($KnownRoles -join ', '))
}
foreach ($Role in $Roles) {
    if ($KnownRoles -notcontains $Role) {
        throw ("Unknown role '{0}'. Valid roles: {1}" -f $Role, ($KnownRoles -join ', '))
    }
}
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$ProjectPath = Join-Path $ProjectRoot 'FPS.uproject'

foreach ($Path in @($EditorPath, $ProjectPath)) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "File not found: $Path"
    }
}

$CommonArguments = @(
    ('"{0}"' -f $ProjectPath),
    '/Game/Maps/StartupMap',
    '-game', '-windowed', '-ResX=960', '-ResY=540', '-log', '-nosteam', '-NoSaveConfig',
    '-ini:Engine:[OnlineSubsystem]:DefaultPlatformService=NULL',
    '-ini:Engine:[OnlineSubsystemSteam]:bEnabled=False',
    '-ini:Engine:[/Script/Engine.GameEngine]:NetDriverDefinitions=(DefName=GameNetDriver,DriverClassName=/Script/OnlineSubsystemUtils.IpNetDriver,DriverClassNameFallback=/Script/OnlineSubsystemUtils.IpNetDriver)',
    '-ini:Engine:[/Script/Engine.GameEngine]:+NetDriverDefinitions=(DefName=DemoNetDriver,DriverClassName=/Script/Engine.DemoNetDriver,DriverClassNameFallback=/Script/Engine.DemoNetDriver)',
    '-ini:Engine:[/Script/Engine.GameEngine]:+NetDriverDefinitions=(DefName=BeaconNetDriver,DriverClassName=/Script/OnlineSubsystemUtils.IpNetDriver,DriverClassNameFallback=/Script/OnlineSubsystemUtils.IpNetDriver)'
)

$Windows = @(
    @{ Role = 'Host'; X = 0; Y = 0 },
    @{ Role = 'Victim'; X = 960; Y = 0 },
    @{ Role = 'Killer'; X = 0; Y = 580 }
) | Where-Object { $Roles -contains $_.Role }

foreach ($Window in $Windows) {
    $Arguments = $CommonArguments + @(
        ('-WinX={0}' -f $Window.X),
        ('-WinY={0}' -f $Window.Y),
        ('-Log=LocalKillcam-{0}.log' -f $Window.Role)
    )
    if ($DryRun) {
        Write-Output ($EditorPath + ' ' + ($Arguments -join ' '))
    }
    else {
        $Process = Start-Process -FilePath $EditorPath -ArgumentList ($Arguments -join ' ') -WorkingDirectory $ProjectRoot -WindowStyle Normal -PassThru
        Write-Output ('{0}: PID {1}' -f $Window.Role, $Process.Id)
        Start-Sleep -Seconds 2
    }
}

if ($Roles -contains 'Killer') {
    Write-Output 'Host: create a room. Victim and Killer: use Local Join after the host reaches Lobby.'
    Write-Output 'Choose opposing teams for Victim and Killer, ready both clients, then start on Host.'
}
else {
    Write-Output 'Host: create a room, then Victim: use Local Join after the host reaches Lobby.'
    Write-Output 'Put Host and Victim on opposing teams. The host plays the killer; killcam runs on Victim.'
}
Write-Output ('Logs: ' + (($Windows | ForEach-Object { 'Saved/Logs/LocalKillcam-{0}.log' -f $_.Role }) -join ', '))
