@echo off
echo Testing repository detection...
echo.

REM Clean up previous tests
if exist test_repo rmdir /s /q test_repo

echo Step 1: Create and verify test repository
mkdir test_repo
cd test_repo
echo Creating MiniGit repository...
..\build\bin\Release\minigit.exe init

echo.
echo Repository structure:
dir .minigit
echo.
echo Files in .minigit:
dir .minigit\*

echo.
echo Repository detection test:
echo Path: %CD%
echo Testing if .minigit exists:
if exist .minigit (
    echo [OK] .minigit directory exists
) else (
    echo [ERROR] .minigit directory not found
)

cd ..

echo.
echo Step 2: Test server repository detection
echo Starting server to test repository detection...
timeout /t 1 /nobreak >nul
start /b build\bin\Release\minigit.exe server --port 8080 --root . --password testpass123

timeout /t 2 /nobreak >nul

echo Step 3: Test client connection to list repositories
build\bin\Release\minigit.exe connect --host localhost --port 8080 --password testpass123

echo.
echo Cleaning up...
timeout /t 1 /nobreak >nul
taskkill /f /im minigit.exe >nul 2>&1

pause