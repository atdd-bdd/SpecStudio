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
#   .\scripts\sign_windows.ps1                        # sign everything in dist\
#   .\scripts\sign_windows.ps1 dist\Foo-setup.exe     # sign just these files
#   .\scripts\sign_windows.ps1 -Thumbprint ABC123     # pick a certificate by hash
#   .\scripts\sign_windows.ps1 -SubjectName 'CN=...'  # or by subject name
#   .\scripts\sign_windows.ps1 -ListCerts             # show what the token offers
#
# -Path is the positional parameter, so a bare file name signs that file. It was
# -Thumbprint that sat in position 0, which meant a bare file name became the
# certificate hash -- signtool said "Invalid SHA1 hash format", and because
# -Path was then empty the run also widened to every file in dist\.
#
# Prefer the no-argument form. It picks the one *unexpired* code-signing
# certificate in the store, which is what you want, and neither -Thumbprint nor
# -SubjectName is needed until there is more than one valid certificate.
#
# Do not reach for signtool's own /n matching here. This store holds an expired
# 2022 certificate whose CN is also "Ken Pugh, Inc.", so /n matches two
# certificates and signtool refuses with "No certificates were found that met
# all the given criteria". -SubjectName below therefore resolves through the
# certificate store -- skipping expired certificates and reporting ambiguity
# itself -- and always signs by thumbprint.
#
# A second trap in the same message: /n matches a substring of the subject as
# rendered, and a CN containing a comma renders quoted -- CN="Ken Pugh, Inc.".
# So passing the full DN CN=Ken Pugh, Inc. cannot match. Pass the CN value only.
#
# The PIN is never taken as a parameter and never written anywhere. Depending on
# how SafeNet Authentication Client is configured you will be prompted once per
# session or once per file; if you are being asked per file, enable
# "Enable single logon" in the SafeNet client rather than scripting the PIN.

[CmdletBinding()]
param(
    [Parameter(Position = 0, ValueFromRemainingArguments = $true)]
    [string[]] $Path,
    [string]   $Thumbprint,
    [string]   $SubjectName,
    [string]   $TimestampUrl = 'http://timestamp.sectigo.com',
    [switch]   $ListCerts,
    [switch]   $VerifyOnly
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot

if ($Thumbprint -and $SubjectName) {
    throw 'Pass -Thumbprint or -SubjectName, not both.'
}

# -Path takes the remaining arguments, which would otherwise quietly absorb a
# mistyped or unsupported switch: it fails Test-Path, gets dropped, and the run
# carries on signing whatever else was named. Refuse instead.
$switches = @($Path | Where-Object { $_ -like '-*' })
if ($switches.Count -gt 0) {
    throw "Unknown option(s): $($switches -join ', '). " +
          'Run Get-Help .\scripts\sign_windows.ps1 for the accepted parameters.'
}

# Reject a thumbprint that is not one before handing it to signtool, whose only
# complaint is "Invalid SHA1 hash format" with no hint as to why. Two ways to
# get one: pass a file path (see the header), or copy the hash out of the
# Windows certificate dialog, which prefixes an invisible U+200E left-to-right
# mark and separates the byte pairs with spaces. Strip what is cosmetic, then
# insist on 40 hex digits.
if ($Thumbprint) {
    $clean = ($Thumbprint -replace '[^0-9A-Fa-f]', '').ToUpperInvariant()
    if ($clean -notmatch '^[0-9A-Fa-f]{40}$') {
        if (Test-Path -LiteralPath $Thumbprint) {
            throw "-Thumbprint got a file path ('$Thumbprint'). To sign that file, " +
                  "pass it without the -Thumbprint name: .\scripts\sign_windows.ps1 '$Thumbprint'"
        }
        throw "-Thumbprint must be a certificate's 40-digit SHA1 hash; got '$Thumbprint'. " +
              'Run -ListCerts to see the available thumbprints.'
    }
    $Thumbprint = $clean
}

# signtool reports progress and warnings on stderr, and Windows PowerShell 5.1
# turns any native stderr line into a terminating NativeCommandError while
# $ErrorActionPreference is 'Stop' -- which would abort a signing run partway
# through, leaving some files signed and some not. Judge by exit code.
function Invoke-SignTool {
    param([string[]] $Arguments)
    $prev = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        # 2>&1 wraps each stderr line in an ErrorRecord whose own ToString() can
        # render as "System.Management.Automation.RemoteException" -- the type
        # name instead of what signtool said. Unwrap to the message text so a
        # failure is readable.
        $out = & $script:signtool @Arguments 2>&1 | ForEach-Object {
            if ($_ -is [System.Management.Automation.ErrorRecord]) {
                if ($_.Exception -and $_.Exception.Message) { $_.Exception.Message }
                else { $_.ToString() }
            } else { [string]$_ }
        }
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $prev
    }
    return [pscustomobject]@{ ExitCode = $code; Output = @($out) }
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

# Resolve a subject name to a thumbprint here rather than passing signtool /n,
# so an expired certificate is skipped and an ambiguous name is reported with the
# candidates listed. Accepts either the CN value or a full DN, with or without
# the quotes a comma-bearing CN renders with.
if (-not $VerifyOnly -and $SubjectName) {
    $wanted = ($SubjectName -replace '^\s*CN\s*=\s*', '').Trim().Trim('"')
    $matched = @(Get-SigningCerts | Where-Object {
        $_.GetNameInfo([System.Security.Cryptography.X509Certificates.X509NameType]::SimpleName, $false) -eq $wanted -or
        $_.Subject -like "*$wanted*"
    })
    if ($matched.Count -eq 0) {
        Write-Host "No unexpired code-signing certificate matches '$SubjectName'." -ForegroundColor Red
        Write-Host 'Use -ListCerts to see what is visible.'
        exit 1
    }
    if ($matched.Count -gt 1) {
        Write-Host "'$SubjectName' matches more than one certificate:" -ForegroundColor Yellow
        $matched | ForEach-Object { "  {0}  {1}" -f $_.Thumbprint, $_.Subject }
        throw 'Pass -Thumbprint to choose one.'
    }
    $Thumbprint = $matched[0].Thumbprint
    Write-Host "Certificate: $($matched[0].Subject)"
}

if (-not $VerifyOnly -and -not $Thumbprint -and -not $SubjectName) {
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
        throw 'Pass -Thumbprint or -SubjectName to choose one.'
    }
    $Thumbprint = $certs[0].Thumbprint
    Write-Host "Certificate: $($certs[0].Subject)"
}

