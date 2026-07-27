; specstudio.iss - Inno Setup script for the SpecStudio installer.
;
; Not run directly: scripts\package_windows.ps1 stages the files and passes
; AppVersion, StageDir and OutDir in. Compile by hand with
;   ISCC /DAppVersion=0.1.0 /DStageDir=..\dist\SpecStudio-0.1.0-windows-x64 /DOutDir=..\dist scripts\specstudio.iss

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef StageDir
  #error StageDir must be defined (the folder holding the staged files)
#endif
#ifndef OutDir
  #define OutDir "."
#endif

#define AppName    "SpecStudio"
#define AppPublisher "Pugh-Killeen Associates"
#define AppExe     "SpecStudio.exe"

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

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Additional shortcuts:"; Flags: unchecked

[Files]
; The whole staged folder: SpecStudio.exe, SpecTableConverter.exe,
; SpecStudioAskPass.exe, the Qt DLLs and the Qt plugin subfolders. The three
; executables must stay in one directory -- SpecStudio finds the other two
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
    if MsgBox('SpecStudio needs the Microsoft Visual C++ 2015-2022 Redistributable (x64),'
              + #13#10 + 'which does not appear to be installed.'
              + #13#10#13#10 + 'Install SpecStudio anyway?'
              + #13#10 + 'You can get the runtime from https://aka.ms/vs/17/release/vc_redist.x64.exe',
              mbConfirmation, MB_YESNO) = IDNO then
      Result := False;
end;
