@echo off
echo Testing add command debug...

:: 创建测试目录
if exist test_add_debug rmdir /s /q test_add_debug
mkdir test_add_debug
cd test_add_debug

:: 初始化仓库
..\build\bin\Release\minigit.exe init

echo.
echo === Step 1: Create and commit some files ===
echo Content A > fileA.txt
echo Content B > fileB.txt
..\build\bin\Release\minigit.exe add .
..\build\bin\Release\minigit.exe commit -m "Initial commit"

echo.
echo === Step 2: Delete one file and check status ===
del fileA.txt
echo Status after deletion:
..\build\bin\Release\minigit.exe status

echo.
echo === Step 3: Try to add specific deleted file ===
echo This should work...
..\build\bin\Release\minigit.exe add fileA.txt

echo.
echo === Step 4: Check status after specific add ===
..\build\bin\Release\minigit.exe status

:: 清理
cd ..
rmdir /s /q test_add_debug

echo.
echo Test completed.