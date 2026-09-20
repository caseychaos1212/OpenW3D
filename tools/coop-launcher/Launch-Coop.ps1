[CmdletBinding()]
param(
    [ValidateSet('Menu','Host','Join','Configure')]
    [string]$Mode = 'Menu',
    [string]$GameDirectory,
    [string]$PlayerName,
    [string]$Server,
    [string]$Mission = 'M01.mix',
    [ValidateRange(1,65535)][int]$Port = 4848,
    [switch]$DryRun
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
# Windows PowerShell does not wait for GUI executables invoked with &, so it
# may never set LASTEXITCODE. Own the process and read its exit code after waiting.
function Invoke-CoopProgram([string]$Name, [string[]]$ProgramArguments) {
    # ProcessStartInfo.ArgumentList is unavailable in Windows PowerShell 5.1.
    # Quote for Windows argv parsing, including embedded quotes/trailing slashes.
    $quoted = foreach ($argument in $ProgramArguments) {
        $escaped = [regex]::Replace($argument, '(\\*)"', '$1$1\"')
        $escaped = [regex]::Replace($escaped, '(\\+)$', '$1$1')
        '"' + $escaped + '"'
    }
    $start = [Diagnostics.ProcessStartInfo]::new()
    $start.FileName = Join-Path $PSScriptRoot $Name
    $start.WorkingDirectory = $PSScriptRoot
    $start.UseShellExecute = $false
    $start.Arguments = $quoted -join ' '
    $process = [Diagnostics.Process]::Start($start)
    try {
        $process.WaitForExit()
        return $process.ExitCode
    } finally { $process.Dispose() }
}

try {
    $settingsFile = Join-Path $PSScriptRoot 'launcher-settings.json'
    $configFile = Join-Path $PSScriptRoot 'openw3d.conf'
    $saved = $null
    if (Test-Path -LiteralPath $settingsFile -PathType Leaf) {
        $saved = Get-Content -LiteralPath $settingsFile -Raw | ConvertFrom-Json
    }
    if ($Mode -eq 'Configure') {
        if ($DryRun) { Write-Output 'Configuration launcher OK'; exit 0 }
        exit (Invoke-CoopProgram 'wwconfig.exe' @('--ini',$configFile))
    }
    if (-not $GameDirectory -and $null -ne $saved) { $GameDirectory = $saved.GameDirectory }
    if (-not $GameDirectory -and (Test-Path -LiteralPath (Join-Path $PSScriptRoot 'Data/always.dat'))) {
        $GameDirectory = $PSScriptRoot
    }
    if (-not $GameDirectory) {
        if ($DryRun) { throw 'Supply -GameDirectory for the launcher check.' }
        $GameDirectory = (Read-Host 'Renegade installation folder (the folder containing Data)').Trim().Trim('"')
    }
    $GameDirectory = (Resolve-Path -LiteralPath $GameDirectory).Path
    if (-not (Test-Path -LiteralPath (Join-Path $GameDirectory 'Data/always.dat') -PathType Leaf)) {
        throw 'That folder does not contain Data/always.dat. Select a complete Renegade installation.'
    }
    if (-not $PlayerName -and $null -ne $saved) { $PlayerName = $saved.PlayerName }
    if (-not $PlayerName) {
        if ($DryRun) { $PlayerName = 'Player' }
        else { $PlayerName = Read-Host 'Your player name (different for each player)' }
    }
    if ([string]::IsNullOrWhiteSpace($PlayerName)) { throw 'A player name is required.' }
    if ($Mode -eq 'Join' -and -not $Server) {
        if ($DryRun) { throw 'Supply -Server for the join launcher check.' }
        $Server = Read-Host 'Host IP address (optionally IP:port)'
    }
    $launchArgs = @('--gamedir',$GameDirectory,'--ini',$configFile,'--multi','--netplayername',$PlayerName)
    if ($Mode -eq 'Host') { $launchArgs += @('--coop-host',$Mission,'--coop-port',"$Port") }
    if ($Mode -eq 'Join') { $launchArgs += @('--coop-connect',$Server) }
    if ($DryRun) {
        [ordered]@{Executable=(Join-Path $PSScriptRoot 'renegade.exe');Arguments=$launchArgs} | ConvertTo-Json -Depth 3
        exit 0
    }
    [ordered]@{GameDirectory=$GameDirectory;PlayerName=$PlayerName} |
        ConvertTo-Json | Set-Content -LiteralPath $settingsFile -Encoding UTF8
    if (-not (Test-Path -LiteralPath $configFile -PathType Leaf)) {
        $configExitCode = Invoke-CoopProgram 'wwconfig.exe' @('--ini',$configFile)
        if ($configExitCode -ne 0) { exit $configExitCode }
        if (-not (Test-Path -LiteralPath $configFile -PathType Leaf)) {
            throw 'Save your video/audio settings before starting the game.'
        }
    }
    exit (Invoke-CoopProgram 'renegade.exe' $launchArgs)
} catch {
    Write-Host $_.Exception.Message -ForegroundColor Red
    exit 1
}
