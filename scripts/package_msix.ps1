# package_msix.ps1 - build an MSIX package of AlignThree and sign it.
#
# Produces dist\AlignThree-<version>-x64.msix from the same staged folder the
# zip and the installer come from, so all three ship identical binaries.
#
#   .\scripts\package_msix.ps1                  # sign with the token
#   .\scripts\package_msix.ps1 -SkipSigning     # unsigned, for a Store upload
#   .\scripts\package_msix.ps1 -StoreIdentity   # the Store's identity values
#
# ---- signed, or for the Store? -----------------------------------------------
#
# These are two different packages and only one of them is signed here.
#
# A package you host yourself is signed with your own certificate, and the
# manifest's Publisher must equal that certificate's subject exactly -- the
# whole distinguished name as Windows renders it. This script reads the subject
# off the certificate rather than copying it into a file, because a manifest
# that disagrees with the certificate by one character fails to sign, and the
# error does not say which character.
#
# A package for the Microsoft Store is uploaded UNSIGNED: the Store signs it
# with Microsoft's own certificate. Its identity does not come from your
# certificate either -- Partner Center assigns it, under Product identity. Pass
# -StoreIdentity with those values and -SkipSigning:
#
#   .\scripts\package_msix.ps1 -SkipSigning `
#        -StoreIdentity @{ Name = '12345KenPughInc.AlignThree'
#                          Publisher = 'CN=ABCD1234-...'
#                          PublisherDisplayName = 'Ken Pugh, Inc.' }
#
# ---- package identity --------------------------------------------------------
#
# Identity Name plus Publisher is the identity of the application, so changing
# either makes Windows treat the next version as a different program: it will
# not upgrade in place, and users have to uninstall first. A certificate renewal
# therefore has to keep the same subject. The two certificates on this machine
# do NOT have the same subject -- the expired 2022 one carried STREET and
# PostalCode -- so they are not interchangeable for this purpose.

[CmdletBinding()]
param(
    [string]    $Version,
    [string]    $Stage,
    [string]    $Thumbprint,
    [string]    $TimestampUrl = 'http://timestamp.sectigo.com',
    [hashtable] $StoreIdentity,
    [switch]    $SkipSigning
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'Signing.ps1')

function Need-Path([string] $path, [string] $what) {
    if (-not (Test-Path -LiteralPath $path)) { throw "$what not found: $path" }
    return $path
}

