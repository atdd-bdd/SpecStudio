# package_windows.ps1 - build AlignThree for Windows and package it.
#
# Produces, in dist/:
#   AlignThree-<version>-windows-x64/       a self-contained folder
#   AlignThree-<version>-windows-x64.zip    the same folder, zipped (portable)
#   AlignThree-<version>-setup.exe          an installer, if Inno Setup is present
#
# Everything needed to run is bundled: the Qt DLLs and plugins that windeployqt
# resolves, plus the two helper executables AlignThree locates beside itself
# (SpecTableConverter and AlignThreeAskPass). A machine with no Qt and no
# Visual Studio can run the result -- except for the C++ runtime, see below.
#
# The executables this script produces are not signed. Signing needs the Sectigo
# token plugged in and a PIN, so it is a separate, deliberate step:
# scripts\sign_windows.ps1.
#
# The one exception is the installer, and it has to be -- see "the uninstaller"
# below. If a code-signing certificate is visible when the installer is built,
# Inno Setup signs it and the embedded uninstaller stub during the compile.
# Pass -SkipSigning to build a deliberately unsigned installer.
#
#   .\scripts\package_windows.ps1
#   .\scripts\package_windows.ps1 -QtDir C:\Qt\6.10.0\msvc2022_64 -BuildType Release
#   .\scripts\package_windows.ps1 -SkipBuild        # package what is already built
#   .\scripts\package_windows.ps1 -SkipSigning      # never touch the token
#
# ---- the uninstaller ---------------------------------------------------------
#
# unins000.exe is not built here. Inno generates it on the *user's* machine from
# a stub embedded in the installer, so sign_windows.ps1 -- which only ever sees
# files in dist\ -- cannot reach it. It shipped unsigned, and users got a
# SmartScreen warning while trying to remove the program.
#
# Inno's SignedUninstaller signs that stub at compile time instead. The uninstall
# data is written to unins000.dat rather than appended to the .exe, so the file
# laid down on disk is byte-for-byte what was signed.
#
# ---- signing a release needs two phases --------------------------------------
#
# The zip and the installer are both built *from the staged folder*, so a plain
# "package, then sign" run signs the loose staged executables and the installer
# wrapper while leaving the copies embedded in the zip and the installer
# unsigned. A user who installs then has an unsigned AlignThree.exe on disk.
#
# So for anything that ships, stage and package separately, signing in between:
#
#   .\scripts\package_windows.ps1 -StageOnly        # build + stage, no zip/installer
#   .\scripts\sign_windows.ps1                      # sign the staged .exe files
#   .\scripts\package_windows.ps1 -PackageOnly      # zip + installer from signed files
#
# -PackageOnly deliberately does not re-stage: re-staging would overwrite the
# executables that were just signed. It reports whether what it is about to
# package is signed.
#
# There is no fourth step signing the installer: the -PackageOnly compile signs
# it and the uninstaller stub itself. Expect the token to be asked for during
# that step, possibly twice -- once per file -- unless SafeNet single logon is
# enabled. If signing was skipped, the script says so and tells you to run
# sign_windows.ps1 over the setup .exe, which still leaves the uninstaller
# unsigned.

[CmdletBinding()]
param(
    [string] $QtDir     = 'C:\Qt\6.10.0\msvc2022_64',
    [string] $BuildType = 'Release',
    [string] $CMake     = 'C:\Qt\Tools\CMake_64\bin\cmake.exe',
    [string] $Thumbprint,
    [string] $TimestampUrl = 'http://timestamp.sectigo.com',
    [switch] $SkipBuild,
    [switch] $StageOnly,
    [switch] $PackageOnly,
    [switch] $SkipSigning
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'Signing.ps1')

if ($StageOnly -and $PackageOnly) {
    throw '-StageOnly and -PackageOnly are the two halves of one release; pass one or neither.'
}

function Need-Path($path, $what) {
    if (-not (Test-Path $path)) { throw "$what not found: $path" }
    return $path
}

# Run an external tool and judge it by its exit code alone.
#
# Windows PowerShell 5.1 wraps every stderr line from a native executable in a
# NativeCommandError, and with $ErrorActionPreference = 'Stop' that terminates
# the script even when the tool succeeded. windeployqt warning "Cannot find any
# version of the dxcompiler.dll" -- entirely benign, AlignThree uses no Direct3D
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
    $m = Select-String -Path (Join-Path $repo 'CMakeLists.txt') -Pattern 'project\(AlignThree VERSION ([0-9.]+)'
    $version = if ($m) { $m.Matches[0].Groups[1].Value } else { '0.0.0' }
}
$version = "$version".Trim().TrimStart('v')
Write-Host "AlignThree $version ($BuildType)" -ForegroundColor Cyan

