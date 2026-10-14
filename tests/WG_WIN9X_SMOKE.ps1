param(
    [Parameter(Mandatory = $true)][string]$PackagePath,
    [ValidateSet('win9x', 'win2k')][string]$Profile = 'win9x',
    [string]$WorkRoot = (Join-Path $PSScriptRoot '../build/win9x-smoke')
)
$ErrorActionPreference = 'Stop'
$package = (Resolve-Path -LiteralPath $PackagePath).Path
$fixture = Join-Path $WorkRoot ([Guid]::NewGuid().ToString('N'))
$gameDir = Join-Path $fixture 'Game With Spaces'
New-Item -ItemType Directory -Force -Path $gameDir | Out-Null
Copy-Item -LiteralPath (Join-Path $package 'WOLF3D.EXE') -Destination $gameDir
Get-ChildItem -LiteralPath $package -Filter '*.dll' | Copy-Item -Destination $gameDir
$gameDir = (Resolve-Path -LiteralPath $gameDir).Path
$fixture = (Resolve-Path -LiteralPath $fixture).Path
$exe = Join-Path $gameDir 'WOLF3D.EXE'
$bytes = [IO.File]::ReadAllBytes($exe)
$pe = [BitConverter]::ToInt32($bytes, 0x3c)
$expectedSubsystem = if ($Profile -eq 'win9x') { 3 } else { 2 }
if ([BitConverter]::ToUInt16($bytes, $pe + 92) -ne $expectedSubsystem) {
    throw 'Unexpected PE subsystem.'
}
$defaultDriver = if ($Profile -eq 'win9x') { 'adlib' } else { 'dbopl' }
$compiledDrivers = if ($Profile -eq 'win9x') { 'dbopl silent adlib' } else { 'dbopl silent' }
function Check-Run([string]$Arguments, [int]$ExpectedExit, [string]$ExpectedText) {
    $info = New-Object Diagnostics.ProcessStartInfo
    $info.FileName = $exe
    $info.Arguments = $Arguments
    $info.WorkingDirectory = $fixture
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    $process = [Diagnostics.Process]::Start($info)
    $output = $process.StandardOutput.ReadToEnd()
    $errorText = $process.StandardError.ReadToEnd()
    $process.WaitForExit()
    if ($process.ExitCode -ne $ExpectedExit -or $output -notmatch $ExpectedText) {
        throw "Failed $Arguments (exit $($process.ExitCode)):`n$output`n$errorText"
    }
    $process.Dispose()
    Write-Host "PASS: $Arguments"
    return $output
}
$help = Check-Run '--no-config --help' 0 "Compiled OPL drivers: $compiledDrivers(?:\r?\n|$)"
if ($help -notmatch "Default OPL driver: $defaultDriver") { throw 'Wrong default driver.' }
$null = Check-Run '--no-config --diag --opl dbopl' 0 'Requested OPL driver \(not probed\): dbopl'
$ini = Join-Path $gameDir 'Wolf3d.ini'
[IO.File]::WriteAllText($ini, "--opl dbopl`n")
$diagnostics = Check-Run '--diag' 0 'Requested OPL driver \(not probed\): dbopl'
if ($diagnostics -notmatch 'Config:.*[\\/]wolf3d\.ini') { throw 'INI was not loaded beside the executable.' }
$null = Check-Run '--diag --opl silent' 0 'Requested OPL driver \(not probed\): silent'
$null = Check-Run '--no-config --diag' 0 "Requested OPL driver \(not probed\): $defaultDriver"
$null = Check-Run '--no-config --opl dbpol' 1 'Unknown or unavailable OPL driver: dbpol'
if ($Profile -eq 'win2k') {
    $null = Check-Run '--no-config --opl adlib' 1 'Unknown or unavailable OPL driver: adlib'
}
$null = Check-Run '--no-config --opl nuked' 1 'Unknown or unavailable OPL driver: nuked'
$null = Check-Run '--no-config --opl' 1 '--opl requires a driver name'
[IO.File]::WriteAllText($ini, "--opl dbpol`n")
$null = Check-Run '--diag' 1 'Unknown or unavailable OPL driver: dbpol'
$null = Check-Run '--diag --opl dbopl' 0 'Requested OPL driver \(not probed\): dbopl'
Write-Output "$Profile binary smoke checks passed. Fixture retained at $fixture"
