; Auto-Type installer. Build: iscc /DAppVersion=0.1.0 installer\AutoType.iss
#ifndef AppVersion
  #define AppVersion "0.2.8"
#endif

[Setup]
AppId={{103733A2-F0D5-4DF0-B88A-D6281528AC32}
AppName=Auto-Type
AppVersion={#AppVersion}
AppVerName=Auto-Type {#AppVersion}
AppPublisher=cybereun
AppPublisherURL=https://github.com/cybereun/Auto-Type
DefaultDirName={autopf}\AutoType
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
DefaultGroupName=Auto-Type
DisableProgramGroupPage=yes
UninstallDisplayName=Auto-Type (한영 자동) {#AppVersion}
CloseApplications=no
RestartIfNeededByRun=no

[Languages]
Name: "korean"; MessagesFile: "compiler:Languages\Korean.isl"

[Files]
; Each version goes into its own folder, so an update never has to overwrite a DLL that running apps
; still have loaded (that is what used to force a reboot). regserver points Windows at the new copy;
; apps pick it up when reopened. The uninstaller unregisters it, so no new app loads it afterwards.
Source: "..\build-x64\Release\AutoType.dll"; DestDir: "{app}\{#AppVersion}\x64"; Flags: regserver 64bit ignoreversion
Source: "..\build-x86\Release\AutoType.dll"; DestDir: "{app}\{#AppVersion}\x86"; Flags: regserver 32bit ignoreversion
Source: "..\scripts\debug-on.bat"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\scripts\debug-off.bat"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\scripts\register-user-input.ps1"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\THIRD_PARTY_NOTICES.md"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\Auto-Type 진단 기록 켜기"; Filename: "{app}\debug-on.bat"
Name: "{group}\Auto-Type 진단 기록 끄고 보기"; Filename: "{app}\debug-off.bat"

[Messages]
korean.FinishedLabel=설치가 끝났습니다.%n%nWin + Space 를 눌러 "Auto-Type (한영 자동)"을 고르세요.%n이미 열려 있던 앱은 다시 실행해야 인식합니다. 재부팅은 필요 없습니다.%n%n원래 입력기로 돌아가려면 Win + Space 로 "Microsoft 입력기"를 고르면 됩니다.

[Code]
// Removes folders of older versions. A folder still loaded by an open app is skipped and retried on
// the next install or cleaned up at the next sign-in, never with a reboot prompt.
procedure RemoveOldVersions();
var
  Found: TFindRec;
  Base: String;
begin
  Base := ExpandConstant('{app}');
  // 0.1.x - 0.2.1 installed straight into x64\ and x86\
  DelTree(Base + '\x64', True, True, True);
  DelTree(Base + '\x86', True, True, True);
  if FindFirst(Base + '\*', Found) then
  begin
    try
      repeat
        if (Found.Attributes and FILE_ATTRIBUTE_DIRECTORY <> 0) and (Found.Name <> '.') and (Found.Name <> '..') and
           (Found.Name <> '{#AppVersion}') and (Pos('.', Found.Name) > 0) then
          DelTree(Base + '\' + Found.Name, True, True, True);
      until not FindNext(Found);
    finally
      FindClose(Found);
    end;
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  ResultCode: Integer;
  X64Registered, X86Registered, UserProfileAdded: Boolean;
  Base: String;
begin
  if CurStep = ssPostInstall then
  begin
    RemoveOldVersions();
    Base := ExpandConstant('{app}\{#AppVersion}');
    X64Registered := Exec(ExpandConstant('{sys}\regsvr32.exe'), '/s "' + Base + '\x64\AutoType.dll"', '',
      SW_HIDE, ewWaitUntilTerminated, ResultCode) and (ResultCode = 0);
    X86Registered := Exec(ExpandConstant('{syswow64}\regsvr32.exe'), '/s "' + Base + '\x86\AutoType.dll"', '',
      SW_HIDE, ewWaitUntilTerminated, ResultCode) and (ResultCode = 0);
    UserProfileAdded := Exec(ExpandConstant('{sys}\WindowsPowerShell\v1.0\powershell.exe'),
      '-NoProfile -NonInteractive -ExecutionPolicy Bypass -File "' + ExpandConstant('{app}\register-user-input.ps1') + '"',
      '', SW_HIDE, ewWaitUntilTerminated, ResultCode) and (ResultCode = 0);
    if not (X64Registered and X86Registered and UserProfileAdded) then
      MsgBox('Auto-Type 등록을 완전히 마치지 못했습니다. 설치 후 Win+Space 목록에 Auto-Type가 없으면 다시 설치해 주세요.',
        mbError, MB_OK);
  end;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  Base: String;
  ResultCode: Integer;
begin
  if CurUninstallStep = usUninstall then
  begin
    Base := ExpandConstant('{app}');
    Exec(ExpandConstant('{sys}\WindowsPowerShell\v1.0\powershell.exe'),
      '-NoProfile -NonInteractive -ExecutionPolicy Bypass -File "' + Base + '\register-user-input.ps1" -Remove',
      '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
  end;
  if CurUninstallStep = usPostUninstall then
  begin
    Base := ExpandConstant('{app}');
    DelTree(Base, True, True, True);
    // Files still held by open apps: delete them at the next sign-in instead of asking for a reboot.
    if DirExists(Base) then
      RegWriteStringValue(HKLM, 'Software\Microsoft\Windows\CurrentVersion\RunOnce', 'AutoTypeCleanup',
        ExpandConstant('{cmd}') + ' /c rmdir /s /q "' + Base + '"');
  end;
end;
