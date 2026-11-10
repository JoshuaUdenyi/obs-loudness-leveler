; Inno Setup script for "Loudness Leveler for OBS" (Windows, 64-bit OBS Studio).
; Built automatically by GitHub Actions. To build by hand, install Inno Setup 6.3+ and run:
;   ISCC.exe /DAppVersion=1.0.0 /DStageDir=C:\path\to\release\RelWithDebInfo obs-loudness-leveler.iss

#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#ifndef StageDir
  #define StageDir "..\release\RelWithDebInfo"
#endif

#define AppName "Loudness Leveler for OBS"
#define PluginId "obs-loudness-leveler"
#define GuideFile "Loudness-Leveler-Guide.html"

[Setup]
AppId={{B7D3F3A2-6C1E-4B8E-9A55-3F1D2C7E9A10}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=Loudness Leveler
DefaultDirName={code:GetObsDir}
DirExistsWarning=no
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=..\release
OutputBaseFilename=Loudness-Leveler-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName={#AppName}
VersionInfoVersion={#AppVersion}

[Messages]
SelectDirDesc=Where is OBS Studio installed?
SelectDirLabel3=Setup will add the plugin to this OBS Studio folder.
SelectDirBrowseLabel=This is usually correct. If you installed OBS somewhere else, click Browse and choose that folder. Then click Next.

[Files]
Source: "{#StageDir}\obs-plugins\*"; DestDir: "{app}\obs-plugins"; Excludes: "*.pdb"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#StageDir}\data\*"; DestDir: "{app}\data"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#GuideFile}"; DestDir: "{app}\data\obs-plugins\{#PluginId}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\{#AppName}\Quick Guide"; Filename: "{app}\data\obs-plugins\{#PluginId}\{#GuideFile}"
Name: "{autoprograms}\{#AppName}\Uninstall"; Filename: "{uninstallexe}"

[Run]
Filename: "{app}\data\obs-plugins\{#PluginId}\{#GuideFile}"; Description: "Open the quick guide"; Flags: postinstall shellexec skipifsilent

[Code]
{ Finds the OBS Studio folder: OBS's own installer records it in the registry. }
function GetObsDir(Param: String): String;
var
  Path: String;
begin
  if RegQueryStringValue(HKLM64, 'SOFTWARE\OBS Studio', '', Path) and DirExists(Path) then
    Result := Path
  else if RegQueryStringValue(HKLM32, 'SOFTWARE\OBS Studio', '', Path) and DirExists(Path) then
    Result := Path
  else
    Result := ExpandConstant('{commonpf64}\obs-studio');
end;

function ObsIsRunning: Boolean;
var
  ResultCode: Integer;
begin
  { "find" returns 0 when obs64.exe appears in the process list }
  Result := Exec(ExpandConstant('{sys}\cmd.exe'),
    '/C tasklist /FI "IMAGENAME eq obs64.exe" | find /I "obs64.exe"',
    '', SW_HIDE, ewWaitUntilTerminated, ResultCode) and (ResultCode = 0);
end;

{ Warn if the chosen folder does not look like OBS Studio. }
function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if CurPageID = wpSelectDir then
  begin
    if not FileExists(AddBackslash(WizardDirValue) + 'bin\64bit\obs64.exe') then
      Result := MsgBox('OBS Studio was not found in this folder (obs64.exe is missing).' + #13#10 + #13#10 +
        'Install here anyway?', mbConfirmation, MB_YESNO or MB_DEFBUTTON2) = IDYES;
  end;
end;

{ OBS must be closed so it can load the plugin on the next start. }
function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  Result := '';
  while ObsIsRunning do
  begin
    if MsgBox('OBS Studio is running.' + #13#10 + #13#10 +
      'Please close OBS completely, then click Retry.', mbError, MB_RETRYCANCEL) = IDCANCEL then
    begin
      Result := 'Setup stopped because OBS Studio is still running. Close OBS and run Setup again.';
      Exit;
    end;
  end;
end;
