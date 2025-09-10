@echo off
echo Testing historical file deletion detection...

:: 创建测试目录
if exist test_historical_deletion rmdir /s /q test_historical_deletion
mkdir test_historical_deletion
cd test_historical_deletion

:: 初始化仓库
..\build\bin\Release\minigit.exe init

echo.
echo === Step 1: Create and commit some files ===
echo Content of file1 > file1.txt
echo Content of file2 > file2.txt
echo Content of file3 > file3.txt
..\build\bin\Release\minigit.exe add .
..\build\bin\Release\minigit.exe commit -m "Initial commit with 3 files"

echo.
echo === Step 2: Remove file1 from tracking and commit ===
del file1.txt
..\build\bin\Release\minigit.exe add .
..\build\bin\Release\minigit.exe commit -m "Remove file1"

echo.
echo === Step 3: Make another commit ===
echo More content >> file2.txt
..\build\bin\Release\minigit.exe add file2.txt
..\build\bin\Release\minigit.exe commit -m "Update file2"

echo.
echo === Step 4: Now delete file1 from working directory (simulating your scenario) ===
echo This simulates the case where file1 existed in historical commits but not in current HEAD
echo Current status should show file1 as missing from historical tracking:
..\build\bin\Release\minigit.exe status
..\build\bin\Release\minigit.exe log

echo.
echo === Step 5: Also test deleting file2 that exists in current HEAD ===
del file2.txt
echo Status after deleting file2 (should show D status):
..\build\bin\Release\minigit.exe status

:: 清理
cd ..
rmdir /s /q test_historical_deletion

echo.
echo Test completed.