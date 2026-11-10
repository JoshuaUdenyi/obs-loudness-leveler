; Inno Setup script for "Loudness Leveler for OBS" (Windows, 64-bit OBS Studio).
; Built automatically by GitHub Actions. To build by hand, install Inno Setup 6.3+ and run:
;   ISCC.exe /DAppVersion=1.0.0 /DStageDir=C:\path\to\release\RelWithDebInfo obs-loudness-leveler.iss
;
; The build output (StageDir) contains one folder named after the plugin:
;   obs-loudness-leveler\bin\64bit\obs-loudness-leveler.dll
;   obs-loudness-leveler\data\locale\en-US.ini
; OBS loads plugins in exactly that layout from %ProgramData%\obs-studio\plugins\,
; so the installer copies that folder there. No OBS folder lookup is needed.

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
DefaultDirName={commonappdata}\obs-studio\plugins\{#PluginId}
DisableDirPage=yes
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

[Files]
Source: "{#StageDir}\{#PluginId}\*"; DestDir: "{app}"; Excludes: "*.pdb"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#GuideFile}"; DestDir: "{app}\data"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\{#AppName}\Quick Guide"; Filename: "{app}\data\{#GuideFile}"
Name: "{autoprograms}\{#AppName}\Uninstall"; Filename: "{uninstallexe}"

[Run]
Filename: "{app}\data\{#GuideFile}"; Description: "Open the quick guide"; Flags: postinstall shellexec skipifsilent

[Code]
function ObsIsRunning: Boolean;
var
  ResultCode: Integer;
begin
  { "find" returns 0 when obs64.exe appears in the process list }
  Result := Exec(ExpandConstant('{sys}\cmd.exe'),
    '/C tasklist /FI "IMAGENAME eq obs64.exe" | find /I "obs64.exe"',
    '', SW_HIDE, ewWaitUntilTerminated, ResultCode) and (ResultCode = 0);
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
