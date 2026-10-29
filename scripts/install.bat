@echo off
chcp 65001 >nul
rem Auto-Type 설치: 관리자 권한으로 실행하세요 (우클릭 - 관리자 권한으로 실행).
setlocal
set DIR=%ProgramFiles%\AutoType
net session >nul 2>&1 || (echo 관리자 권한으로 실행해 주세요. & pause & exit /b 1)

mkdir "%DIR%\x64" 2>nul
mkdir "%DIR%\x86" 2>nul
copy /y "%~dp0x64\AutoType.dll" "%DIR%\x64\" >nul || goto fail
regsvr32 /s "%DIR%\x64\AutoType.dll" || goto fail
rem 32비트 프로그램(일부 구형 앱)에서도 쓰려면 32비트 DLL도 등록합니다.
if exist "%~dp0x86\AutoType.dll" (
  copy /y "%~dp0x86\AutoType.dll" "%DIR%\x86\" >nul
  "%SystemRoot%\SysWOW64\regsvr32.exe" /s "%DIR%\x86\AutoType.dll"
)
powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%~dp0register-user-input.ps1" || goto fail
echo 설치 완료. 설정 - 시간 및 언어 - 언어 및 지역 - 한국어 - 언어 옵션 - 키보드 에서
echo "Auto-Type (한영 자동)"이 보이는지 확인하세요. Win+Space 로 전환합니다.
pause
exit /b 0
:fail
echo 설치 실패. 다른 앱이 DLL을 쓰고 있으면 로그아웃 후 다시 시도하세요.
pause
exit /b 1
