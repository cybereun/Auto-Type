@echo off
chcp 65001 >nul
del "%LOCALAPPDATA%\AutoType\debug" 2>nul
echo 진단 기록을 껐습니다.
start "" notepad "%LOCALAPPDATA%\AutoType\log.txt"
pause
