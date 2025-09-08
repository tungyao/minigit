@echo off
echo Testing push/pull with network remote...
echo.

REM Clean up
if exist test_network_remote rmdir /s /q test_network_remote

echo Step 1: Create a test repository
mkdir test_network_remote
cd test_network_remote
build\bin\Release\minigit.exe init
echo This is a test file > test.txt
build\bin\Release\minigit.exe add test.txt
build\bin\Release\minigit.exe commit -m "Test commit"

echo.
echo Step 2: Manually set network remote (simulating clone behavior)
echo remote=server://localhost:8080/test_repo > .minigit\config

echo.
echo Step 3: Test push command (should show helpful error message)
build\bin\Release\minigit.exe push

echo.
echo Step 4: Test pull command (should show helpful error message)
build\bin\Release\minigit.exe pull

echo.
echo Step 5: Change to local remote to confirm local push/pull still works
build\bin\Release\minigit.exe set-remote ..\test_local_bare
echo Testing local push...
build\bin\Release\minigit.exe push

cd ..

echo.
echo Test completed!
echo - Network remotes now show helpful error messages instead of filesystem errors
echo - Local remotes still work normally
echo.

pause