$buildDir = Join-Path $repo 'build'
$distRoot = Join-Path $repo 'dist'
$stage    = Join-Path $distRoot "AlignThree-$version-windows-x64"

# Set by Get-InstallerSigningArgs. Drives the closing advice: whether the setup
# .exe still needs a sign_windows.ps1 pass, or came out of the compile signed.
$script:installerSigned = $false

# The extra ISCC arguments that turn on signing, or an empty array.
#
# Inno needs both halves together: /DSignUninstaller makes the script ask for a
# sign tool, /Ssigntool=<command> supplies it. Naming one without the other
# fails the compile.
#
# Returns nothing rather than throwing when the token is absent. Packaging an
# unsigned test installer is legitimate, and the caller reports the outcome
# either way -- what must not happen is silence.
function Get-InstallerSigningArgs {
    if ($SkipSigning) {
        Write-Host 'Installer signing skipped (-SkipSigning).' -ForegroundColor Yellow
        return @()
    }

    $tool = Find-SignTool -Quiet
    if (-not $tool) {
        Write-Host 'signtool.exe not found - the installer and uninstaller will be unsigned.' -ForegroundColor Yellow
        Write-Host '  Install the Windows SDK (Signing Tools component).' -ForegroundColor Yellow
        return @()
    }

    $thumb = $Thumbprint
    if ($thumb) {
        # Same normalisation as sign_windows.ps1: a hash copied out of the
        # Windows certificate dialog carries an invisible U+200E mark and spaces
        # between byte pairs, and signtool rejects it as "Invalid SHA1 hash
        # format" without saying why.
        $thumb = ($thumb -replace '[^0-9A-Fa-f]', '').ToUpperInvariant()
        if ($thumb -notmatch '^[0-9A-F]{40}$') {
            throw "-Thumbprint must be a certificate's 40-digit SHA1 hash; got '$Thumbprint'."
        }
    } else {
        $certs = @(Get-SigningCerts)
        if ($certs.Count -eq 0) {
            Write-Host 'No code-signing certificate visible - the installer and uninstaller will be unsigned.' -ForegroundColor Yellow
            Write-Host '  Plug in the Sectigo token and start SafeNet Authentication Client.' -ForegroundColor Yellow
            return @()
        }
        if ($certs.Count -gt 1) {
            Write-Host 'More than one code-signing certificate is available:' -ForegroundColor Yellow
            $certs | ForEach-Object { "  {0}  {1}" -f $_.Thumbprint, $_.Subject }
            Write-Host 'Pass -Thumbprint to choose one. Unsigned installer and uninstaller for now.' -ForegroundColor Yellow
            return @()
        }
        $thumb = $certs[0].Thumbprint
        Write-Host "Certificate: $($certs[0].Subject)"
    }

    # $f is Inno's placeholder for the file being signed, and Inno quotes the
    # substitution itself -- wrapping it in $q$q here yields a doubly quoted path
    # that signtool cannot open. Single-quoted so PowerShell leaves it alone.
    $command = "$tool sign /sha1 $thumb /fd sha256 /tr $TimestampUrl /td sha256 " + '$f'

    $script:installerSigned = $true
    return @('/DSignUninstaller', "/Ssigntool=$command")
}

# A function rather than inline code, because packaging has to run either at the
# end of a full run or on its own under -PackageOnly. One definition means the
# two phases cannot drift apart.
function Invoke-Package {
    $zip = "$stage.zip"
    if (Test-Path $zip) { Remove-Item $zip -Force }
    Write-Host 'Zipping...'
    Compress-Archive -Path "$stage\*" -DestinationPath $zip

    $iscc = 'C:\Program Files (x86)\Inno Setup 6\ISCC.exe'
    if (-not (Test-Path $iscc)) {
        $c = Get-Command iscc -ErrorAction SilentlyContinue
        if ($c) { $iscc = $c.Source }
    }
    if (Test-Path $iscc) {
        $signArgs = @(Get-InstallerSigningArgs)
        if ($script:installerSigned) {
            Write-Host 'Building installer (signed - enter the token PIN if prompted)...'
        } else {
            Write-Host 'Building installer...'
        }
        # Inno verifies the signature after the sign tool exits, so a tool that
        # returns 0 without signing aborts the compile rather than producing an
        # installer that only looks signed.
        #
        # The certificate stays visible in the store for a while after the token
        # is unplugged, so signing can be attempted with no hardware to sign
        # with. That surfaces as a bare Inno exit code; say what to do about it.
        try {
            Invoke-Native $iscc (@("/DAppVersion=$version", "/DStageDir=$stage",
                                   "/DOutDir=$distRoot") + $signArgs +
                                 @((Join-Path $PSScriptRoot 'alignthree.iss'))) -What 'Inno Setup'
        } catch {
            if ($script:installerSigned) {
                Write-Host ''
                Write-Host 'The installer compile failed while signing was enabled.' -ForegroundColor Red
                Write-Host 'If the Sectigo token is not plugged in, or SafeNet Authentication' -ForegroundColor Yellow
                Write-Host 'Client is not running, signtool cannot reach the private key even' -ForegroundColor Yellow
                Write-Host 'though the certificate is still listed in the store.' -ForegroundColor Yellow
                Write-Host 'Plug it in, or re-run with -SkipSigning to build unsigned.' -ForegroundColor Yellow
            }
            throw
        }
    } else {
        Write-Host 'Inno Setup not found - skipping installer, zip only.' -ForegroundColor Yellow
        Write-Host '  Get it from https://jrsoftware.org/isdl.php' -ForegroundColor Yellow
    }
}

