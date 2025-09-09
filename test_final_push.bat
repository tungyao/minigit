@echo off
echo Final test of smart push functionality...
echo.

REM Clean up
if exist final_client rmdir /s /q final_client

echo Step 1: Start server
start /b build\bin\Release\minigit.exe server --port 8080 --root . --password testpass123
timeout /t 2 /nobreak >nul

echo Step 2: Create client repository with commits
mkdir final_client
cd final_client
..\build\bin\Release\minigit.exe init

echo Client file 1 > file1.txt
..\build\bin\Release\minigit.exe add file1.txt
..\build\bin\Release\minigit.exe commit -m "Client commit 1"

echo Client file 2 > file2.txt
..\build\bin\Release\minigit.exe add file2.txt
..\build\bin\Release\minigit.exe commit -m "Client commit 2"

REM Set remote to existing test_repo
echo remote=server://localhost:8080/test_repo > .minigit\config

echo.
echo Step 3: Display client status before push
echo Client commits before push:
..\build\bin\Release\minigit.exe log --oneline

echo.
echo Step 4: Attempt smart push
echo Attempting smart push...
..\build\bin\Release\minigit.exe push --password testpass123

echo.
echo Step 5: Display results
echo Client status after push:
..\build\bin\Release\minigit.exe status

echo.
echo Server repository status:
cd ..\test_repo
..\build\bin\Release\minigit.exe status

cd ..

echo.
echo Cleaning up...
timeout /t 1 /nobreak >nul
taskkill /f /im minigit.exe >nul 2>&1

pause