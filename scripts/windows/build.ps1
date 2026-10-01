<#
.SYNOPSIS
Build one wolf3d-portable Windows configuration with an installed Visual Studio toolchain.

.DESCRIPTION
This is a human-friendly dispatcher for the authoritative CMake presets. It
detects supported Visual Studio installations, selects the requested wrapper,
architecture, configuration, and CRT mode, then prints and runs the resulting
CMake commands.

.EXAMPLE
.\build.ps1
Publishes both x64 wrappers with the newest supported installed compiler.

.EXAMPLE
.\build.ps1 -Compiler vs2019 -Architecture x86 -Wrapper win32
Publishes the 32-bit Win32 host with Visual Studio 2019/v142.

.EXAMPLE
.\build.ps1 -Action build -Configuration Debug -Wrapper sdl3
Builds (but does not stage) the x64 SDL3 Debug host.

.EXAMPLE
.\build.ps1 -List
Shows the supported compiler installations detected on this computer.
#>
[CmdletBinding()]
param(
    [ValidateSet('auto', 'vs2022', 'vs2019')]
    [string]$Compiler = 'auto',

    [ValidateSet('x64', 'x86')]
    [string]$Architecture = 'x64',

    [ValidateSet('all', 'win32', 'sdl3')]
    [string]$Wrapper = 'all',

    [ValidateSet('Release', 'Debug')]
    [string]$Configuration = 'Release',

    [ValidateSet('static', 'dynamic')]
    [string]$Runtime = 'static',

    [ValidateSet('publish', 'build', 'clean')]
    [string]$Action = 'publish',

    [ValidateRange(0, 256)]
    [int]$Jobs = 0,

    [switch]$List,
    [switch]$DryRun,
    [switch]$NonInteractive
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'

function Find-VisualStudio {
    param(
        [string]$Name,
        [string]$VersionRange,
        [string]$FallbackPath,
        [string]$Toolset,
        [string]$PresetPrefix
    )

    $installation = $null
    if (Test-Path -LiteralPath $vswhere) {
        $result = & $vswhere -latest -products '*' -version $VersionRange `
            -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
            -property installationPath
        if ($LASTEXITCODE -eq 0 -and $result) {
            $installation = ($result | Select-Object -Last 1).Trim()
        }
    }
    if (-not $installation -and (Test-Path -LiteralPath $FallbackPath)) {
        $installation = $FallbackPath
    }

    $cmake = $null
    if ($installation) {
        $candidate = Join-Path $installation `
            'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
        if (Test-Path -LiteralPath $candidate) {
            $cmake = $candidate
        }
    }

    [pscustomobject]@{
        Name = $Name
        Toolset = $Toolset
        PresetPrefix = $PresetPrefix
        Installation = $installation
        CMake = $cmake
        Available = [bool]($installation -and $cmake)
    }
}

$toolchains = @(
    Find-VisualStudio -Name 'vs2022' -VersionRange '[17.0,18.0)' `
        -FallbackPath 'C:\Program Files\Microsoft Visual Studio\2022\Enterprise' `
        -Toolset 'v143' -PresetPrefix 'windows-vs2022'
    Find-VisualStudio -Name 'vs2019' -VersionRange '[16.0,17.0)' `
        -FallbackPath 'C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise' `
        -Toolset 'v142' -PresetPrefix 'windows'
)

function Read-BuildChoice {
    param(
        [string]$Prompt,
        [string[]]$Options,
        [int]$DefaultIndex = 0
    )

    while ($true) {
        Write-Host ''
        Write-Host $Prompt
        for ($index = 0; $index -lt $Options.Count; ++$index) {
            $defaultMarker = if ($index -eq $DefaultIndex) { ' (default)' } else { '' }
            Write-Host ("  {0}. {1}{2}" -f ($index + 1), $Options[$index], $defaultMarker)
        }
        $answer = Read-Host 'Selection'
        if ([string]::IsNullOrWhiteSpace($answer)) {
            return $Options[$DefaultIndex]
        }
        $number = 0
        if ([int]::TryParse($answer, [ref]$number) -and
            $number -ge 1 -and $number -le $Options.Count) {
            return $Options[$number - 1]
        }
        $match = $Options | Where-Object { $_ -ieq $answer } | Select-Object -First 1
        if ($match) { return $match }
        Write-Warning 'Enter a listed number or name.'
    }
}

$canPrompt = [Environment]::UserInteractive
try { $canPrompt = $canPrompt -and -not [Console]::IsInputRedirected } catch {}
if ($PSBoundParameters.Count -eq 0 -and -not $NonInteractive -and $canPrompt) {
    Write-Host 'wolf3d-portable guided Windows build'
    Write-Host 'Press Enter to accept each default.'
    $availableCompilers = @($toolchains | Where-Object { $_.Available } |
        ForEach-Object { $_.Name })
    if ($availableCompilers.Count -eq 0) {
        throw 'No supported Visual Studio installation was detected.'
    }
    $Compiler = Read-BuildChoice 'Compiler' $availableCompilers
    $Architecture = Read-BuildChoice 'Architecture' @('x64', 'x86')
    $Wrapper = Read-BuildChoice 'Wrapper' @('all', 'win32', 'sdl3')
    $Action = Read-BuildChoice 'Action' @('publish', 'build', 'clean')
    if ($Action -ne 'publish') {
        $Configuration = Read-BuildChoice 'Configuration' @('Release', 'Debug')
    }
    $Runtime = Read-BuildChoice 'MSVC runtime' @('static', 'dynamic')
    Write-Host ''
    $confirmation = Read-Host 'Continue with this build? [Y/n]'
    if ($confirmation -and $confirmation -notmatch '^[Yy]') { exit 0 }
}

if ($List) {
    $toolchains | Select-Object Name, Toolset, Available, Installation | Format-Table -AutoSize
    exit 0
}

if ($Action -eq 'publish' -and $Configuration -ne 'Release') {
    throw 'Publishing is restricted to Release builds. Use -Action build for Debug.'
}

if ($Compiler -eq 'auto') {
    $selected = $toolchains | Where-Object { $_.Available } | Select-Object -First 1
} else {
    $selected = $toolchains | Where-Object { $_.Name -eq $Compiler } | Select-Object -First 1
}
if (-not $selected -or -not $selected.Available) {
    $requested = if ($Compiler -eq 'auto') { 'a supported Visual Studio installation' } else { $Compiler }
    throw "Could not find $requested with C++ tools and bundled CMake. Run .\build.ps1 -List."
}

$archPreset = if ($Architecture -eq 'x86') { 'x86' } else { 'x64' }
$devPreset = "$($selected.PresetPrefix)-dev-$archPreset"
$staticReleasePreset = $null
if ($Wrapper -eq 'win32') {
    $staticReleasePreset = "$($selected.PresetPrefix)-release-$archPreset"
} elseif ($Wrapper -eq 'sdl3') {
    $staticReleasePreset = "$($selected.PresetPrefix)-sdl3-$archPreset"
}

$useDedicatedPreset = $Action -eq 'publish' -and $Runtime -eq 'static' `
    -and $Wrapper -ne 'all'
$preset = if ($useDedicatedPreset) { $staticReleasePreset } else { $devPreset }
$usePresetBinaryDir = $useDedicatedPreset -or `
    ($Wrapper -eq 'all' -and $Runtime -eq 'static')

if ($usePresetBinaryDir) {
    $buildDir = $null
} else {
    $parts = @('windows', $selected.Name, $Wrapper, $archPreset)
    if ($Configuration -eq 'Debug') {
        $parts += 'debug'
    }
    if ($Runtime -eq 'dynamic') {
        $parts += 'dynamic-crt'
    }
    $buildDir = Join-Path $root ('build\' + ($parts -join '-'))
}

$targets = @()
if ($Action -eq 'publish') {
    if ($Wrapper -eq 'all' -or $Wrapper -eq 'win32') {
        $targets += 'win32-release'
    }
    if ($Wrapper -eq 'all' -or $Wrapper -eq 'sdl3') {
        $targets += 'sdl3-release'
    }
} elseif ($Action -eq 'build') {
    if ($Wrapper -eq 'win32') {
        $targets += 'wolf3d-win32'
    } elseif ($Wrapper -eq 'sdl3') {
        $targets += 'wolf3d-sdl3'
    }
} else {
    $targets += 'clean'
}

function Invoke-DisplayedCommand {
    param([string]$Executable, [string[]]$Arguments)

    $displayArguments = $Arguments | ForEach-Object {
        if ($_ -match '[\s"]') { '"' + ($_ -replace '"', '\"') + '"' } else { $_ }
    }
    Write-Host ('> "{0}" {1}' -f $Executable, ($displayArguments -join ' '))
    if (-not $DryRun) {
        & $Executable @Arguments
        if ($LASTEXITCODE -ne 0) {
            throw "Command failed with exit code $LASTEXITCODE."
        }
    }
}

Write-Host 'wolf3d-portable Windows build'
Write-Host "  Compiler:      $($selected.Name) / $($selected.Toolset)"
Write-Host "  Architecture:  $Architecture"
Write-Host "  Wrapper:       $Wrapper"
Write-Host "  Configuration: $Configuration"
Write-Host "  CRT:           $Runtime"
Write-Host "  Action:        $Action"

if ($Action -eq 'clean' -and $buildDir -and
    -not (Test-Path -LiteralPath (Join-Path $buildDir 'CMakeCache.txt'))) {
    Write-Host "Nothing to clean: $buildDir has not been configured."
    exit 0
}

if ($Action -ne 'clean') {
    $configureArguments = @('--preset', $preset)
    if ($buildDir) {
        $configureArguments += @(
            '-B', $buildDir,
            "-DW3P_BUILD_WIN32=$(if ($Wrapper -eq 'all' -or $Wrapper -eq 'win32') { 'ON' } else { 'OFF' })",
            "-DW3P_BUILD_SDL3=$(if ($Wrapper -eq 'all' -or $Wrapper -eq 'sdl3') { 'ON' } else { 'OFF' })",
            "-DWG_STATIC_MSVC_RUNTIME=$(if ($Runtime -eq 'static') { 'ON' } else { 'OFF' })"
        )
    }
    Invoke-DisplayedCommand -Executable $selected.CMake -Arguments $configureArguments
}

$buildArguments = @('--build')
if ($buildDir) {
    $buildArguments += $buildDir
} else {
    $buildArguments += @('--preset', $preset)
}
$buildArguments += @('--config', $Configuration)
if ($targets.Count -gt 0 -and -not $useDedicatedPreset) {
    $buildArguments += '--target'
    $buildArguments += $targets
}
if ($Jobs -gt 0) {
    $buildArguments += @('--parallel', $Jobs.ToString())
}
Invoke-DisplayedCommand -Executable $selected.CMake -Arguments $buildArguments

if ($Action -eq 'publish') {
    Write-Host "Published package(s) are under $root\dist."
}
