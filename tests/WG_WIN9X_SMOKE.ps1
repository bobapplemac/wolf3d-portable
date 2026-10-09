param(
    [Parameter(Mandatory = $true)][string]$PackagePath,
    [string]$WorkRoot = (Join-Path $PSScriptRoot '../build/win9x-smoke')
)
$ErrorActionPreference = 'Stop'
$package = (Resolve-Path -LiteralPath $PackagePath).Path
$fixture = Join-Path $WorkRoot ([Guid]::NewGuid().ToString('N'))
$gameDir = Join-Path $fixture 'Game With Spaces'
New-Item -ItemType Directory -Force -Path $gameDir | Out-Null
Copy-Item -LiteralPath (Join-Path $package 'wolf3d.exe') -Destination $gameDir
Get-ChildItem -LiteralPath $package -Filter '*.dll' | Copy-Item -Destination $gameDir
$gameDir = (Resolve-Path -LiteralPath $gameDir).Path
$fixture = (Resolve-Path -LiteralPath $fixture).Path
$exe = Join-Path $gameDir 'wolf3d.exe'
$bytes = [IO.File]::ReadAllBytes($exe)
$pe = [BitConverter]::ToInt32($bytes, 0x3c)
if ([BitConverter]::ToUInt16($bytes, $pe + 92) -ne 3) {
    throw 'Win9x executable must use the console subsystem.'
}
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
$help = Check-Run '--no-config --help' 0 'Compiled OPL drivers: dbopl silent adlib'
if ($help -notmatch 'Default OPL driver: adlib') { throw 'Wrong default driver.' }
$null = Check-Run '--no-config --diag --opl dbopl' 0 'Requested OPL driver \(not probed\): dbopl'
$ini = Join-Path $gameDir 'Wolf3d.ini'
[IO.File]::WriteAllText($ini, "--opl dbopl`n")
$diagnostics = Check-Run '--diag' 0 'Requested OPL driver \(not probed\): dbopl'
if ($diagnostics -notmatch 'Config:.*[\\/]wolf3d\.ini') { throw 'INI was not loaded beside the executable.' }
$null = Check-Run '--diag --opl silent' 0 'Requested OPL driver \(not probed\): silent'
$null = Check-Run '--no-config --diag' 0 'Requested OPL driver \(not probed\): adlib'
$null = Check-Run '--no-config --opl dbpol' 1 'Unknown or unavailable OPL driver: dbpol'
$null = Check-Run '--no-config --opl nuked' 1 'Unknown or unavailable OPL driver: nuked'
$null = Check-Run '--no-config --opl' 1 '--opl requires a driver name'
[IO.File]::WriteAllText($ini, "--opl dbpol`n")
$null = Check-Run '--diag' 1 'Unknown or unavailable OPL driver: dbpol'
$null = Check-Run '--diag --opl dbopl' 0 'Requested OPL driver \(not probed\): dbopl'
Write-Output "Win9x binary smoke checks passed. Fixture retained at $fixture"
