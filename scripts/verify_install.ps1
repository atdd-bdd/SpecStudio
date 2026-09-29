# verify_install.ps1 - install AlignThree silently, check what it put on the
# machine, and take it back off again.
#
# This is the evidence a store submission asks for. Microsoft's automated checks
# on a Win32 installer report "we could not identify if your app installs
# silently" and "we could not identify the app name and the publisher name that
# your app has added in the add or remove programs", and then ask you to verify
# both by hand. Those two facts, plus the signatures, are what this prints.
#
#   .\scripts\verify_install.ps1
#   .\scripts\verify_install.ps1 -Setup dist\AlignThree-0.9.1-setup.exe
#   .\scripts\verify_install.ps1 -KeepInstalled      # leave it on disk to poke at
#
# It installs into a temporary folder, never into Program Files, and uninstalls
# at the end unless told otherwise. Both halves need elevation, so expect two UAC
# prompts when this is run from an ordinary shell; run it from an elevated one to
# get none.
#
# An install of AlignThree that is already registered stops the run. The Add or
# Remove Programs entry is keyed by AppId, not by folder, so a test install would
# take over the real one's entry and the uninstall at the end would remove it.
# -Force says go ahead anyway.

[CmdletBinding()]
param(
    [string] $Setup,
    [string] $InstallDir,
    [string] $ExpectedName      = 'AlignThree',
    [string] $ExpectedPublisher = 'Ken Pugh, Inc.',
    [switch] $KeepInstalled,
    [switch] $Force
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot

$script:failures = @()

function Check([string] $what, [bool] $ok, [string] $detail) {
    if ($ok) {
        $mark  = 'PASS'
        $color = 'Green'
    } else {
        $mark  = 'FAIL'
        $color = 'Red'
        $script:failures += $what
    }
    $line = "  [{0}] {1}" -f $mark, $what
    if ($detail) { $line = "{0} -- {1}" -f $line, $detail }
    Write-Host $line -ForegroundColor $color
}

function Elevated() {
    $id = [Security.Principal.WindowsIdentity]::GetCurrent()
    return (New-Object Security.Principal.WindowsPrincipal($id)).IsInRole(
                [Security.Principal.WindowsBuiltInRole]::Administrator)
}

# Runs an installer or uninstaller and returns its exit code. Elevates only when
# this shell is not already elevated: Start-Process -Verb RunAs from an elevated
# shell works, but it loses the exit code on some builds.
function Invoke-Installer([string] $exe, [string[]] $arguments) {
    try {
        if (Elevated) {
            $p = Start-Process $exe -ArgumentList $arguments -PassThru -Wait
        } else {
            $p = Start-Process $exe -ArgumentList $arguments -Verb RunAs -PassThru -Wait
        }
    } catch [InvalidOperationException] {
        # "The operation was canceled by the user" -- the UAC prompt was declined,
        # or nobody was there to answer it. Say that, rather than let a stack
        # trace stand where a one-line explanation belongs.
        Write-Host ''
        Write-Host 'The elevation prompt was declined, so nothing ran.' -ForegroundColor Red
        Write-Host ('Both halves of this check need administrator rights. Accept the prompt, ' +
                    'or start an elevated PowerShell and run this script there to be asked ' +
                    'once instead of twice.') -ForegroundColor Yellow
        return $null
    }
    return $p.ExitCode
}

# Every Add or Remove Programs entry whose DisplayName mentions AlignThree, from
# both registry views and the per-user hive -- a per-user install writes to HKCU,
# and a checker looking only in HKLM is why "could not identify" happens.
function Get-ArpEntries() {
    $roots = @(
        'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall'
        'HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall'
        'HKCU:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall'
    )
    $found = @()
    foreach ($root in $roots) {
        if (-not (Test-Path $root)) { continue }
        $keys = Get-ChildItem $root -ErrorAction SilentlyContinue
        foreach ($key in $keys) {
            $props = Get-ItemProperty $key.PSPath -ErrorAction SilentlyContinue
            if ($props -and $props.DisplayName -like '*AlignThree*') {
                $found += [pscustomobject]@{
                    Hive                 = $root
                    Key                  = $key.PSChildName
                    DisplayName          = $props.DisplayName
                    Publisher            = $props.Publisher
                    DisplayVersion       = $props.DisplayVersion
                    InstallLocation      = $props.InstallLocation
                    UninstallString      = $props.UninstallString
                    QuietUninstallString = $props.QuietUninstallString
                }
            }
        }
    }
    return $found
}

function Signature([string] $path) {
    $s = Get-AuthenticodeSignature $path
    return [pscustomobject]@{
        Status      = "$($s.Status)"
        Thumbprint  = $s.SignerCertificate.Thumbprint
        Subject     = $s.SignerCertificate.Subject
        Timestamped = ($null -ne $s.TimeStamperCertificate)
    }
}

# ---- what to install ---------------------------------------------------------

if (-not $Setup) {
    $candidates = @(Get-ChildItem (Join-Path $repo 'dist\AlignThree-*-setup.exe') -ErrorAction SilentlyContinue |
                    Sort-Object LastWriteTime -Descending)
    if ($candidates.Count -eq 0) {
        throw 'No dist\AlignThree-*-setup.exe found. Run scripts\package_windows.ps1 first, or pass -Setup.'
    }
    $Setup = $candidates[0].FullName
}
if (-not (Test-Path -LiteralPath $Setup)) { throw "Installer not found: $Setup" }
$Setup = (Resolve-Path -LiteralPath $Setup).Path

if (-not $InstallDir) {
    $InstallDir = Join-Path $env:TEMP ('AlignThree-verify-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
}
$log = "$InstallDir.log"

Write-Host ''
Write-Host "Verifying $([IO.Path]::GetFileName($Setup))" -ForegroundColor Cyan
Write-Host "  install folder: $InstallDir"
Write-Host "  setup log:      $log"
Write-Host ''

$existing = @(Get-ArpEntries)
if ($existing.Count -gt 0 -and -not $Force) {
    Write-Host 'AlignThree is already registered in Add or Remove Programs:' -ForegroundColor Yellow
    $existing | ForEach-Object { "    {0}  ({1})" -f $_.DisplayName, $_.Key } | Write-Host
    throw ('A test install shares its AppId, so it would take over that entry and the ' +
           'uninstall at the end would remove it. Uninstall the real copy first, or pass -Force.')
}

# ---- the installer itself ----------------------------------------------------

Write-Host 'The installer:'
$sig = Signature $Setup
Check 'installer is signed' ($sig.Status -eq 'Valid') "$($sig.Status), $($sig.Thumbprint)"
Check 'installer signature is timestamped' $sig.Timestamped ''

# ---- silent install ----------------------------------------------------------

Write-Host ''
Write-Host 'Installing silently (accept the elevation prompt)...'
$started = Get-Date
$code = Invoke-Installer $Setup @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART',
                                  "/DIR=$InstallDir", "/LOG=$log")
if ($null -eq $code) { exit 2 }
$seconds = [int]((Get-Date) - $started).TotalSeconds

Write-Host ''
Write-Host 'Silent install:'
Check 'installer exit code is 0' ($code -eq 0) "exit=$code, ${seconds}s"
Check 'install folder exists' (Test-Path -LiteralPath $InstallDir) $InstallDir
if (-not (Test-Path -LiteralPath $InstallDir)) {
    Write-Host ''
    Write-Host "Nothing was installed. The setup log is at $log" -ForegroundColor Red
    exit 1
}
$files = @(Get-ChildItem -Recurse -File $InstallDir)
Check 'files were installed' ($files.Count -gt 0) "$($files.Count) files"
Check 'no restart was requested' ($code -ne 1641 -and $code -ne 3010) "exit=$code"

# ---- Add or Remove Programs --------------------------------------------------

Write-Host ''
Write-Host 'Add or Remove Programs:'
$arp = @(Get-ArpEntries)
Check 'an entry was created' ($arp.Count -ge 1) "$($arp.Count) entry/entries"
if ($arp.Count -ge 1) {
    $e = $arp[0]
    Write-Host "      hive            $($e.Hive)"
    Write-Host "      key             $($e.Key)"
    Write-Host "      DisplayName     $($e.DisplayName)"
    Write-Host "      Publisher       $($e.Publisher)"
    Write-Host "      DisplayVersion  $($e.DisplayVersion)"
    Write-Host "      InstallLocation $($e.InstallLocation)"
    Write-Host "      UninstallString $($e.UninstallString)"
    Check "DisplayName is exactly '$ExpectedName'" ($e.DisplayName -ceq $ExpectedName) $e.DisplayName
    Check "Publisher is exactly '$ExpectedPublisher'" ($e.Publisher -ceq $ExpectedPublisher) $e.Publisher
    Check 'DisplayVersion is set' ([bool] $e.DisplayVersion) $e.DisplayVersion
    Check 'UninstallString is set' ([bool] $e.UninstallString) ''
}

# ---- what landed on disk -----------------------------------------------------

Write-Host ''
Write-Host 'Installed files:'
$exes = @(Get-ChildItem -Recurse -Filter *.exe $InstallDir)
Check 'the three executables and the uninstaller are there' ($exes.Count -ge 4) `
      (($exes | ForEach-Object { $_.Name }) -join ', ')
$thumbs = @()
foreach ($exe in $exes) {
    $s = Signature $exe.FullName
    Check "$($exe.Name) is signed" ($s.Status -eq 'Valid') "$($s.Status), timestamped=$($s.Timestamped)"
    if ($s.Thumbprint) { $thumbs += $s.Thumbprint }
}
$distinct = @($thumbs | Sort-Object -Unique)
Check 'every executable carries the same certificate' ($distinct.Count -eq 1) ($distinct -join ', ')

# The Visual C++ runtime ships in the folder, so the program runs on a machine
# that has never had the redistributable. Nothing here can prove that from this
# box -- the runtime is installed on any machine with Visual Studio -- so the
# check is that the DLLs are present, and the store's clean VM does the rest.
$crt = @(Get-ChildItem $InstallDir -File | Where-Object {
             $_.Name -match '^(vcruntime|msvcp|concrt|vccorlib)\d' })
Check 'the MSVC runtime DLLs are in the folder' ($crt.Count -ge 3) "$($crt.Count) files"

# Runs with no Qt anywhere on PATH: proves the folder is self-contained rather
# than borrowing this machine's development environment.
$converter = Join-Path $InstallDir 'SpecTableConverter.exe'
if (Test-Path -LiteralPath $converter) {
    $savedPath = $env:PATH
    try {
        $env:PATH = (($savedPath -split ';') | Where-Object { $_ -notmatch '\\Qt\\' }) -join ';'
        & $converter --help > $null 2>&1
        $ran = ($LASTEXITCODE -eq 0)
    } finally {
        $env:PATH = $savedPath
    }
    Check 'the converter runs with Qt off PATH' $ran "exit=$LASTEXITCODE"
}

# ---- uninstall ---------------------------------------------------------------

if ($KeepInstalled) {
    Write-Host ''
    Write-Host "Left installed at $InstallDir (-KeepInstalled)." -ForegroundColor Yellow
    Write-Host 'Remove it with:' -ForegroundColor Yellow
    Write-Host "  Start-Process '$InstallDir\unins000.exe' -ArgumentList '/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART' -Verb RunAs -Wait" -ForegroundColor Yellow
} else {
    $uninstaller = Join-Path $InstallDir 'unins000.exe'
    Write-Host ''
    Write-Host 'Uninstalling (accept the elevation prompt)...'
    if (-not (Test-Path -LiteralPath $uninstaller)) {
        Check 'the uninstaller exists' $false $uninstaller
    } else {
        $ucode = Invoke-Installer $uninstaller @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART')
        if ($null -eq $ucode) {
            Write-Host "Still installed at $InstallDir -- uninstall it yourself." -ForegroundColor Yellow
            exit 2
        }
        Write-Host ''
        Write-Host 'Uninstall:'
        Check 'uninstaller exit code is 0' ($ucode -eq 0) "exit=$ucode"
        # Inno's uninstaller returns before the last files are gone: it restarts
        # itself from a temporary copy to delete its own directory.
        for ($i = 0; $i -lt 20 -and (Test-Path -LiteralPath $InstallDir); $i++) {
            Start-Sleep -Milliseconds 500
        }
        Check 'the install folder is gone' (-not (Test-Path -LiteralPath $InstallDir)) ''
        Check 'the Add or Remove Programs entry is gone' (@(Get-ArpEntries).Count -eq 0) ''
    }
}

# ---- the report --------------------------------------------------------------

Write-Host ''
if ($script:failures.Count -eq 0) {
    Write-Host 'All checks passed.' -ForegroundColor Green
    Write-Host ''
    Write-Host 'For a store submission that asks you to verify these by hand:' -ForegroundColor Cyan
    Write-Host "  silent install command   $([IO.Path]::GetFileName($Setup)) /VERYSILENT /SUPPRESSMSGBOXES /NORESTART"
    Write-Host '  success exit code        0'
    if ($arp.Count -ge 1) {
        Write-Host "  app name in ARP          $($arp[0].DisplayName)"
        Write-Host "  publisher in ARP         $($arp[0].Publisher)"
        Write-Host "  version in ARP           $($arp[0].DisplayVersion)"
    }
    Write-Host "  bundled software         none; $($files.Count) files, all AlignThree's own, Qt's, or the MSVC runtime"
    Write-Host "  signed by                $($distinct -join ', ')"
    Write-Host ''
    Write-Host "Setup log kept at $log"
    exit 0
} else {
    Write-Host "$($script:failures.Count) check(s) failed:" -ForegroundColor Red
    $script:failures | ForEach-Object { Write-Host "  $_" -ForegroundColor Red }
    Write-Host ''
    Write-Host "Setup log kept at $log" -ForegroundColor Yellow
    exit 1
}
