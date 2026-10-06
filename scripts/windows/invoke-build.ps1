<#
.SYNOPSIS
Build one wolf3d-portable Windows configuration with Visual Studio or MinGW.

.DESCRIPTION
This is a human-friendly dispatcher for the authoritative CMake presets. It
detects supported Visual Studio and MSYS2 UCRT64 installations, selects the requested wrapper,
architecture, configuration, and CRT mode, then prints and runs the resulting
CMake commands.

.EXAMPLE
.\build.ps1
Publishes both x64 wrappers with the newest supported installed compiler.

.EXAMPLE
.\build.ps1 -Compiler vs2015 -Architecture x86 -Wrapper win32
Publishes the 32-bit Win32 host with Visual Studio 2015/v140.

.EXAMPLE
.\build.ps1 -Action build -Configuration Debug -Wrapper sdl3
Builds (but does not stage) the x64 SDL3 Debug host.

.EXAMPLE
.\build.ps1 -Compiler mingw-ucrt64 -Wrapper all -Publish
Publishes both MinGW UCRT64 wrappers. -Publish is an alias for -Action publish.

.EXAMPLE
.\build.ps1 -List
Shows the supported compiler installations detected on this computer.
#>
[CmdletBinding()]
param(
    [ValidateSet('auto', 'mingw-ucrt64', 'vs2022', 'vs2019', 'vs2017',
        'vs2015', 'vs2015-xp', 'vs2013', 'vs2012', 'vs2010', 'vs2008')]
    [string]$Compiler = 'auto',

    [ValidateSet('x64', 'x86')]
    [string]$Architecture = 'x64',

    [ValidateSet('all', 'win32', 'sdl3')]
    [string]$Wrapper = 'all',

    [ValidateSet('Release', 'Debug')]
    [string]$Configuration = 'Release',

    [ValidateSet('static', 'dynamic')]
    [string]$Runtime = 'static',

    [ValidateSet('all', 'nuked', 'dbopl', 'silent', 'nuked-dbopl',
        'nuked-silent', 'dbopl-silent')]
    [string]$Drivers = 'all',

    [ValidateSet('nuked', 'dbopl', 'silent')]
    [string]$DefaultOpl = 'nuked',

    [ValidateRange(8000, 192000)]
    [int]$SampleRate = 48000,

    [ValidateSet('publish', 'build', 'clean')]
    [string]$Action = 'publish',

    [ValidateRange(0, 256)]
    [int]$Jobs = 0,

    [string]$Msys2Root = 'C:\msys64',

    [switch]$List,
    [switch]$ListObjects,
    [switch]$Publish,
    [switch]$DryRun,
    [switch]$NonInteractive
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'

if ($Publish) {
    if ($PSBoundParameters.ContainsKey('Action') -and $Action -ne 'publish') {
        throw '-Publish cannot be combined with a non-publish -Action value.'
    }
    $Action = 'publish'
}

$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'

function Find-MinGW {
    param([string]$Root)

    $bin = Join-Path $Root 'ucrt64\bin'
    $gcc = Join-Path $bin 'gcc.exe'
    $cmake = Join-Path $bin 'cmake.exe'
    $ninja = Join-Path $bin 'ninja.exe'
    $objdump = Join-Path $bin 'objdump.exe'
    [pscustomobject]@{
        Name = 'mingw-ucrt64'
        Toolset = 'GCC/UCRT64'
        PresetPrefix = 'windows-mingw-ucrt64'
        SupportsSDL3 = $true
        Installation = $Root
        CMake = $cmake
        Bin = $bin
        ObjDump = $objdump
        Available = [bool]((Test-Path -LiteralPath $gcc) -and
            (Test-Path -LiteralPath $cmake) -and
            (Test-Path -LiteralPath $ninja) -and
            (Test-Path -LiteralPath $objdump))
    }
}

function Find-VisualStudio {
    param(
        [string]$Name,
        [string]$VersionRange,
        [string]$FallbackPath,
        [string]$Toolset,
        [string]$PresetPrefix,
        [bool]$SupportsSDL3 = $true,
        [string]$RequiredComponent = 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64',
        [string]$CMakeFallbackDirectory = '',
        [string]$RequiredFile = ''
    )

    $installation = $null
    if (Test-Path -LiteralPath $vswhere) {
        $result = & $vswhere -latest -products '*' -version $VersionRange `
            -requires $RequiredComponent `
            -property installationPath
        if ($LASTEXITCODE -eq 0 -and $result) {
            $installation = ($result | Select-Object -Last 1).Trim()
        }
    }
    if (-not $installation -and $FallbackPath -and
        (Test-Path -LiteralPath $FallbackPath)) {
        $installation = $FallbackPath
    }
    if ($installation -and $RequiredFile -and
        -not (Test-Path -LiteralPath $RequiredFile)) {
        $installation = $null
    }

    $cmake = $null
    if ($installation) {
        $candidate = Join-Path $installation `
            'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
        if (Test-Path -LiteralPath $candidate) {
            $cmake = $candidate
        }
    }
    if (-not $cmake -and $CMakeFallbackDirectory) {
        $candidate = Join-Path $CMakeFallbackDirectory 'cmake.exe'
        if (Test-Path -LiteralPath $candidate) { $cmake = $candidate }
    }
    if (-not $cmake) {
        $pathCMake = Get-Command cmake.exe -ErrorAction SilentlyContinue
        if ($pathCMake) { $cmake = $pathCMake.Source }
    }

    [pscustomobject]@{
        Name = $Name
        Toolset = $Toolset
        PresetPrefix = $PresetPrefix
        SupportsSDL3 = $SupportsSDL3
        Installation = $installation
        CMake = $cmake
        Available = [bool]($installation -and $cmake)
    }
}

$legacyCMakeDirectory = ''
if (Test-Path -LiteralPath $vswhere) {
    foreach ($versionRange in @('[16.0,17.0)', '[17.0,18.0)', '[18.0,19.0)')) {
        $modernInstallation = & $vswhere -latest -products '*' `
            -version $versionRange -property installationPath
        if ($modernInstallation) {
            $candidate = Join-Path $modernInstallation.Trim() `
                'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin'
            if (Test-Path -LiteralPath (Join-Path $candidate 'cmake.exe')) {
                $legacyCMakeDirectory = $candidate
                break
            }
        }
    }
}

$toolchains = @(
    Find-VisualStudio -Name 'vs2022' -VersionRange '[17.0,18.0)' `
        -FallbackPath 'C:\Program Files\Microsoft Visual Studio\2022\Enterprise' `
        -Toolset 'v143' -PresetPrefix 'windows-vs2022'
    Find-VisualStudio -Name 'vs2019' -VersionRange '[16.0,17.0)' `
        -FallbackPath 'C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise' `
        -Toolset 'v142' -PresetPrefix 'windows'
    Find-VisualStudio -Name 'vs2017' -VersionRange '[16.0,17.0)' `
        -FallbackPath '' -Toolset 'v141' -PresetPrefix 'windows-vs2017' `
        -SupportsSDL3 $false `
        -RequiredComponent 'Microsoft.VisualStudio.Component.VC.v141.x86.x64'
    Find-VisualStudio -Name 'vs2015' -VersionRange '[14.0,15.0)' `
        -FallbackPath 'C:\Program Files (x86)\Microsoft Visual Studio 14.0' `
        -Toolset 'v140' -PresetPrefix 'windows-vs2015' `
        -SupportsSDL3 $false -CMakeFallbackDirectory $legacyCMakeDirectory
    Find-VisualStudio -Name 'vs2015-xp' -VersionRange '[14.0,15.0)' `
        -FallbackPath 'C:\Program Files (x86)\Microsoft Visual Studio 14.0' `
        -Toolset 'v140_xp' -PresetPrefix 'windows-vs2015-xp' `
        -SupportsSDL3 $false -CMakeFallbackDirectory $legacyCMakeDirectory `
        -RequiredFile 'C:\Program Files (x86)\MSBuild\Microsoft.Cpp\v4.0\V140\Platforms\Win32\PlatformToolsets\v140_xp\Toolset.props'
    Find-VisualStudio -Name 'vs2013' -VersionRange '[12.0,13.0)' `
        -FallbackPath 'C:\Program Files (x86)\Microsoft Visual Studio 12.0' `
        -Toolset 'v120' -PresetPrefix 'windows-vs2013' `
        -SupportsSDL3 $false -CMakeFallbackDirectory $legacyCMakeDirectory `
        -RequiredFile 'C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\bin\cl.exe'
    Find-VisualStudio -Name 'vs2012' -VersionRange '[11.0,12.0)' `
        -FallbackPath 'C:\Program Files (x86)\Microsoft Visual Studio 11.0' `
        -Toolset 'v110' -PresetPrefix 'windows-vs2012' `
        -SupportsSDL3 $false -CMakeFallbackDirectory $legacyCMakeDirectory `
        -RequiredFile 'C:\Program Files (x86)\Microsoft Visual Studio 11.0\VC\bin\cl.exe'
    Find-VisualStudio -Name 'vs2010' -VersionRange '[10.0,11.0)' `
        -FallbackPath 'C:\Program Files (x86)\Microsoft Visual Studio 10.0' `
        -Toolset 'v100' -PresetPrefix 'windows-vs2010' `
        -SupportsSDL3 $false -CMakeFallbackDirectory $legacyCMakeDirectory `
        -RequiredFile 'C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\bin\cl.exe'
    Find-VisualStudio -Name 'vs2008' -VersionRange '[9.0,10.0)' `
        -FallbackPath 'C:\Program Files (x86)\Microsoft Visual Studio 9.0' `
        -Toolset 'v90' -PresetPrefix 'windows-vs2008' `
        -SupportsSDL3 $false -CMakeFallbackDirectory $legacyCMakeDirectory `
        -RequiredFile 'C:\Program Files (x86)\Microsoft Visual Studio 9.0\VC\bin\cl.exe'
    Find-MinGW -Root $Msys2Root
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
    if ($Compiler -eq 'mingw-ucrt64') { $Architecture = 'x64' }
    else { $Architecture = Read-BuildChoice 'Architecture' @('x64', 'x86') }
    $promptToolchain = $toolchains | Where-Object { $_.Name -eq $Compiler } |
        Select-Object -First 1
    $wrapperChoices = if ($promptToolchain.SupportsSDL3) {
        @('all', 'win32', 'sdl3')
    } else {
        @('win32')
    }
    $Wrapper = Read-BuildChoice 'Wrapper' $wrapperChoices
    $Action = Read-BuildChoice 'Action' @('publish', 'build', 'clean')
    if ($Action -ne 'publish') {
        $Configuration = Read-BuildChoice 'Configuration' @('Release', 'Debug')
    }
    if ($Compiler -eq 'mingw-ucrt64') { $Runtime = 'static' }
    else { $Runtime = Read-BuildChoice 'MSVC runtime' @('static', 'dynamic') }
    $Drivers = Read-BuildChoice 'Compiled OPL drivers' @('all', 'nuked-dbopl', 'nuked-silent', 'dbopl-silent', 'nuked', 'dbopl', 'silent')
    $availableDefaults = if ($Drivers -eq 'all') { @('nuked', 'dbopl', 'silent') } else { @($Drivers -split '-') }
    $DefaultOpl = Read-BuildChoice 'Default OPL driver' $availableDefaults
    $SampleRate = [int](Read-BuildChoice 'Preferred PCM sample rate' @('48000', '44100'))
    Write-Host ''
    $confirmation = Read-Host 'Continue with this build? [Y/n]'
    if ($confirmation -and $confirmation -notmatch '^[Yy]') { exit 0 }
}

if ($ListObjects) {
    $toolchains
    exit 0
}
if ($List) {
    $toolchains | Select-Object Name, Toolset, Available, Installation | Format-Table -AutoSize
    exit 0
}

if ($Action -eq 'publish' -and $Configuration -ne 'Release') {
    throw 'Publishing is restricted to Release builds. Use -Action build for Debug.'
}
$driverList = if ($Drivers -eq 'all') { @('nuked', 'dbopl', 'silent') } else { @($Drivers -split '-') }
if ($driverList -notcontains $DefaultOpl) {
    throw "Default driver '$DefaultOpl' is not included by -Drivers $Drivers."
}

if ($Compiler -eq 'auto') {
    $selected = $toolchains | Where-Object { $_.Available } | Select-Object -First 1
} else {
    $selected = $toolchains | Where-Object { $_.Name -eq $Compiler } | Select-Object -First 1
}
if (-not $selected -or -not $selected.Available) {
    $requested = if ($Compiler -eq 'auto') { 'a supported Windows compiler' } else { $Compiler }
    throw "Could not find $requested. Run .\build.ps1 -List; for MinGW, pass -Msys2Root if MSYS2 is not under C:\msys64."
}
if ($selected.Name -eq 'mingw-ucrt64' -and $Architecture -ne 'x64') {
    throw 'The MSYS2 UCRT64 profile supports x64 only.'
}
if ($selected.Name -eq 'mingw-ucrt64' -and $Runtime -ne 'static') {
    throw 'MinGW packages require the statically linked GCC support runtime.'
}
if ($selected.Name -eq 'mingw-ucrt64') {
    $env:PATH = $selected.Bin + ';' +
        (Join-Path $selected.Installation 'usr\bin') + ';' + $env:PATH
}
if (-not $selected.SupportsSDL3 -and $Wrapper -ne 'win32') {
    throw "$($selected.Name)/$($selected.Toolset) supports only the Win32 wrapper; select -Wrapper win32."
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
    -and $Wrapper -ne 'all' -and $Drivers -eq 'all' `
    -and $DefaultOpl -eq 'nuked' -and $SampleRate -eq 48000
$preset = if ($useDedicatedPreset) { $staticReleasePreset } else { $devPreset }
$usePresetBinaryDir = $useDedicatedPreset -or `
    ($Wrapper -eq 'all' -and $Runtime -eq 'static' `
     -and $Drivers -eq 'all' -and $DefaultOpl -eq 'nuked' `
     -and $SampleRate -eq 48000)

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
    if ($Drivers -ne 'all') { $parts += $Drivers }
    if ($DefaultOpl -ne 'nuked') { $parts += "default-$DefaultOpl" }
    if ($SampleRate -ne 48000) { $parts += "$SampleRate-hz" }
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

function Assert-MinGWRuntimeImports {
    param([string]$ObjDump)

    $version = (Get-Content -LiteralPath (Join-Path $root 'lib\wolf3d\VERSION') `
        -TotalCount 1).Trim()
    $packages = @(Get-ChildItem -LiteralPath (Join-Path $root 'dist') `
        -Directory -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -like "wolf3d-portable-$version-*-mingw-ucrt-gcc*" })
    if ($packages.Count -eq 0) {
        throw 'No MinGW package was found for runtime-import validation.'
    }
    foreach ($package in $packages) {
        $binaries = Get-ChildItem -LiteralPath $package.FullName -File |
            Where-Object { $_.Extension -in @('.exe', '.dll') }
        foreach ($binary in $binaries) {
            $imports = & $ObjDump -p $binary.FullName
            if ($LASTEXITCODE -ne 0) {
                throw "Could not inspect imports for $($binary.FullName)."
            }
            if ($imports -match 'cygwin1\.dll|msys-2\.0\.dll|libgcc_s_.*\.dll|libstdc\+\+-6\.dll|libwinpthread-1\.dll') {
                throw "Unexpected MinGW/MSYS runtime dependency in $($binary.FullName)."
            }
        }
    }
    Write-Host 'Verified: packaged binaries require no MSYS, Cygwin, libgcc, libstdc++, or winpthread DLL.'
}

Write-Host 'wolf3d-portable Windows build'
Write-Host "  Compiler:      $($selected.Name) / $($selected.Toolset)"
Write-Host "  Architecture:  $Architecture"
Write-Host "  Wrapper:       $Wrapper"
Write-Host "  Configuration: $Configuration"
Write-Host "  Compiler CRT:  $Runtime"
Write-Host "  OPL drivers:   $($driverList -join ', ')"
Write-Host "  OPL default:   $DefaultOpl"
Write-Host "  Sample rate:   $SampleRate Hz"
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
            "-DWG_STATIC_MSVC_RUNTIME=$(if ($Runtime -eq 'static') { 'ON' } else { 'OFF' })",
            "-DWG_STATIC_GNU_RUNTIME=$(if ($Runtime -eq 'static') { 'ON' } else { 'OFF' })",
            "-DWG_ENABLE_OPL_NUKED=$(if ($driverList -contains 'nuked') { 'ON' } else { 'OFF' })",
            "-DWG_ENABLE_OPL_DBOPL=$(if ($driverList -contains 'dbopl') { 'ON' } else { 'OFF' })",
            "-DWG_ENABLE_OPL_SILENT=$(if ($driverList -contains 'silent') { 'ON' } else { 'OFF' })",
            "-DWG_DEFAULT_OPL_DRIVER=$DefaultOpl",
            "-DWG_DEFAULT_SAMPLE_RATE=$SampleRate"
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
    if ($selected.Name -eq 'mingw-ucrt64' -and -not $DryRun) {
        Assert-MinGWRuntimeImports -ObjDump $selected.ObjDump
    }
    Write-Host "Published package(s) are under $root\dist."
}
