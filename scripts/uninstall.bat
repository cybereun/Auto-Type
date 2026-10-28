@echo off
chcp 65001 >nul
rem Auto-Type 제거: 관리자 권한으로 실행하세요. 기존 Microsoft 입력기는 그대로 남습니다.
setlocal
set DIR=%ProgramFiles%\AutoType
net session >nul 2>&1 || (echo 관리자 권한으로 실행해 주세요. & pause & exit /b 1)
if exist "%DIR%\x64\AutoType.dll" regsvr32 /u /s "%DIR%\x64\AutoType.dll"
if exist "%DIR%\x86\AutoType.dll" "%SystemRoot%\SysWOW64\regsvr32.exe" /u /s "%DIR%\x86\AutoType.dll"
rem 열린 앱이 DLL을 쥐고 있어 지금 못 지운 파일은 다음 로그인 때 지웁니다 (재부팅 불필요).
rmdir /s /q "%DIR%" 2>nul
if exist "%DIR%" reg add "HKLM\Software\Microsoft\Windows\CurrentVersion\RunOnce" /v AutoTypeCleanup /t REG_SZ /d "cmd /c rmdir /s /q \"%DIR%\"" /f >nul
echo 제거 완료. 이제 Microsoft 입력기만 남았습니다.
pause
