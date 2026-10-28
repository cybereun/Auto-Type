#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
[Setup]
AppId={{DE23B5A2-0AB8-4E7F-8EE5-62AF512A894B}
AppName=Auto-Type
AppVersion={#AppVersion}
AppVerName=Auto-Type v{#AppVersion}
AppPublisher=cybereun
DefaultDirName={localappdata}\Programs\AutoType
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=..\out
OutputBaseFilename=Auto-Type-Setup-{#AppVersion}
SetupIconFile=..\resources\autotype.ico
UninstallDisplayIcon={app}\Auto-Type.exe
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
DefaultGroupName=Auto-Type
DisableProgramGroupPage=yes
CloseApplications=yes

[Languages]
Name: "korean"; MessagesFile: "compiler:Languages\Korean.isl"

[Tasks]
Name: "desktopicon"; Description: "바탕화면에 바로가기 만들기"; GroupDescription: "바로가기"

[Files]
Source: "..\build-x64\Release\AutoTypePreview.exe"; DestDir: "{app}"; DestName: "Auto-Type.exe"; Flags: ignoreversion
Source: "..\USAGE.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\THIRD_PARTY_NOTICES.md"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\Auto-Type"; Filename: "{app}\Auto-Type.exe"; IconFilename: "{app}\Auto-Type.exe"
Name: "{autodesktop}\Auto-Type"; Filename: "{app}\Auto-Type.exe"; IconFilename: "{app}\Auto-Type.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\Auto-Type.exe"; Description: "Auto-Type 실행"; Flags: nowait postinstall skipifsilent
