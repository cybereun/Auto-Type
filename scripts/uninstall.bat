@echo off
chcp 65001 >nul
rem Auto-Type 제거: 관리자 권한으로 실행하세요. 기존 Microsoft 입력기는 그대로 남습니다.
setlocal
set DIR=%ProgramFiles%\AutoType
net session >nul 2>&1 || (echo 관리자 권한으로 실행해 주세요. & pause & exit /b 1)
if exist "%DIR%\x64\AutoType.dll" regsvr32 /u /s "%DIR%\x64\AutoType.dll"
if exist "%DIR%\x86\AutoType.dll" "%SystemRoot%\SysWOW64\regsvr32.exe" /u /s "%DIR%\x86\AutoType.dll"
rem 실행 중인 앱이 DLL을 잡고 있으면 파일 삭제는 재부팅 후 다시 실행하면 됩니다.
rmdir /s /q "%DIR%" 2>nul
echo 제거 완료. 이제 Microsoft 입력기만 남았습니다.
pause
