@echo off
echo Testing MiniGit Clone Functionality...
echo.

REM 检查可执行文件是否存在
if not exist "build\bin\Release\minigit.exe" (
    echo Error: minigit.exe not found! Please build first.
    pause
    exit /b 1
)

echo Test 1: Display help for clone command
build\bin\Release\minigit.exe clone
echo.

echo Test 2: Display help for connect command  
build\bin\Release\minigit.exe connect
echo.

echo Test 3: Display help for server command
build\bin\Release\minigit.exe server
echo.

echo.
echo Clone functionality test scenarios:
echo.
echo Scenario 1: Start server manually
echo   minigit server --port 8080 --root "D:\test_repos" --password "test123"
echo.
echo Scenario 2: Connect and create repo
echo   minigit connect --host localhost --port 8080 --password "test123"
echo   ^> create testRepo
echo   ^> quit
echo.
echo Scenario 3: Clone using independent command
echo   minigit clone localhost:8080/testRepo --password "test123"
echo.
echo Scenario 4: Clone using interactive mode
echo   minigit connect --host localhost --port 8080 --password "test123"
echo   ^> clone testRepo
echo   ^> quit
echo.

echo All clone functionality has been implemented and ready for testing!
echo Please run the scenarios manually to test the complete workflow.
echo.

pause