; Auto-Type installer. Build: iscc /DAppVersion=0.1.0 installer\AutoType.iss
#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif

[Setup]
AppId={{103733A2-F0D5-4DF0-B88A-D6281528AC32}
AppName=Auto-Type
AppVersion={#AppVersion}
AppVerName=Auto-Type {#AppVersion}
AppPublisher=cybereun
AppPublisherURL=https://github.com/cybereun/Auto-Type
DefaultDirName={autopf}\AutoType
DisableProgramDirPage=yes
DisableDirPage=yes
DisableReadyPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=..\out
OutputBaseFilename=AutoType-Setup-{#AppVersion}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName=Auto-Type (한영 자동) {#AppVersion}
CloseApplications=no
RestartIfNeededByRun=no

[Languages]
Name: "korean"; MessagesFile: "compiler:Languages\Korean.isl"

[Files]
; regserver registers the TSF input method; the uninstaller unregisters it automatically.
Source: "..\build-x64\Release\AutoType.dll"; DestDir: "{app}\x64"; Flags: regserver 64bit ignoreversion restartreplace uninsrestartdelete
Source: "..\build-x86\Release\AutoType.dll"; DestDir: "{app}\x86"; Flags: regserver 32bit ignoreversion restartreplace uninsrestartdelete
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\THIRD_PARTY_NOTICES.md"; DestDir: "{app}"; Flags: ignoreversion

[Messages]
korean.FinishedLabel=설치가 끝났습니다.%n%nWin + Space 를 눌러 "Auto-Type (한영 자동)"을 고르세요.%n이미 열려 있던 앱은 다시 실행해야 인식합니다.%n%n원래 입력기로 돌아가려면 Win + Space 로 "Microsoft 입력기"를 고르면 됩니다.
