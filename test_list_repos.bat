@echo off
echo Testing list repositories command...
echo.

REM Start server
start /b build\bin\Release\minigit.exe server --port 8080 --root . --password testpass123

timeout /t 2 /nobreak >nul

REM Test connection and list repositories using echo
echo ls > temp_commands.txt
echo quit >> temp_commands.txt

echo Testing repository listing...
build\bin\Release\minigit.exe connect --host localhost --port 8080 --password testpass123 < temp_commands.txt

del temp_commands.txt

echo.
echo Cleaning up...
timeout /t 1 /nobreak >nul
taskkill /f /im minigit.exe >nul 2>&1

pause