@echo off
echo Testing smart push functionality...
echo.

REM Clean up previous tests
if exist server_repos rmdir /s /q server_repos
if exist test_smart_push rmdir /s /q test_smart_push

echo Step 1: Create server repository structure
REM Create server root directory
mkdir server_repos

REM Create a test repository inside server_repos
mkdir server_repos\test_repo
cd server_repos\test_repo
..\..\build\bin\Release\minigit.exe init
echo This is initial server file > server.txt
..\..\build\bin\Release\minigit.exe add server.txt
..\..\build\bin\Release\minigit.exe commit -m "Initial server commit"
cd ..\..

echo.
echo Step 2: Start server with correct repository root
start /b build\bin\Release\minigit.exe server --port 8080 --root server_repos --password testpass123

REM Wait for server to start
timeout /t 3 /nobreak >nul

echo Step 3: Create client repository and make commits
mkdir test_smart_push
cd test_smart_push
..\build\bin\Release\minigit.exe init
echo This is client file 1 > file1.txt
..\build\bin\Release\minigit.exe add file1.txt
..\build\bin\Release\minigit.exe commit -m "Client commit 1"

echo This is client file 2 > file2.txt
..\build\bin\Release\minigit.exe add file2.txt
..\build\bin\Release\minigit.exe commit -m "Client commit 2"

echo This is client file 3 > file3.txt
..\build\bin\Release\minigit.exe add file3.txt
..\build\bin\Release\minigit.exe commit -m "Client commit 3"

REM Set network remote pointing to the test repository
echo remote=server://localhost:8080/test_repo > .minigit\config

echo.
echo Step 4: Test smart push with password
echo Attempting smart push...
..\build\bin\Release\minigit.exe push --password testpass123

echo.
echo Step 5: Check client status
echo Client status:
..\build\bin\Release\minigit.exe status

echo.
echo Step 6: Check server repository
cd ..\server_repos\test_repo
echo Server status:
..\..\build\bin\Release\minigit.exe status

cd ..\..

echo.
echo Step 7: Test incremental push - add more commits
cd test_smart_push
echo This is client file 4 > file4.txt
..\build\bin\Release\minigit.exe add file4.txt
..\build\bin\Release\minigit.exe commit -m "Client commit 4"

echo.
echo Step 8: Test incremental push (should only push the new commit)
echo Attempting incremental push...
..\build\bin\Release\minigit.exe push --password testpass123

cd ..

echo.
echo Cleaning up...
timeout /t 2 /nobreak >nul
taskkill /f /im minigit.exe >nul 2>&1

echo.
echo Test completed!
echo Expected results:
echo - First push should upload all 3 commits from client
echo - Second push should only upload the 1 new commit
echo - Server repository should contain all client commits

pause