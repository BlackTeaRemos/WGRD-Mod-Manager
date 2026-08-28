#Requires -Version 5.1

Set-StrictMode -Version Latest

$TOOLCHAIN_VISUAL_STUDIO_ROOT = ''
$TOOLCHAIN_CMAKE_BIN = ''
$TOOLCHAIN_NINJA_BIN = ''
$TOOLCHAIN_VCPKG_ROOT = ''
$TOOLCHAIN_RESHARPER_BIN = ''

$scriptDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
$localOverridesPath = Join-Path $scriptDirectory 'ToolchainPaths.local.ps1'

if (Test-Path -LiteralPath $localOverridesPath) {
    . $localOverridesPath
}

function Find-VisualStudioRoot {
    param([string] $ConfiguredRoot)

    if ($ConfiguredRoot -and (Test-Path -LiteralPath $ConfiguredRoot)) {
        return $ConfiguredRoot
    }

    $vswherePath = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswherePath) {
        $discovered = & $vswherePath -latest -products * `
            -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
            -property installationPath 2>$null
        if ($discovered) {
            return $discovered.Trim()
        }
    }

    $candidateRoots = @(
        (Join-Path $env:ProgramFiles 'Microsoft Visual Studio'),
        (Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio')
    )
    foreach ($candidateRoot in $candidateRoots) {
        if (-not (Test-Path -LiteralPath $candidateRoot)) {
            continue
        }
        $found = Get-ChildItem -Path $candidateRoot -Recurse -Depth 2 -Filter 'vcvars64.bat' -ErrorAction SilentlyContinue |
            Select-Object -First 1
        if ($found) {
            return (Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $found.FullName)))
        }
    }

    return ''
}

function Find-ToolDirectory {
    param([string] $ConfiguredDirectory, [string] $ExecutableName)

    if ($ConfiguredDirectory -and (Test-Path -LiteralPath $ConfiguredDirectory)) {
        return $ConfiguredDirectory
    }

    $resolved = Get-Command $ExecutableName -ErrorAction SilentlyContinue
    if ($resolved) {
        return (Split-Path -Parent $resolved.Source)
    }

    return ''
}

$visualStudioRoot = Find-VisualStudioRoot -ConfiguredRoot $TOOLCHAIN_VISUAL_STUDIO_ROOT
if (-not $visualStudioRoot) {
    Write-Error "visual studio unresolved"
    return
}

$vcvarsPath = Join-Path $visualStudioRoot 'VC\Auxiliary\Build\vcvars64.bat'
if (-not (Test-Path -LiteralPath $vcvarsPath)) {
    Write-Error "vcvars missing"
    return
}

if (-not $TOOLCHAIN_NINJA_BIN) {
    $TOOLCHAIN_NINJA_BIN = Join-Path $visualStudioRoot 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja'
}

$cmakeBin = Find-ToolDirectory -ConfiguredDirectory $TOOLCHAIN_CMAKE_BIN -ExecutableName 'cmake'
$ninjaBin = Find-ToolDirectory -ConfiguredDirectory $TOOLCHAIN_NINJA_BIN -ExecutableName 'ninja'
$vcpkgRoot = Find-ToolDirectory -ConfiguredDirectory $TOOLCHAIN_VCPKG_ROOT -ExecutableName 'vcpkg'
$resharperBin = Find-ToolDirectory -ConfiguredDirectory $TOOLCHAIN_RESHARPER_BIN -ExecutableName 'inspectcode'

if (-not $vcpkgRoot -and $env:VCPKG_ROOT) {
    $vcpkgRoot = $env:VCPKG_ROOT
}

$resolvedPaths = [ordered]@{
    'visual studio' = $visualStudioRoot
    'cmake'         = $cmakeBin
    'ninja'         = $ninjaBin
    'vcpkg'         = $vcpkgRoot
    'resharper'     = $resharperBin
}

Write-Host "Using ToolchainPaths"
foreach ($entry in $resolvedPaths.GetEnumerator()) {
    $value = if ($entry.Value) { $entry.Value } else { 'unresolved' }
    Write-Host ("  {0,-14}{1}" -f $entry.Key, $value)
}

$capturedEnvironment = & "${env:COMSPEC}" /c "`"$vcvarsPath`" >nul 2>&1 && set"

foreach ($environmentLine in $capturedEnvironment) {
    if ($environmentLine -match '^([^=]+)=(.*)$') {
        Set-Item -Path "env:$($matches[1])" -Value $matches[2]
    }
}

foreach ($toolDirectory in @($cmakeBin, $ninjaBin, $vcpkgRoot, $resharperBin)) {
    if (-not $toolDirectory) {
        continue
    }
    if (($env:Path -split ';') -notcontains $toolDirectory) {
        $env:Path = "$toolDirectory;$env:Path"
    }
}

if ($vcpkgRoot) {
    $env:VCPKG_ROOT = $vcpkgRoot
}
$env:VCPKG_DEFAULT_TRIPLET = 'x64-windows-static'

foreach ($requiredTool in @('cl', 'cmake', 'ninja')) {
    if (-not (Get-Command $requiredTool -ErrorAction SilentlyContinue)) {
        Write-Warning "$requiredTool unresolved"
    }
}

$compilerBanner = (& cl.exe 2>&1 | Select-Object -First 1)
$cmakeBanner = (& cmake --version | Select-Object -First 1)

Write-Host "Ready"
Write-Host "  $compilerBanner"
Write-Host "  $cmakeBanner"
Write-Host "  triplet $env:VCPKG_DEFAULT_TRIPLET"

$global:LASTEXITCODE = 0
