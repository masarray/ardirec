; ArdIREC Windows installer
; Canonical brand identity is sourced from /icon.
; CI can override version/source, for example:
; ISCC.exe /DMyAppVersion=0.2.0-rc.1 /DMySourceDir=... installer\ardirec.iss

#ifndef MyAppVersion
  #define MyAppVersion "dev"
#endif

#ifndef MySourceDir
  #define MySourceDir "..\dist\ardirec-windows-x64"
#endif

#define MyAppName "ArdIREC"
#define MyAppPublisher "ArdIREC contributors"
#define MyAppURL "https://github.com/masarray/ardirec"
#define MyAppExeName "ardirec.exe"

[Setup]
AppId={{1D3C5EA4-4557-4C9A-A7A5-26D7864E980A}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}/issues
AppUpdatesURL={#MyAppURL}/releases
DefaultDirName={localappdata}\Programs\ArdIREC
DefaultGroupName=ArdIREC
DisableProgramGroupPage=yes
LicenseFile=..\LICENSE
OutputDir=..\dist
OutputBaseFilename=ardirec-v{#MyAppVersion}-windows-x64-setup
SetupIconFile=..\icon\favicon.ico
UninstallDisplayIcon={app}\{#MyAppExeName}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Additional shortcuts:"; Flags: unchecked

[Files]
Source: "{#MySourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\ArdIREC"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\ArdIREC"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Launch ArdIREC"; Flags: nowait postinstall skipifsilent
