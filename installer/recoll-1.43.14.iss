; Inno Setup script for Recoll 1.43.14 (Windows x64)

#define MyAppName "Recoll"
#define MyAppVersion "1.43.14"
#define MyAppPublisher "Recoll.org"
#define MyAppURL "http://www.recoll.org"
#define MyAppExeName "recoll.exe"
#define IdxAppName "Recollindex background indexer"
#define IdxAppExeName "recollindex.exe"
#define DistDir "C:\work\recoll-14\recoll\dist"

[Setup]
AppId={{E9BC39EC-0E3D-4DDA-8DA0-FDB8ED16DC8D}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
ArchitecturesInstallIn64BitMode=x64compatible
ArchitecturesAllowed=x64compatible
DefaultGroupName={#MyAppName}
OutputDir=C:\work\recoll-14\recoll\installer\output
Compression=lzma
SolidCompression=yes
PrivilegesRequired=admin
DefaultDirName={commonpf}\{#MyAppName}
LicenseFile={#DistDir}\COPYING.txt
OutputBaseFilename=recoll-setup-{#MyAppVersion}-x64
WizardStyle=modern

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
; VC++ redistributable runtime. Extracted by VCRedistNeedsInstall(), if needed.
Source: "{#DistDir}\vc_redist.x64.exe"; DestDir: {tmp}; Flags: dontcopy
; Main executables and all deployed dependencies
Source: "{#DistDir}\recoll.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#DistDir}\recollindex.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#DistDir}\recollq.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#DistDir}\*"; DestDir: "{app}"; Excludes: "vc_redist.x64.exe,recoll.exe,recollindex.exe,recollq.exe"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\{#IdxAppName}"; Filename: "{app}\{#IdxAppExeName}"; IconFilename: "{app}\{#MyAppExeName}"; Parameters: "-m -w 0"; Flags: runminimized preventpinning
Name: "{commondesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[CustomMessages]
InstallingVCredist=Installing Microsoft VC++ redistributable

[Run]
Filename: "{tmp}\vc_redist.x64.exe"; \
  StatusMsg: "{cm:InstallingVCredist}"; \
  Parameters: "/quiet /norestart"; Check: VCRedistNeedsInstall; Flags: waituntilterminated
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[Code]
function VCRedistNeedsInstall: Boolean;
var
  Version: String;
begin
  if RegQueryStringValue(HKEY_LOCAL_MACHINE,
       'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64', 'Version',
       Version) then
  begin
    Log('VC Redist Version check : found ' + Version);
    Result := (CompareStr(Version, 'v14.42.34438.00') < 0);
  end
  else
  begin
    Result := True;
  end;
  if (Result) then
  begin
    ExtractTemporaryFile('vc_redist.x64.exe');
  end;
end;