function Write-ArtifactSummary {
    Write-Host ''
    Write-Host 'Done. Artifacts in dist\:' -ForegroundColor Green
    Get-ChildItem $distRoot | Where-Object { $_.Name -like "*$version*" } |
        ForEach-Object { "  {0}" -f $_.Name }
}

# Whether the staged executables carry a valid signature. Reported rather than
# enforced -- packaging unsigned binaries is legitimate for a test build, it
# just must not happen silently when the point was to ship signed ones.
function Test-StagedSignatures {
    $exes = @(Get-ChildItem $stage -Recurse -Include '*.exe' -File)
    if ($exes.Count -eq 0) { return $false }
    $unsigned = @($exes | Where-Object {
        (Get-AuthenticodeSignature $_.FullName).Status -ne 'Valid'
    })
    if ($unsigned.Count -eq 0) {
        Write-Host "All $($exes.Count) staged executable(s) are signed." -ForegroundColor Green
        return $true
    }
    Write-Host 'Staged executables without a valid signature:' -ForegroundColor Yellow
    $unsigned | ForEach-Object { Write-Host "  $($_.Name)" -ForegroundColor Yellow }
    return $false
}

# ---- package only ------------------------------------------------------------
# Stops well before staging, so the signed executables in $stage survive.
if ($PackageOnly) {
    if (-not (Test-Path $stage)) {
        throw "Nothing staged at $stage. Run with -StageOnly first."
    }
    Write-Host "Packaging the existing staged folder: $stage"
    if (-not (Test-StagedSignatures)) {
        Write-Host 'The zip and installer will carry them unsigned.' -ForegroundColor Yellow
        Write-Host 'Run .\scripts\sign_windows.ps1 first if this is a release.' -ForegroundColor Yellow
    }
    Invoke-Package
    Write-ArtifactSummary
    Write-Host ''
    if ($script:installerSigned) {
        Write-Host 'The installer and its uninstaller stub were signed during the compile.' -ForegroundColor Green
        Write-Host 'Nothing further to sign.' -ForegroundColor Green
    } else {
        Write-Host "Now sign the installer: .\scripts\sign_windows.ps1 dist\AlignThree-$version-setup.exe" -ForegroundColor Yellow
        Write-Host 'That leaves unins000.exe unsigned - users will see SmartScreen when' -ForegroundColor Yellow
        Write-Host 'they uninstall. Only a signed compile can reach the uninstaller stub.' -ForegroundColor Yellow
    }
    exit 0
}

# ---- build -------------------------------------------------------------------
if (-not $SkipBuild) {
    # AlignThree.exe cannot be relinked while it is running.
    Get-Process AlignThree -ErrorAction SilentlyContinue | Stop-Process -Force
    Start-Sleep -Milliseconds 500

    Write-Host 'Configuring...'
    Invoke-Native $CMake @('-S', $repo, '-B', $buildDir,
                           '-G', 'Visual Studio 17 2022', '-A', 'x64',
                           "-DQt6_DIR=$QtDir\lib\cmake\Qt6") -What 'CMake configure'

    Write-Host 'Building...'
    Invoke-Native $CMake @('--build', $buildDir, '--config', $BuildType) -What 'Build'
}

# ---- stage -------------------------------------------------------------------
# All three executables go in one folder: AlignThree finds the converter and the
# askpass helper through applicationDirPath(), so the layout is load-bearing.
$binaries = @(
    @{ Src = "$buildDir\src\$BuildType\AlignThree.exe";                 Name = 'AlignThree.exe' },
    @{ Src = "$buildDir\converter\$BuildType\SpecTableConverter.exe";   Name = 'SpecTableConverter.exe' },
    @{ Src = "$buildDir\src\$BuildType\AlignThreeAskPass.exe";          Name = 'AlignThreeAskPass.exe' }
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
    '--no-translations'      # AlignThree ships no .qm files
    '--no-system-d3d-compiler'
    '--no-opengl-sw'
    (Join-Path $stage 'AlignThree.exe')
)
if ($BuildType -ne 'Release') { $deployArgs[0] = '--debug' }
Invoke-Native $windeployqt $deployArgs -What 'windeployqt (AlignThree)'

