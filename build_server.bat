@echo off
echo Building MiniGit with Server Support...
echo.

REM 清理旧的build目录
if exist build (
    echo Cleaning old build directory...
    rmdir /s /q build >nul 2>&1
    timeout /t 2 >nul
)

REM 创建build目录
mkdir build
cd build

echo Configuring with CMake...
cmake .. -G "Visual Studio 17 2022" -A x64

if %ERRORLEVEL% NEQ 0 (
    echo CMake configuration failed!
    pause
    exit /b 1
)

echo Building project...
cmake --build . --config Release

if %ERRORLEVEL% NEQ 0 (
    echo Build failed! Checking for errors...
    echo.
    echo Common issues and solutions:
    echo 1. Make sure Visual Studio 2022 is installed
    echo 2. Check if Windows SDK is properly installed
    echo 3. Ensure CMake is in PATH
    echo 4. Close any running instances of the application
    pause
    exit /b 1
)

echo.
echo Build successful! 
echo Executable location: build\bin\Release\minigit.exe
echo.

if exist bin\Release\minigit.exe (
    echo Testing basic functionality...
    bin\Release\minigit.exe
    echo.
) else (
    echo Warning: Executable not found in expected location
)

echo.
echo Server usage examples:
echo 1. Start server: minigit server --port 8080 --root "C:\repos" --password "test123"
echo 2. Connect client: minigit connect --host localhost --port 8080 --password "test123"
echo.

pause