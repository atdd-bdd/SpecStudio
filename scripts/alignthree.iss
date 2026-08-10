; alignthree.iss - Inno Setup script for the AlignThree installer.
;
; Not run directly: scripts\package_windows.ps1 stages the files and passes
; AppVersion, StageDir and OutDir in. Compile by hand with
;   ISCC /DAppVersion=0.1.0 /DStageDir=..\dist\AlignThree-0.1.0-windows-x64 /DOutDir=..\dist scripts\alignthree.iss

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef StageDir
  #error StageDir must be defined (the folder holding the staged files)
#endif
#ifndef OutDir
  #define OutDir "."
#endif

#define AppName    "AlignThree"
; Must match the CN on the signing certificate. UAC shows the certificate
; subject, the wizard shows this, and two different company names either side of
; the elevation prompt is exactly the sort of thing that makes a user cancel.
#define AppPublisher "Ken Pugh, Inc."
#define AppExe     "AlignThree.exe"

[Setup]
AppId={{7F3B2C14-9E5A-4C2D-8B71-6A0E4D3F91C2}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
OutputDir={#OutDir}
OutputBaseFilename={#AppName}-{#AppVersion}-setup
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
; x64 only: the Qt build and both helpers are 64-bit.
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; Per-machine when elevated, per-user otherwise, so it installs without admin.
PrivilegesRequiredOverridesAllowed=dialog
UninstallDisplayIcon={app}\{#AppExe}
DisableProgramGroupPage=yes

; Signing, only when the caller supplied a sign tool.
;
; unins000.exe is not built here -- Inno generates it on the *user's* machine
; from a stub embedded in the installer, so sign_windows.ps1 can never reach it.
; The result was an unsigned uninstaller, and SmartScreen warning people while
; they were trying to remove the program. SignedUninstaller signs that stub at
; compile time instead; the uninstall data goes to unins000.dat, so the .exe laid
; down on disk is a byte-for-byte copy of what was signed and the signature
; survives.
;
; The same SignTool also signs the finished installer, so a compile with signing
; enabled needs no separate sign_windows.ps1 pass over the setup .exe.
;
; Guarded by a define because a compile with SignTool named but not configured
; fails outright, and building an unsigned test installer -- no token, no PIN --
; has to keep working. package_windows.ps1 passes both /DSignUninstaller and
; /Ssigntool=<command> together or neither.
#ifdef SignUninstaller
SignTool=signtool
SignedUninstaller=yes
#endif

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Additional shortcuts:"; Flags: unchecked

[Files]
; The whole staged folder: AlignThree.exe, SpecTableConverter.exe,
; AlignThreeAskPass.exe, the Qt DLLs and the Qt plugin subfolders. The three
; executables must stay in one directory -- AlignThree finds the other two
; through its own applicationDirPath().
Source: "{#StageDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#AppName}";           Filename: "{app}\{#AppExe}"
Name: "{group}\Uninstall {#AppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}";     Filename: "{app}\{#AppExe}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExe}"; Description: "Launch {#AppName}"; Flags: nowait postinstall skipifsilent

[Code]
// Qt applications built with MSVC need the Visual C++ runtime. Rather than
// redistribute the DLLs loose, check for it and point the user at Microsoft's
// installer -- a missing runtime otherwise shows up as an unexplained failure
// to start.
function VCRuntimePresent(): Boolean;
var
  Installed: Cardinal;
begin
  Result := RegQueryDWordValue(HKLM, 'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64',
                               'Installed', Installed) and (Installed = 1);
  if not Result then
    Result := RegQueryDWordValue(HKLM, 'SOFTWARE\WOW6432Node\Microsoft\VisualStudio\14.0\VC\Runtimes\x64',
                                 'Installed', Installed) and (Installed = 1);
end;

function InitializeSetup(): Boolean;
begin
  Result := True;
  if not VCRuntimePresent() then
    if MsgBox('AlignThree needs the Microsoft Visual C++ 2015-2022 Redistributable (x64),'
              + #13#10 + 'which does not appear to be installed.'
              + #13#10#13#10 + 'Install AlignThree anyway?'
              + #13#10 + 'You can get the runtime from https://aka.ms/vs/17/release/vc_redist.x64.exe',
              mbConfirmation, MB_YESNO) = IDNO then
      Result := False;
end;