# makeappx lives beside signtool in the Windows SDK, so the search that finds
# one finds the other.
function Find-MakeAppx {
    $signtool = Find-SignTool -Quiet
    if ($signtool) {
        $candidate = Join-Path (Split-Path -Parent $signtool) 'makeappx.exe'
        if (Test-Path -LiteralPath $candidate) { return $candidate }
    }
    $found = Get-ChildItem 'C:\Program Files (x86)\Windows Kits\10\bin\*\x64\makeappx.exe' `
                           -ErrorAction SilentlyContinue |
             Sort-Object FullName -Descending | Select-Object -First 1
    if ($found) { return $found.FullName }
    throw 'makeappx.exe not found. Install the Windows SDK (Windows App Certification / signing tools).'
}

# ---- version and staged files ------------------------------------------------

if (-not $Version) {
    $Version = $null
    try   { $Version = (& git -C $repo describe --tags --abbrev=0 2>&1 | Select-Object -First 1) }
    catch { $Version = $null }
    if ($LASTEXITCODE -ne 0) { $Version = $null }
    if (-not $Version) {
        $m = Select-String -Path (Join-Path $repo 'CMakeLists.txt') -Pattern 'project\(AlignThree VERSION ([0-9.]+)'
        $Version = if ($m) { $m.Matches[0].Groups[1].Value } else { '0.0.0' }
    }
    $Version = "$Version".Trim().TrimStart('v')
}

# An MSIX version is four parts and the last must be 0 -- the Store reserves it.
$parts = @($Version.Split('.'))
while ($parts.Count -lt 3) { $parts += '0' }
$msixVersion = "{0}.{1}.{2}.0" -f $parts[0], $parts[1], $parts[2]

if (-not $Stage) { $Stage = Join-Path $repo "dist\AlignThree-$Version-windows-x64" }
Need-Path $Stage 'Staged folder' | Out-Null
Need-Path (Join-Path $Stage 'AlignThree.exe') 'AlignThree.exe' | Out-Null

$logos = Need-Path (Join-Path $repo 'resources\logos') 'Logos (run scripts\make_logos.py)'

Write-Host "AlignThree $msixVersion (MSIX)" -ForegroundColor Cyan
Write-Host "  from $Stage"

# ---- who the package says it is ----------------------------------------------

$publisherDisplayName = 'Ken Pugh, Inc.'
$identityName = 'KenPughInc.AlignThree'

if ($StoreIdentity) {
    foreach ($key in @('Name', 'Publisher')) {
        if (-not $StoreIdentity.ContainsKey($key)) {
            throw "-StoreIdentity needs at least Name and Publisher, from Partner Center's Product identity page."
        }
    }
    $identityName = $StoreIdentity.Name
    $publisher    = $StoreIdentity.Publisher
    if ($StoreIdentity.ContainsKey('PublisherDisplayName')) {
        $publisherDisplayName = $StoreIdentity.PublisherDisplayName
    }
    Write-Host "  identity from Partner Center: $identityName"
    if (-not $SkipSigning) {
        Write-Host 'A Store package is uploaded unsigned -- the Store signs it. Pass -SkipSigning.' -ForegroundColor Yellow
    }
} else {
    # The certificate decides the Publisher string, so read it off the
    # certificate rather than trusting a copy of it.
    $cert = $null
    if ($Thumbprint) {
        $thumb = ($Thumbprint -replace '[^0-9A-Fa-f]', '').ToUpperInvariant()
        $cert = Get-ChildItem Cert:\CurrentUser\My | Where-Object { $_.Thumbprint -eq $thumb }
        if (-not $cert) { throw "No certificate in CurrentUser\My with thumbprint $thumb." }
    } else {
        $certs = @(Get-SigningCerts)
        if ($certs.Count -eq 0) {
            throw ('No code-signing certificate is visible, so the package Publisher cannot be ' +
                   'determined. Plug in the token and start SafeNet Authentication Client, or ' +
                   'pass -StoreIdentity for a Store package.')
        }
        if ($certs.Count -gt 1) {
            Write-Host 'More than one code-signing certificate is available:' -ForegroundColor Yellow
            $certs | ForEach-Object { "  {0}  {1}" -f $_.Thumbprint, $_.Subject } | Write-Host
            throw 'Name one with -Thumbprint.'
        }
        $cert = $certs[0]
    }
    $publisher = $cert.Subject
    Write-Host "  publisher    $publisher"
    Write-Host "  certificate  $($cert.Thumbprint), expires $($cert.NotAfter.ToString('yyyy-MM-dd'))"
}

# ---- the layout --------------------------------------------------------------

$work = Join-Path $repo 'dist\msix-layout'
if (Test-Path $work) { Remove-Item $work -Recurse -Force }
New-Item -ItemType Directory -Path $work -Force | Out-Null

Write-Host 'Laying out the package...'
Copy-Item (Join-Path $Stage '*') $work -Recurse -Force

# Assets are named without the scale qualifier in the manifest; makeappx pairs
# each Square150x150Logo.scale-200.png with the Square150x150Logo it declares.
$assets = Join-Path $work 'Assets'
New-Item -ItemType Directory -Path $assets -Force | Out-Null
Copy-Item (Join-Path $logos '*.png') $assets -Force
# A copy without the qualifier too, so the manifest's literal paths resolve even
# where the resource index is not consulted (the installer dialog, for one).
foreach ($base in @('Square44x44Logo', 'Square150x150Logo', 'StoreLogo')) {
    Copy-Item (Join-Path $logos "$base.scale-100.png") (Join-Path $assets "$base.png") -Force
}

$manifestTemplate = Need-Path (Join-Path $PSScriptRoot 'AppxManifest.xml') 'AppxManifest.xml'
$manifest = (Get-Content -Raw $manifestTemplate).
                Replace('{VERSION}', $msixVersion).
                Replace('{PUBLISHER}', [Security.SecurityElement]::Escape($publisher)).
                Replace('Name="KenPughInc.AlignThree"', "Name=""$identityName""").
                Replace('<PublisherDisplayName>Ken Pugh, Inc.</PublisherDisplayName>',
                        "<PublisherDisplayName>$([Security.SecurityElement]::Escape($publisherDisplayName))</PublisherDisplayName>")
Set-Content (Join-Path $work 'AppxManifest.xml') $manifest -Encoding UTF8

# ---- pack --------------------------------------------------------------------

$makeappx = Find-MakeAppx
$outDir = Join-Path $repo 'dist'
$msix = Join-Path $outDir "AlignThree-$Version-x64.msix"
if (Test-Path $msix) { Remove-Item $msix -Force }

Write-Host 'Packing...'
# /o overwrite, /d the layout directory. Its output goes to stdout; PowerShell
# 5.1 turns any stderr line from a native tool into a terminating error, so
# capture both and decide on the exit code.
$packLog = & $makeappx pack /o /d $work /p $msix 2>&1
if ($LASTEXITCODE -ne 0) {
    $packLog | ForEach-Object { Write-Host "  $_" -ForegroundColor Red }
    throw "makeappx failed with exit code $LASTEXITCODE."
}

Remove-Item $work -Recurse -Force

# ---- sign --------------------------------------------------------------------

if ($SkipSigning) {
    Write-Host ''
    Write-Host "Unsigned: $msix" -ForegroundColor Yellow
    if ($StoreIdentity) {
        Write-Host 'Upload this to Partner Center. The Store signs it.' -ForegroundColor Yellow
    } else {
        Write-Host 'Nothing will install this until it is signed.' -ForegroundColor Yellow
    }
    exit 0
}

Write-Host 'Signing (enter the token PIN if prompted)...'
$signtool = Find-SignTool
$args = @('sign', '/fd', 'SHA256', '/tr', $TimestampUrl, '/td', 'SHA256',
          '/sha1', $cert.Thumbprint, $msix)
$signLog = & $signtool @args 2>&1
if ($LASTEXITCODE -ne 0) {
    $signLog | ForEach-Object { Write-Host "  $_" -ForegroundColor Red }
    throw ("signtool failed with exit code $LASTEXITCODE. If it says the publisher name does " +
           "not match, the manifest Publisher and the certificate subject have diverged.")
}

$sig = Get-AuthenticodeSignature $msix
Write-Host ''
Write-Host "Done: $msix" -ForegroundColor Green
Write-Host "  signature    $($sig.Status), $($sig.SignerCertificate.Thumbprint)"
Write-Host "  timestamped  $($null -ne $sig.TimeStamperCertificate)"
Write-Host ("  size         {0:N1} MB" -f ((Get-Item $msix).Length / 1MB))
Write-Host ''
Write-Host 'Install it with:  Add-AppxPackage <path>' -ForegroundColor Cyan
Write-Host 'Remove it with:   Get-AppxPackage *AlignThree* | Remove-AppxPackage' -ForegroundColor Cyan
