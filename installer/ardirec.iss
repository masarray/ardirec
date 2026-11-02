; ArDiRec per-user Windows installer, Inno Setup 6.
#ifndef MyAppVersion
  #error "Pass /DMyAppVersion=... to ISCC"
#endif
#ifndef MySourceDir
  #error "Pass /DMySourceDir=... to ISCC"
#endif
#ifndef MyOutputDir
  #error "Pass /DMyOutputDir=... to ISCC"
#endif
#define MyAppName "ArDiRec"
#define MyAppPublisher "ArDiRec contributors"
#define MyAppURL "https://github.com/masarray/ardirec"
#define MyAppExeName "ardirec.exe"

[Setup]
AppId={{1D3C5EA4-4557-4C9A-A7A5-26D7864E980A}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}/issues
AppUpdatesURL={#MyAppURL}/releases
DefaultDirName={localappdata}\Programs\ArDiRec
DefaultGroupName=ArDiRec
UsePreviousAppDir=yes
UsePreviousTasks=yes
DisableProgramGroupPage=yes
LicenseFile=..\LICENSE
OutputDir={#MyOutputDir}
OutputBaseFilename=ArDiRec-v{#MyAppVersion}-Windows-x64-Setup
SetupIconFile=..\icon\favicon.ico
UninstallDisplayIcon={app}\{#MyAppExeName}
UninstallDisplayName=ArDiRec
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
CloseApplications=yes
RestartApplications=no
SetupLogging=yes
ChangesAssociations=no

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Optional shortcuts:"; Flags: unchecked

[Files]
Source: "{#MySourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\ArDiRec"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\ArDiRec"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Launch ArDiRec"; Flags: nowait postinstall skipifsilent
