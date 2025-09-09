@echo off
echo Complete smart push test...
echo.

REM Clean up
if exist clean_server_repo rmdir /s /q clean_server_repo
if exist clean_client rmdir /s /q clean_client

echo Step 1: Create server repository with initial commit
mkdir clean_server_repo
cd clean_server_repo
..\build\bin\Release\minigit.exe init
echo Server initial file > server.txt
..\build\bin\Release\minigit.exe add server.txt
..\build\bin\Release\minigit.exe commit -m "Server initial commit"

echo Server repository status:
..\build\bin\Release\minigit.exe status

cd ..

echo.
echo Step 2: Start server
start /b build\bin\Release\minigit.exe server --port 8080 --root . --password testpass123
timeout /t 2 /nobreak >nul

echo Step 3: Create client repository
mkdir clean_client
cd clean_client
..\build\bin\Release\minigit.exe init

echo Client file 1 > file1.txt
..\build\bin\Release\minigit.exe add file1.txt
..\build\bin\Release\minigit.exe commit -m "Client commit 1"

echo Client file 2 > file2.txt
..\build\bin\Release\minigit.exe add file2.txt
..\build\bin\Release\minigit.exe commit -m "Client commit 2"

echo remote=server://localhost:8080/clean_server_repo > .minigit\config

echo.
echo Step 4: Display status before push
echo Client commits:
..\build\bin\Release\minigit.exe log --oneline

echo.
echo Step 5: Attempt smart push
echo Testing smart push...
..\build\bin\Release\minigit.exe push --password testpass123

echo.
echo Step 6: Check results
echo Client final status:
..\build\bin\Release\minigit.exe status

echo.
echo Server final status:
cd ..\clean_server_repo
..\build\bin\Release\minigit.exe status

cd ..

echo.
echo Cleaning up...
timeout /t 1 /nobreak >nul
taskkill /f /im minigit.exe >nul 2>&1

pause