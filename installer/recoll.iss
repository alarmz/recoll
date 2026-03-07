; Recoll Inno Setup Script

#ifndef MyAppVersion
#define MyAppVersion "1.43.13"
#endif

#define MyAppName "Recoll"
#define MyAppPublisher "Recoll"
#define MyAppURL "https://www.recoll.org/"
#define MyAppExeName "recollindex.exe"

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
SetupIconFile=..\src\desktop\recoll.ico
Compression=lzma
SolidCompression=yes
WizardStyle=modern
ArchitecturesInstallIn64BitMode=x64compatible
ArchitecturesAllowed=x64compatible

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
; Main executables
Source: "..\src\build_win\recollindex.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\src\build_win\recollq.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\src\build_win\recoll.exe"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
; Library
Source: "..\src\build_win\recoll.lib"; DestDir: "{app}\lib"; Flags: ignoreversion
; Configuration examples
Source: "..\src\sampleconf\*"; DestDir: "{app}\examples"; Flags: ignoreversion recursesubdirs
; Filters
Source: "..\src\filters\*"; DestDir: "{app}\filters"; Flags: ignoreversion recursesubdirs
; Documentation
Source: "..\src\doc\user\usermanual.html"; DestDir: "{app}\doc"; Flags: ignoreversion
Source: "..\src\doc\user\docbook-xsl.css"; DestDir: "{app}\doc"; Flags: ignoreversion
; vcpkg DLLs
Source: "..\src\build_win\*.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
; Qt platform plugin (required for GUI, path configurable via /DQtPlatformPluginDir)
#ifndef QtPlatformPluginDir
#define QtPlatformPluginDir "C:\vcpkg\installed\x64-windows\Qt6\plugins\platforms"
#endif
Source: "{#QtPlatformPluginDir}\qwindows.dll"; DestDir: "{app}\platforms"; Flags: ignoreversion skipifsourcedoesntexist

[Icons]
Name: "{group}\Recoll"; Filename: "{app}\recoll.exe"
Name: "{group}\Recoll Index"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"

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
