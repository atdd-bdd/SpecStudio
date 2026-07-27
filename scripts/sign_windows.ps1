# sign_windows.ps1 - Authenticode-sign the Windows artefacts with the Sectigo token.
#
# Deliberately separate from package_windows.ps1. Two reasons:
#
#   1. The private key lives on the USB token, not on disk. Since June 2023 the
#      CA/Browser Forum has required code-signing keys to sit on FIPS 140-2
#      Level 2 hardware, which is why Sectigo shipped one. Every signing session
#      needs the token present and its PIN entered, so this cannot be part of an
#      unattended build.
#   2. Keeping it separate means the build stays reproducible and the signing
#      step stays auditable: build once, sign the exact bytes you built.
#
# Windows only. The Sectigo certificate does not sign macOS binaries -- Gatekeeper
# only accepts an Apple Developer ID, see scripts/notarize_mac.sh. Linux has no
# equivalent; sign the package or the repository metadata with GPG instead.
#
#   .\scripts\sign_windows.ps1                     # sign everything in dist\
#   .\scripts\sign_windows.ps1 -Thumbprint ABC123  # pick a specific certificate
#   .\scripts\sign_windows.ps1 -ListCerts          # show what the token offers
#
# The PIN is never taken as a parameter and never written anywhere. Depending on
# how SafeNet Authentication Client is configured you will be prompted once per
# session or once per file; if you are being asked per file, enable
# "Enable single logon" in the SafeNet client rather than scripting the PIN.

[CmdletBinding()]
param(
    [string]   $Thumbprint,
    [string[]] $Path,
    [string]   $TimestampUrl = 'http://timestamp.sectigo.com',
    [switch]   $ListCerts,
    [switch]   $VerifyOnly
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot

# signtool reports progress and warnings on stderr, and Windows PowerShell 5.1
# turns any native stderr line into a terminating NativeCommandError while
# $ErrorActionPreference is 'Stop' -- which would abort a signing run partway
# through, leaving some files signed and some not. Judge by exit code.
function Invoke-SignTool {
    param([string[]] $Arguments)
    $prev = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $out = & $script:signtool @Arguments 2>&1
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $prev
    }
    return [pscustomobject]@{ ExitCode = $code; Output = $out }
}

# ---- locate signtool ---------------------------------------------------------
function Find-SignTool {
    $c = Get-Command signtool -ErrorAction SilentlyContinue
    if ($c) { return $c.Source }
    $kits = 'C:\Program Files (x86)\Windows Kits\10\bin'
    if (Test-Path $kits) {
        $found = Get-ChildItem $kits -Recurse -Filter signtool.exe -ErrorAction SilentlyContinue |
                 Where-Object { $_.FullName -match '\\x64\\' } |
                 Sort-Object FullName -Descending | Select-Object -First 1
        if ($found) { return $found.FullName }
    }
    throw "signtool.exe not found. Install the Windows SDK (Signing Tools component)."
}
$script:signtool = Find-SignTool
Write-Host "signtool: $script:signtool"

# ---- certificates ------------------------------------------------------------
# The SafeNet client publishes the token's certificate into the current user's
# personal store while the token is plugged in, so signtool can reach it by
# thumbprint and the key stays on the hardware.
function Get-SigningCerts {
    Get-ChildItem Cert:\CurrentUser\My -CodeSigningCert -ErrorAction SilentlyContinue |
        Where-Object { $_.NotAfter -gt (Get-Date) }
}

if ($ListCerts) {
    $certs = Get-SigningCerts
    if (-not $certs) {
        Write-Host 'No code-signing certificates visible.' -ForegroundColor Yellow
        Write-Host 'Plug in the Sectigo token and make sure SafeNet Authentication Client is running.'
        exit 1
    }
    $certs | Format-List Subject, Issuer, Thumbprint, NotAfter
    exit 0
}

