@echo off
echo Testing authentication only...
echo.

REM Clean up previous tests
if exist debug_test rmdir /s /q debug_test

echo Step 1: Create minimal test repository
mkdir debug_test
cd debug_test
..\build\bin\Release\minigit.exe init
echo test > test.txt
..\build\bin\Release\minigit.exe add test.txt
..\build\bin\Release\minigit.exe commit -m "Test commit"

echo remote=server://localhost:8080/test_repo > .minigit\config

echo.
echo Step 2: Start server (ensure clean state)
start /b ..\build\bin\Release\minigit.exe server --port 8080 --root .. --password testpass123

timeout /t 2 /nobreak >nul

echo Step 3: Test authentication with detailed output
echo Attempting network push to test authentication...
..\build\bin\Release\minigit.exe push --password testpass123

cd ..

echo.
echo Cleaning up...
timeout /t 1 /nobreak >nul
taskkill /f /im minigit.exe >nul 2>&1

pause