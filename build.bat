@echo off
REM Windows批处理构建脚本

echo 检测到Windows平台
echo 开始构建MiniGit...

REM 创建构建目录
if not exist build mkdir build
cd build

REM 尝试检测Visual Studio版本
where devenv >nul 2>&1
if %errorlevel% equ 0 (
    echo 检测到Visual Studio
    cmake .. -G "Visual Studio 17 2022" -A x64
    if %errorlevel% neq 0 (
        echo 尝试Visual Studio 16 2019...
        cmake .. -G "Visual Studio 16 2019" -A x64
    )
) else (
    echo 使用默认生成器...
    cmake .. -DCMAKE_BUILD_TYPE=Release
)

REM 编译
cmake --build . --config Release

if %errorlevel% equ 0 (
    echo.
    echo 构建成功！
    echo 可执行文件位于: build\bin\Release\minigit.exe
) else (
    echo.
    echo 构建失败，请检查错误信息
    exit /b 1
)

pause