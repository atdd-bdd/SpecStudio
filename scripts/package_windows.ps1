# package_windows.ps1 - build SpecStudio for Windows and package it.
#
# Produces, in dist/:
#   SpecStudio-<version>-windows-x64/       a self-contained folder
#   SpecStudio-<version>-windows-x64.zip    the same folder, zipped (portable)
#   SpecStudio-<version>-setup.exe          an installer, if Inno Setup is present
#
# Everything needed to run is bundled: the Qt DLLs and plugins that windeployqt
# resolves, plus the two helper executables SpecStudio locates beside itself
# (SpecTableConverter and SpecStudioAskPass). A machine with no Qt and no
# Visual Studio can run the result -- except for the C++ runtime, see below.
#
# Nothing here is signed. Signing needs the Sectigo token plugged in and a PIN,
# so it is a separate, deliberate step: scripts\sign_windows.ps1.
#
#   .\scripts\package_windows.ps1
#   .\scripts\package_windows.ps1 -QtDir C:\Qt\6.10.0\msvc2022_64 -BuildType Release
#   .\scripts\package_windows.ps1 -SkipBuild        # package what is already built

[CmdletBinding()]
param(
    [string] $QtDir     = 'C:\Qt\6.10.0\msvc2022_64',
    [string] $BuildType = 'Release',
    [string] $CMake     = 'C:\Qt\Tools\CMake_64\bin\cmake.exe',
    [switch] $SkipBuild
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot

function Need-Path($path, $what) {
    if (-not (Test-Path $path)) { throw "$what not found: $path" }
    return $path
}

# Run an external tool and judge it by its exit code alone.
#
# Windows PowerShell 5.1 wraps every stderr line from a native executable in a
# NativeCommandError, and with $ErrorActionPreference = 'Stop' that terminates
# the script even when the tool succeeded. windeployqt warning "Cannot find any
# version of the dxcompiler.dll" -- entirely benign, SpecStudio uses no Direct3D
# -- was enough to kill packaging after a clean build. Exit code is the only
# trustworthy signal here.
function Invoke-Native {
    param(
        [Parameter(Mandatory)][string]   $Exe,
        [Parameter(Mandatory)][string[]] $Arguments,
        [string] $What = 'command',
        [switch] $ShowOutput
    )
    $prev = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $output = & $Exe @Arguments 2>&1
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $prev
    }
    if ($ShowOutput) { $output | ForEach-Object { Write-Host "    $_" } }
    if ($code -ne 0) {
        $output | ForEach-Object { Write-Host "    $_" -ForegroundColor DarkGray }
        throw "$What failed (exit $code)"
    }
    # Surface warnings without letting them stop the run.
    $output | Where-Object { "$_" -match '^\s*(Warning|WARNING)' } |
        ForEach-Object { Write-Host "    $_" -ForegroundColor DarkYellow }
}

Need-Path $QtDir 'Qt install' | Out-Null
$windeployqt = Need-Path (Join-Path $QtDir 'bin\windeployqt.exe') 'windeployqt'
if (-not (Test-Path $CMake)) {
    $c = Get-Command cmake -ErrorAction SilentlyContinue
    if (-not $c) { throw "CMake not found. Pass -CMake <path>." }
    $CMake = $c.Source
}

# Version for the artefact names: a release tag if the tree has one, otherwise
# the project() version in CMakeLists.txt. Never a bare commit hash -- these
# names end up in front of users.
# git exits non-zero and writes to stderr when the repository has no tags at
# all, which is a perfectly ordinary state and must not stop packaging. 2>$null
# is not enough under 'Stop' -- 5.1 still raises NativeCommandError.
$version = $null
$prev = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
try   { $version = (& git -C $repo describe --tags --abbrev=0 2>&1 |
                    Where-Object { $_ -is [string] } | Select-Object -First 1) }
catch { $version = $null }
finally { $ErrorActionPreference = $prev }
if ($LASTEXITCODE -ne 0) { $version = $null }

if (-not $version) {
    $m = Select-String -Path (Join-Path $repo 'CMakeLists.txt') -Pattern 'project\(SpecStudio VERSION ([0-9.]+)'
    $version = if ($m) { $m.Matches[0].Groups[1].Value } else { '0.0.0' }
}
$version = "$version".Trim().TrimStart('v')
Write-Host "SpecStudio $version ($BuildType)" -ForegroundColor Cyan

$buildDir = Join-Path $repo 'build'
$distRoot = Join-Path $repo 'dist'
$stage    = Join-Path $distRoot "SpecStudio-$version-windows-x64"

