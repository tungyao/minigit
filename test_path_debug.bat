@echo off
echo Debug path handling...

:: 创建测试目录
if exist test_path_debug rmdir /s /q test_path_debug
mkdir test_path_debug
cd test_path_debug

:: 初始化仓库
..\build\bin\Release\minigit.exe init

echo.
echo === Current directory: ===
cd

echo.
echo === Repo root path should be: %cd% ===

echo.
echo === Create and commit file ===
echo Content > test.txt
..\build\bin\Release\minigit.exe add test.txt
..\build\bin\Release\minigit.exe commit -m "Initial"

echo.
echo === Delete file and try different add commands ===
del test.txt
echo After deletion:
..\build\bin\Release\minigit.exe status

echo.
echo Try: add test.txt
..\build\bin\Release\minigit.exe add test.txt

echo.
echo Try: add .\test.txt
..\build\bin\Release\minigit.exe add .\test.txt

echo.
echo Try: add . (from subdirectory)
mkdir subdir
cd subdir
echo Try from subdirectory...
..\..\build\bin\Release\minigit.exe add ..

cd ..
echo Back to main directory...

:: 清理
cd ..
rmdir /s /q test_path_debug

echo Test completed.