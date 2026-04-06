; Recoll Inno Setup Script

#ifndef MyAppVersion
#define MyAppVersion "1.43.13"
#endif
#define MyAppName "Recoll"
#define MyAppPublisher "Recoll"
#define MyAppURL "https://www.recoll.org/"

[Setup]
AppId={{8B7E4A5F-3C2D-4E1A-9F8B-6D5C4A3B2E1F}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
AllowNoIcons=yes
LicenseFile=..\src\COPYING
OutputDir=Output
OutputBaseFilename=recoll-{#MyAppVersion}-win64-setup
SetupIconFile=dist\recoll.ico
Compression=lzma
SolidCompression=yes
WizardStyle=modern
ArchitecturesInstallIn64BitMode=x64compatible
ArchitecturesAllowed=x64compatible

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
; All files from dist directory (executables, DLLs, Qt plugins, config, filters)
Source: "dist\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Recoll"; Filename: "{app}\recoll.exe"; IconFilename: "{app}\recoll.ico"
Name: "{group}\Recoll Index"; Filename: "{app}\recollindex.exe"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\recoll.exe"; IconFilename: "{app}\recoll.ico"; Tasks: desktopicon

[Registry]
Root: HKLM; Subkey: "SYSTEM\CurrentControlSet\Control\Session Manager\Environment"; \
    ValueType: expandsz; ValueName: "Path"; ValueData: "{olddata};{app}"; \
    Check: NeedsAddPath('{app}')

[Code]
function NeedsAddPath(Param: string): boolean;
var
  OrigPath: string;
begin
  if not RegQueryStringValue(HKLM,
    'SYSTEM\CurrentControlSet\Control\Session Manager\Environment',
    'Path', OrigPath)
  then begin
    Result := True;
    exit;
  end;
  Result := Pos(';' + UpperCase(Param) + ';', ';' + UpperCase(OrigPath) + ';') = 0;
end;