# ---- build -------------------------------------------------------------------
if (-not $SkipBuild) {
    # SpecStudio.exe cannot be relinked while it is running.
    Get-Process SpecStudio -ErrorAction SilentlyContinue | Stop-Process -Force
    Start-Sleep -Milliseconds 500

    Write-Host 'Configuring...'
    Invoke-Native $CMake @('-S', $repo, '-B', $buildDir,
                           '-G', 'Visual Studio 17 2022', '-A', 'x64',
                           "-DQt6_DIR=$QtDir\lib\cmake\Qt6") -What 'CMake configure'

    Write-Host 'Building...'
    Invoke-Native $CMake @('--build', $buildDir, '--config', $BuildType) -What 'Build'
}

# ---- stage -------------------------------------------------------------------
# All three executables go in one folder: SpecStudio finds the converter and the
# askpass helper through applicationDirPath(), so the layout is load-bearing.
$binaries = @(
    @{ Src = "$buildDir\src\$BuildType\SpecStudio.exe";                 Name = 'SpecStudio.exe' },
    @{ Src = "$buildDir\converter\$BuildType\SpecTableConverter.exe";   Name = 'SpecTableConverter.exe' },
    @{ Src = "$buildDir\src\$BuildType\SpecStudioAskPass.exe";          Name = 'SpecStudioAskPass.exe' }
)
foreach ($b in $binaries) { Need-Path $b.Src "$($b.Name) (build it first)" | Out-Null }

Write-Host "Staging to $stage"
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Path $stage -Force | Out-Null
foreach ($b in $binaries) { Copy-Item $b.Src (Join-Path $stage $b.Name) }

# ---- Qt runtime --------------------------------------------------------------
# windeployqt walks the imports of each executable and copies the Qt DLLs and
# the plugins they need (platforms\qwindows.dll above all -- without it the app
# dies at startup with "could not find or load the Qt platform plugin").
Write-Host 'Deploying Qt runtime...'
$deployArgs = @(
    '--release'
    '--no-translations'      # SpecStudio ships no .qm files
    '--no-system-d3d-compiler'
    '--no-opengl-sw'
    (Join-Path $stage 'SpecStudio.exe')
)
if ($BuildType -ne 'Release') { $deployArgs[0] = '--debug' }
Invoke-Native $windeployqt $deployArgs -What 'windeployqt (SpecStudio)'

# The converter is a separate Qt Core process; make sure its dependencies are
# in too. It needs no plugins, so this only tops up DLLs.
Invoke-Native $windeployqt @('--release', '--no-translations', '--no-plugins',
                             (Join-Path $stage 'SpecTableConverter.exe')) `
                           -What 'windeployqt (converter)'

Copy-Item (Join-Path $repo 'README.md') $stage -ErrorAction SilentlyContinue
Copy-Item (Join-Path $repo 'SpecStudio User Guide.md') $stage -ErrorAction SilentlyContinue

# The MSVC runtime is the one thing not bundled: redistributing it as loose DLLs
# is allowed but fragile, and the installer below pulls it in properly. Say so
# rather than let a portable-zip user hit a missing-DLL dialog.
@"
SpecStudio $version

Run SpecStudio.exe. Everything it needs is in this folder: the Qt runtime, the
code generator (SpecTableConverter.exe) and the git credential helper
(SpecStudioAskPass.exe). Keep them together -- SpecStudio looks for the other
two beside itself.

Requires the Microsoft Visual C++ 2015-2022 Redistributable (x64), which most
machines already have. If SpecStudio.exe will not start, install it from
https://aka.ms/vs/17/release/vc_redist.x64.exe

The language toolchains needed to build generated tests (JDK, .NET, Go, Rust,
Python, Node, Swift, a C++ compiler) are not included -- install whichever you
generate for.
"@ | Set-Content (Join-Path $stage 'README-FIRST.txt') -Encoding utf8

# ---- zip ---------------------------------------------------------------------
$zip = "$stage.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }
Write-Host 'Zipping...'
Compress-Archive -Path "$stage\*" -DestinationPath $zip

# ---- installer ---------------------------------------------------------------
$iscc = 'C:\Program Files (x86)\Inno Setup 6\ISCC.exe'
if (-not (Test-Path $iscc)) {
    $c = Get-Command iscc -ErrorAction SilentlyContinue
    if ($c) { $iscc = $c.Source }
}
if (Test-Path $iscc) {
    Write-Host 'Building installer...'
    Invoke-Native $iscc @("/DAppVersion=$version", "/DStageDir=$stage",
                          "/DOutDir=$distRoot",
                          (Join-Path $PSScriptRoot 'specstudio.iss')) -What 'Inno Setup'
} else {
    Write-Host 'Inno Setup not found - skipping installer, zip only.' -ForegroundColor Yellow
    Write-Host '  Get it from https://jrsoftware.org/isdl.php' -ForegroundColor Yellow
}

Write-Host ''
Write-Host 'Done. Artifacts in dist\:' -ForegroundColor Green
Get-ChildItem $distRoot | Where-Object { $_.Name -like "*$version*" } |
    ForEach-Object { "  {0}" -f $_.Name }
Write-Host ''
Write-Host 'Unsigned. To sign: .\scripts\sign_windows.ps1' -ForegroundColor Yellow
