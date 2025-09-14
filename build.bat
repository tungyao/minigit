@echo off
REM Windows批处理构建脚本

echo windows build script
echo start cmake...

REM 创建构建目录
if not exist build mkdir build
cd build

where devenv >nul 2>&1
if %errorlevel% equ 0 (
    echo use Visual Studio generator
    cmake .. -G "Visual Studio 17 2022" -A x64
    if %errorlevel% neq 0 (
        echo 尝试Visual Studio 16 2019...
        cmake .. -G "Visual Studio 16 2019" -A x64
    )
) else (
    echo use default generator
    cmake .. -DCMAKE_BUILD_TYPE=Release
)

cmake --build . --config Release

if %errorlevel% equ 0 (
    echo.
    echo build successed!
    echo exec file: build\bin\Release\minigit.exe
) else (
    echo.
    echo build failed!
    exit /b 1
)