# The converter is a separate Qt Core process; make sure its dependencies are
# in too. It needs no plugins, so this only tops up DLLs.
Invoke-Native $windeployqt @('--release', '--no-translations', '--no-plugins',
                             (Join-Path $stage 'SpecTableConverter.exe')) `
                           -What 'windeployqt (converter)'

# Named explicitly and checked, not copied with -ErrorAction SilentlyContinue:
# this used to reference 'AlignThree User Guide.md', and when that file was
# renamed the copy failed in silence and the distribution shipped without a
# guide. A missing document should stop the build, not disappear from it.
foreach ($doc in @('README.md', 'Getting Started.md', 'User Guide.md',
                    'Configuration Guide.md', 'Git Setup.md',
                    'spectable syntax v3.3a.md')) {
    $src = Join-Path $repo $doc
    if (-not (Test-Path -LiteralPath $src)) {
        throw "Document not found: $doc. Update the list in package_windows.ps1 if it was renamed."
    }
    Copy-Item -LiteralPath $src -Destination $stage
}

# The MSVC runtime is the one thing not bundled: redistributing it as loose DLLs
# is allowed but fragile, and the installer below pulls it in properly. Say so
# rather than let a portable-zip user hit a missing-DLL dialog.
@"
AlignThree $version

Run AlignThree.exe. Everything it needs is in this folder: the Qt runtime, the
code generator (SpecTableConverter.exe) and the git credential helper
(AlignThreeAskPass.exe). Keep them together -- AlignThree looks for the other
two beside itself.

If Windows warned you when you started AlignThree.exe, that is the download tag
Windows copies onto everything extracted from a zip, not a problem with the
program. To avoid it next time, unblock the zip before extracting: right-click
it, Properties, tick Unblock, OK. AlignThree is signed by Ken Pugh, Inc. -- the
publisher shown behind "More info" is what to check.

Requires the Microsoft Visual C++ 2015-2022 Redistributable (x64), which most
machines already have. If AlignThree.exe will not start, install it from
https://aka.ms/vs/17/release/vc_redist.x64.exe

The language toolchains needed to build generated tests (JDK, .NET, Go, Rust,
Python, Node, Swift, a C++ compiler) are not included -- install whichever you
generate for.
"@ | Set-Content (Join-Path $stage 'README-FIRST.txt') -Encoding utf8

# ---- zip + installer ---------------------------------------------------------
if ($StageOnly) {
    Write-Host ''
    Write-Host "Staged, not packaged: $stage" -ForegroundColor Green
    Write-Host ''
    Write-Host 'Next, to ship signed binaries inside the zip and installer:' -ForegroundColor Cyan
    # The staged folder is named explicitly rather than relying on the no-argument
    # form, which would also sign a stale installer left over in dist\ from an
    # earlier run -- wasted PIN prompts on a file that step 2 then overwrites.
    Write-Host "  1. .\scripts\sign_windows.ps1 dist\AlignThree-$version-windows-x64\*.exe"
    Write-Host '  2. .\scripts\package_windows.ps1 -PackageOnly'
    Write-Host ''
    Write-Host '  Step 2 signs the installer and the uninstaller stub itself, so there is' -ForegroundColor DarkGray
    Write-Host '  no third step. Keep the token plugged in for it.' -ForegroundColor DarkGray
    exit 0
}

# A full run stages straight from build\, so these are normally unsigned; check
# rather than assume, in case the build output was signed in place.
$stagedWasSigned = @(Get-ChildItem $stage -Recurse -Include '*.exe' -File |
                     Where-Object { (Get-AuthenticodeSignature $_.FullName).Status -ne 'Valid' }
                    ).Count -eq 0

Invoke-Package
Write-ArtifactSummary

Write-Host ''
if ($stagedWasSigned) {
    Write-Host 'The zip and installer contain signed executables.' -ForegroundColor Green
    if ($script:installerSigned) {
        Write-Host 'The installer and its uninstaller stub are signed too.' -ForegroundColor Green
    } else {
        Write-Host "Still to do: .\scripts\sign_windows.ps1 dist\AlignThree-$version-setup.exe" -ForegroundColor Yellow
    }
} else {
    Write-Host 'Unsigned. To sign: .\scripts\sign_windows.ps1' -ForegroundColor Yellow
    Write-Host 'For a release, use -StageOnly / -PackageOnly so the signatures end up' -ForegroundColor Yellow
    Write-Host 'inside the zip and installer too - see the header of this script.' -ForegroundColor Yellow
}