if (-not $VerifyOnly -and -not $Thumbprint) {
    $certs = @(Get-SigningCerts)
    if ($certs.Count -eq 0) {
        Write-Host 'No code-signing certificate found.' -ForegroundColor Red
        Write-Host 'Plug in the Sectigo token, start SafeNet Authentication Client, then re-run.'
        Write-Host 'Use -ListCerts to see what is visible.'
        exit 1
    }
    if ($certs.Count -gt 1) {
        Write-Host 'More than one code-signing certificate is available:' -ForegroundColor Yellow
        $certs | ForEach-Object { "  {0}  {1}" -f $_.Thumbprint, $_.Subject }
        throw 'Pass -Thumbprint to choose one.'
    }
    $Thumbprint = $certs[0].Thumbprint
    Write-Host "Certificate: $($certs[0].Subject)"
}

# ---- what to sign ------------------------------------------------------------
# Sign every executable that ships, not just the launcher: SpecStudio starts
# SpecTableConverter and SpecStudioAskPass as child processes, and an unsigned
# child undermines the signature on the parent. The installer is signed too, so
# SmartScreen sees a signed download.
if (-not $Path) {
    $dist = Join-Path $repo 'dist'
    if (-not (Test-Path $dist)) { throw "No dist\ folder. Run package_windows.ps1 first." }
    $Path = @(
        Get-ChildItem $dist -Recurse -Include '*.exe','*.dll' -File |
            Where-Object { $_.FullName -notmatch '\\Qt6.*\.dll$' } |
            Select-Object -ExpandProperty FullName
    )
    # Qt's own DLLs already carry the Qt Company's signature; re-signing them is
    # unnecessary and slows a token-backed sign to a crawl.
}
$Path = @($Path | Where-Object { Test-Path $_ })
if ($Path.Count -eq 0) { throw 'Nothing to sign.' }

# ---- verify only -------------------------------------------------------------
if ($VerifyOnly) {
    $bad = 0
    foreach ($f in $Path) {
        if ((Invoke-SignTool @('verify', '/pa', '/q', $f)).ExitCode -ne 0) {
            Write-Host "UNSIGNED  $f" -ForegroundColor Red; $bad++
        } else {
            Write-Host "signed    $f" -ForegroundColor Green
        }
    }
    if ($bad) { exit 1 }
    exit 0
}

# ---- sign --------------------------------------------------------------------
Write-Host ""
Write-Host "Signing $($Path.Count) file(s). Enter the token PIN if prompted." -ForegroundColor Cyan

$failed = @()
foreach ($f in $Path) {
    Write-Host "  $([IO.Path]::GetFileName($f))"
    # /fd sha256  digest algorithm for the signature
    # /td sha256  digest algorithm for the RFC-3161 timestamp
    # /tr         timestamp server: without one, signatures stop validating the
    #             day the certificate expires, rather than staying valid for
    #             what was signed while it was live.
    $r = Invoke-SignTool @('sign', '/sha1', $Thumbprint, '/fd', 'sha256',
                           '/tr', $TimestampUrl, '/td', 'sha256', '/q', $f)
    if ($r.ExitCode -ne 0) {
        $r.Output | ForEach-Object { Write-Host "      $_" -ForegroundColor DarkGray }
        $failed += $f
    }
}

if ($failed.Count -gt 0) {
    Write-Host ''
    Write-Host "Failed to sign $($failed.Count) file(s):" -ForegroundColor Red
    $failed | ForEach-Object { Write-Host "  $_" }
    exit 1
}

Write-Host ''
Write-Host 'Verifying...' -ForegroundColor Cyan
foreach ($f in $Path) {
    $r = Invoke-SignTool @('verify', '/pa', '/q', $f)
    if ($r.ExitCode -ne 0) {
        Write-Host "  verification FAILED: $f" -ForegroundColor Red
        $r.Output | ForEach-Object { Write-Host "      $_" -ForegroundColor DarkGray }
        exit 1
    }
}
Write-Host 'All signed and verified.' -ForegroundColor Green
Write-Host ''
Write-Host 'If you re-package after this, sign again - packaging overwrites the files.' -ForegroundColor Yellow
