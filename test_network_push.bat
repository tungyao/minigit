@echo off
echo Testing network push functionality...
echo.

REM Clean up previous tests
if exist test_server_repo rmdir /s /q test_server_repo
if exist test_network_push rmdir /s /q test_network_push

echo Step 1: Create a server repository
mkdir test_server_repo
cd test_server_repo
..\build\bin\Release\minigit.exe init
echo This is server test > server.txt
..\build\bin\Release\minigit.exe add server.txt
..\build\bin\Release\minigit.exe commit -m "Server initial commit"
cd ..

echo.
echo Step 2: Start server
start /b build\bin\Release\minigit.exe server --port 8080 --root . --password testpass123

REM Wait for server to start
timeout /t 3 /nobreak >nul

echo Step 3: Create client repository with network remote
mkdir test_network_push
cd test_network_push
..\build\bin\Release\minigit.exe init
echo This is client test > client.txt
..\build\bin\Release\minigit.exe add client.txt
..\build\bin\Release\minigit.exe commit -m "Client initial commit"

REM Set network remote
echo remote=server://localhost:8080/test_server_repo > .minigit\config

echo.
echo Step 4: Test network push with password
echo Attempting network push...
..\build\bin\Release\minigit.exe push --password testpass123

echo.
echo Step 5: Test network push without password (should fail)
echo Attempting network push without password...
..\build\bin\Release\minigit.exe push

echo.
echo Step 6: Test network pull with password
echo Attempting network pull...
..\build\bin\Release\minigit.exe pull --password testpass123

cd ..

echo.
echo Cleaning up...
timeout /t 2 /nobreak >nul
taskkill /f /im minigit.exe >nul 2>&1

echo.
echo Test completed!
echo Expected results:
echo - Network push with password should work
echo - Network push without password should fail with error message
echo - Network pull with password should work

pause