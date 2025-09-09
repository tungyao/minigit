@echo off
echo Testing fixed smart push functionality...
echo.

REM Clean up previous tests
if exist test_repo rmdir /s /q test_repo
if exist debug_test rmdir /s /q debug_test

echo Step 1: Create server repository
mkdir test_repo
cd test_repo
..\build\bin\Release\minigit.exe init
echo Server initial file > server.txt
..\build\bin\Release\minigit.exe add server.txt
..\build\bin\Release\minigit.exe commit -m "Initial server commit"
cd ..

echo.
echo Step 2: Start server (root is current directory)
start /b build\bin\Release\minigit.exe server --port 8080 --root . --password testpass123

timeout /t 2 /nobreak >nul

echo Step 3: Create client repository
mkdir debug_test
cd debug_test
..\build\bin\Release\minigit.exe init
echo Client file 1 > file1.txt
..\build\bin\Release\minigit.exe add file1.txt
..\build\bin\Release\minigit.exe commit -m "Client commit 1"

echo Client file 2 > file2.txt
..\build\bin\Release\minigit.exe add file2.txt
..\build\bin\Release\minigit.exe commit -m "Client commit 2"

echo remote=server://localhost:8080/test_repo > .minigit\config

echo.
echo Step 4: Test smart push
echo Attempting smart push...
..\build\bin\Release\minigit.exe push --password testpass123

echo.
echo Step 5: Check results
echo Client status:
..\build\bin\Release\minigit.exe status

echo.
echo Server status:
cd ..\test_repo
..\build\bin\Release\minigit.exe status

cd ..

echo.
echo Cleaning up...
timeout /t 1 /nobreak >nul
taskkill /f /im minigit.exe >nul 2>&1

pause