; Inno Setup script for the Infinit Lab Windows installer.
; The build script passes AppVersion, SourceDir and OutputDir with /D.

#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\..\app\build\windows\x64\runner\Release"
#endif
#ifndef OutputDir
  #define OutputDir "..\..\release"
#endif

[Setup]
AppId={{5B1E4C2A-9D77-4A31-9C0E-7F3D2B6A8E14}
AppName=Infinit Lab
AppVersion={#AppVersion}
AppPublisher=Infinit Lab
AppPublisherURL=https://github.com/
AppSupportURL=https://github.com/
DefaultDirName={autopf}\Infinit Lab
DefaultGroupName=Infinit Lab
DisableProgramGroupPage=yes
OutputDir={#OutputDir}
OutputBaseFilename=InfinitLab-{#AppVersion}-Setup
SetupIconFile=..\..\app\assets\branding\infinit-icon.ico
UninstallDisplayIcon={app}\infinit.exe
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
MinVersion=10.0

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional icons:"; Flags: unchecked

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Infinit Lab"; Filename: "{app}\infinit.exe"
Name: "{group}\Uninstall Infinit Lab"; Filename: "{uninstallexe}"
Name: "{autodesktop}\Infinit Lab"; Filename: "{app}\infinit.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\infinit.exe"; Description: "Launch Infinit Lab"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: filesandordirs; Name: "{app}"
