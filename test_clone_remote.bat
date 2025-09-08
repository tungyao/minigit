@echo off
setlocal

echo Testing clone with remote configuration...
echo.

REM Clean up
if exist test_server_repo rmdir /s /q test_server_repo
if exist test_clone_repo rmdir /s /q test_clone_repo

echo Step 1: Create a test repository on server side
mkdir test_server_repo
cd test_server_repo
..\build\bin\Release\minigit.exe init
echo This is a test file > test.txt
..\build\bin\Release\minigit.exe add test.txt
..\build\bin\Release\minigit.exe commit -m "Initial commit"
cd ..

echo.
echo Step 2: Start server in background
start /b build\bin\Release\minigit.exe server --port 8080 --root . --password testpass

REM Wait for server to start
timeout /t 3 /nobreak >nul

echo.
echo Step 3: Clone repository using standalone clone command
build\bin\Release\minigit.exe clone localhost:8080/test_server_repo --password testpass

echo.
echo Step 4: Check if remote configuration was set
if exist test_server_repo\.minigit\config (
    echo Contents of cloned repository's config file:
    type test_server_repo\.minigit\config
) else (
    echo ERROR: Config file not found in cloned repository!
)

echo.
echo Step 5: Verify we can use the cloned repository
cd test_server_repo
echo Checking status of cloned repository:
build\bin\Release\minigit.exe status

cd ..

echo.
echo Test completed! Check the config file output above.
echo Expected: remote=server://localhost:8080/test_server_repo

REM Clean up
timeout /t 2 /nobreak >nul
taskkill /f /im minigit.exe >nul 2>&1

pause