# Run with Windows PowerShell 5.1, the same host used by the .cmd launchers.
[CmdletBinding()]
param([string]$LauncherDirectory)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if (-not $LauncherDirectory) { $LauncherDirectory = Join-Path $PSScriptRoot '../coop-launcher' }
$LauncherDirectory = (Resolve-Path -LiteralPath $LauncherDirectory).Path
$repo = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$testRoot = Join-Path $repo ('build/launcher-tests/' + [Guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($testRoot) | Out-Null
$stub = Join-Path $testRoot 'GuiStub.exe'
# A GUI-subsystem executable reproduces the process type of the game/settings
# utility without opening a window or requiring installed game data.
Add-Type -OutputAssembly $stub -OutputType WindowsApplication -TypeDefinition @"
using System;
using System.IO;
using System.Reflection;
using System.Threading;
public static class GuiStub {
    public static int Main(string[] args) {
        string dir = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);
        string name = Path.GetFileNameWithoutExtension(Assembly.GetExecutingAssembly().Location);
        Thread.Sleep(300);
        File.WriteAllLines(Path.Combine(dir, name + ".args"), args);
        File.WriteAllText(Path.Combine(dir, name + ".cwd"), Environment.CurrentDirectory);
        string codePath = Path.Combine(dir, name + ".exit");
        int code = File.Exists(codePath) ? Int32.Parse(File.ReadAllText(codePath)) : 0;
        if (name == "wwconfig" && code == 0 && File.Exists(Path.Combine(dir, "save-config"))) {
            int ini = Array.IndexOf(args, "--ini");
            if (ini >= 0 && ini + 1 < args.Length) File.WriteAllText(args[ini + 1], "saved");
        }
        return code;
    }
}
"@
function Assert-Equal($Actual, $Expected, [string]$Message) {
    if ($Actual -cne $Expected) { throw "$Message : expected [$Expected], got [$Actual]" }
}
function Quote-PowerShell([string]$Value) { "'" + $Value.Replace("'", "''") + "'" }
function Invoke-Case {
    param(
        [string]$Name,
        [string]$Mode = 'Menu',
        [switch]$FirstRun,
        [int]$ConfigExit = 0,
        [int]$GameExit = 0,
        [switch]$CancelConfig,
        [int]$ExpectedExit = 0,
        [bool]$ExpectGame = $true,
        [string]$Batch
    )
    $dir = Join-Path $testRoot ($Name + ' & package')
    $gameDir = Join-Path $dir 'Command & Conquer Renegade'
    [IO.Directory]::CreateDirectory((Join-Path $gameDir 'Data')) | Out-Null
    [IO.File]::WriteAllText((Join-Path $gameDir 'Data/always.dat'), 'fixture')
    foreach ($file in @('Launch-Coop.ps1','Play.cmd','Host-Coop.cmd','Join-Coop.cmd','Configure.cmd')) {
        Copy-Item -LiteralPath (Join-Path $LauncherDirectory $file) -Destination (Join-Path $dir $file)
    }
    foreach ($exe in @('renegade','wwconfig')) {
        Copy-Item -LiteralPath $stub -Destination (Join-Path $dir ($exe + '.exe'))
    }
    $config = Join-Path $dir 'openw3d.conf'
    if (-not $FirstRun) { [IO.File]::WriteAllText($config, 'existing') }
    if (-not $CancelConfig) { [IO.File]::WriteAllText((Join-Path $dir 'save-config'), '') }
    [IO.File]::WriteAllText((Join-Path $dir 'wwconfig.exit'), [string]$ConfigExit)
    [IO.File]::WriteAllText((Join-Path $dir 'renegade.exit'), [string]$GameExit)
    $player = 'Test "Player"\with space\'
    if ($Batch) { $player = 'Batch Player' }
    $psi = [Diagnostics.ProcessStartInfo]::new()
    $psi.UseShellExecute = $false
    $psi.CreateNoWindow = $true
    $psi.WorkingDirectory = $testRoot # Deliberately different from the package.
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.RedirectStandardInput = $true
    if ($Batch) {
        @{GameDirectory=$gameDir;PlayerName=$player} | ConvertTo-Json |
            Set-Content -LiteralPath (Join-Path $dir 'launcher-settings.json') -Encoding UTF8
        $psi.FileName = $env:ComSpec
        $psi.Arguments = '/d /c ""' + (Join-Path $dir $Batch) + '" -Server 127.0.0.1:4848"'
    } else {
        $command = '& ' + (Quote-PowerShell (Join-Path $dir 'Launch-Coop.ps1')) +
            ' -Mode ' + $Mode + ' -GameDirectory ' + (Quote-PowerShell $gameDir) +
            ' -PlayerName ' + (Quote-PowerShell $player) + ' -Server 127.0.0.1:4848; exit $LASTEXITCODE'
        $psi.FileName = Join-Path $env:WINDIR 'System32/WindowsPowerShell/v1.0/powershell.exe'
        $psi.Arguments = '-NoLogo -NoProfile -ExecutionPolicy Bypass -OutputFormat Text -EncodedCommand ' +
            [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($command))
    }
    $process = [Diagnostics.Process]::Start($psi)
    try {
        $process.StandardInput.Close() # Allows a failure-path batch PAUSE to finish.
        if (-not $process.WaitForExit(15000)) {
            $process.Kill()
            throw "$Name timed out"
        }
        $stdout = $process.StandardOutput.ReadToEnd()
        $stderr = $process.StandardError.ReadToEnd()
        Assert-Equal $process.ExitCode $ExpectedExit "$Name exit code; $stdout $stderr"
    } finally { $process.Dispose() }
    $gameArgsPath = Join-Path $dir 'renegade.args'
    Assert-Equal (Test-Path -LiteralPath $gameArgsPath) $ExpectGame "$Name game execution"
    if ($ExpectGame) {
        $expected = @('--gamedir',$gameDir,'--ini',$config,'--multi','--netplayername',$player)
        if ($Mode -eq 'Host') { $expected += @('--coop-host','M01.mix','--coop-port','4848') }
        if ($Mode -eq 'Join') { $expected += @('--coop-connect','127.0.0.1:4848') }
        $actual = @(Get-Content -LiteralPath $gameArgsPath)
        Assert-Equal $actual.Count $expected.Count "$Name argument count"
        for ($i = 0; $i -lt $expected.Count; $i++) {
            Assert-Equal $actual[$i] $expected[$i] "$Name argument $i"
        }
        Assert-Equal ([IO.File]::ReadAllText((Join-Path $dir 'renegade.cwd'))) $dir "$Name working directory"
    }
    if ($FirstRun -or $Mode -eq 'Configure') {
        $configArgs = @(Get-Content -LiteralPath (Join-Path $dir 'wwconfig.args'))
        Assert-Equal $configArgs.Count 2 "$Name configuration argument count"
        Assert-Equal $configArgs[0] '--ini' "$Name configuration flag"
        Assert-Equal $configArgs[1] $config "$Name configuration path"
        Assert-Equal ([IO.File]::ReadAllText((Join-Path $dir 'wwconfig.cwd'))) $dir "$Name configuration working directory"
    }
    Write-Output "PASS $Name"
}
Invoke-Case 'menu'
Invoke-Case 'host' -Mode Host
Invoke-Case 'join' -Mode Join
Invoke-Case 'configure' -Mode Configure -ExpectGame $false
Invoke-Case 'configure-error' -Mode Configure -ConfigExit 17 -ExpectedExit 17 -ExpectGame $false
Invoke-Case 'first-run' -Mode Host -FirstRun
Invoke-Case 'first-run-error' -FirstRun -ConfigExit 17 -ExpectedExit 17 -ExpectGame $false
Invoke-Case 'first-run-cancel' -FirstRun -CancelConfig -ExpectedExit 1 -ExpectGame $false
Invoke-Case 'game-error' -GameExit 23 -ExpectedExit 23
Invoke-Case 'batch-menu' -Batch Play.cmd
Invoke-Case 'batch-host' -Mode Host -Batch Host-Coop.cmd
Invoke-Case 'batch-join' -Mode Join -Batch Join-Coop.cmd
Invoke-Case 'batch-configure' -Mode Configure -Batch Configure.cmd -ExpectGame $false
Invoke-Case 'batch-error' -Batch Play.cmd -GameExit 23 -ExpectedExit 23
Write-Output "All 14 launcher tests passed. Fixtures: $testRoot"