# ---- what to sign ------------------------------------------------------------
# Every .exe that ships, not just the launcher: AlignThree starts
# SpecTableConverter and AlignThreeAskPass as child processes, and an unsigned
# child undermines the signature on the parent. The installer is signed too, so
# SmartScreen sees a signed download.
#
# DLLs are deliberately not signed. Every one that ships came from Qt, not from
# this build -- the Qt Company's own signature is the accurate provenance, and
# re-signing someone else's binary with our certificate would assert authorship
# we do not have. It also slows a token-backed run considerably.
if (-not $Path) {
    $dist = Join-Path $repo 'dist'
    if (-not (Test-Path $dist)) { throw "No dist\ folder. Run package_windows.ps1 first." }
    $Path = @(
        Get-ChildItem $dist -Recurse -Include '*.exe' -File |
            Select-Object -ExpandProperty FullName
    )
}
# Expand wildcards here. PowerShell does not glob for a [string[]] parameter, so
# dist\*.exe arrives as a literal string; signtool would accept it and sign
# whatever it matched without ever saying what that was.
$Path = @($Path | ForEach-Object {
    if ($_ -match '[*?]') {
        $hits = @(Get-ChildItem -Path $_ -File -ErrorAction SilentlyContinue |
                  Select-Object -ExpandProperty FullName)
        if ($hits.Count -eq 0) { Write-Host "No files match: $_" -ForegroundColor Yellow }
        $hits
    } else { $_ }
})

# Say which arguments were discarded. Without this a mistyped switch or file name
# is dropped in silence, and the run either signs fewer files than asked for or
# reports the unhelpful 'Nothing to sign.'
$missing = @($Path | Where-Object { -not (Test-Path -LiteralPath $_) })
if ($missing.Count -gt 0) {
    Write-Host 'Not found, ignoring:' -ForegroundColor Yellow
    $missing | ForEach-Object { Write-Host "  $_" -ForegroundColor Yellow }
}
$Path = @($Path | Where-Object { Test-Path -LiteralPath $_ })
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
# Always by thumbprint: every selection path above resolved to one certificate,
# so signtool is never left to disambiguate.
$script:certSelector = @('/sha1', $Thumbprint)

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
    $r = Invoke-SignTool (@('sign') + $script:certSelector +
                          @('/fd', 'sha256', '/tr', $TimestampUrl,
                            '/td', 'sha256', '/q', $f))
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
