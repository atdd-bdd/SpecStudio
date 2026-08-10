# Signing.ps1 - locating signtool and the code-signing certificate.
#
# Dot-sourced by sign_windows.ps1 (which signs finished artefacts) and by
# package_windows.ps1 (which hands the same tool and certificate to Inno Setup
# so the uninstaller stub is signed at compile time). Shared rather than copied:
# two answers to "which certificate do we sign with" is how a release ends up
# signed by the expired one.
#
# Nothing here prompts, exits or writes files. The callers decide what an
# absent token means -- fatal for sign_windows.ps1, merely a warning for a
# packaging run that is not a release.

# The Windows SDK installs signtool.exe under a versioned kit folder and does not
# put it on PATH. Take the newest x64 build.
function Find-SignTool {
    param([switch] $Quiet)
    $c = Get-Command signtool -ErrorAction SilentlyContinue
    if ($c) { return $c.Source }
    $kits = 'C:\Program Files (x86)\Windows Kits\10\bin'
    if (Test-Path $kits) {
        $found = Get-ChildItem $kits -Recurse -Filter signtool.exe -ErrorAction SilentlyContinue |
                 Where-Object { $_.FullName -match '\\x64\\' } |
                 Sort-Object FullName -Descending | Select-Object -First 1
        if ($found) { return $found.FullName }
    }
    if ($Quiet) { return $null }
    throw "signtool.exe not found. Install the Windows SDK (Signing Tools component)."
}

# The SafeNet client publishes the token's certificate into the current user's
# personal store while the token is plugged in, so signtool can reach it by
# thumbprint and the private key stays on the hardware.
#
# Expired certificates are filtered out here, not by the caller: this store holds
# a 2022 certificate whose CN is also "Ken Pugh, Inc.", and every ambiguity
# signtool reports comes out as the same unhelpful "No certificates were found
# that met all the given criteria".
function Get-SigningCerts {
    Get-ChildItem Cert:\CurrentUser\My -CodeSigningCert -ErrorAction SilentlyContinue |
        Where-Object { $_.NotAfter -gt (Get-Date) }
}
