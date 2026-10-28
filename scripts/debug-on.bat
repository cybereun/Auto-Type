@echo off
chcp 65001 >nul
rem Auto-Type 진단 기록 켜기. 이후 새로 연 앱부터 기록됩니다.
mkdir "%LOCALAPPDATA%\AutoType" 2>nul
type nul > "%LOCALAPPDATA%\AutoType\debug"
del "%LOCALAPPDATA%\AutoType\log.txt" 2>nul
echo 진단 기록을 켰습니다. 카카오톡을 완전히 종료했다가 다시 열고 dkssud 를 쳐 보세요.
echo 기록 파일: %LOCALAPPDATA%\AutoType\log.txt
pause
