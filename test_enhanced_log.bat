@echo off
echo Testing enhanced log functionality with deleted files...

:: 创建测试目录
if exist test_enhanced_log rmdir /s /q test_enhanced_log
mkdir test_enhanced_log
cd test_enhanced_log

:: 初始化仓库
..\build\bin\Release\minigit.exe init

echo.
echo === Step 1: Create initial commit ===
echo Content A > fileA.txt
echo Content B > fileB.txt
echo Content C > fileC.txt
..\build\bin\Release\minigit.exe add .
..\build\bin\Release\minigit.exe commit -m "Initial commit: added A, B, C"

echo.
echo === Step 2: Modify one file and commit ===
echo Modified content >> fileB.txt
..\build\bin\Release\minigit.exe add fileB.txt
..\build\bin\Release\minigit.exe commit -m "Modified fileB"

echo.
echo === Step 3: Delete files and commit ===
del fileA.txt
del fileC.txt
..\build\bin\Release\minigit.exe add .
..\build\bin\Release\minigit.exe commit -m "Deleted fileA and fileC"

echo.
echo === Step 4: Add new file and commit ===
echo New content > fileD.txt
..\build\bin\Release\minigit.exe add fileD.txt
..\build\bin\Release\minigit.exe commit -m "Added fileD"

echo.
echo === Step 5: Show enhanced log with deleted files ===
echo Log should now show file additions, modifications, and deletions:
..\build\bin\Release\minigit.exe log

echo.
echo === Step 6: Test oneline format ===
echo Oneline format:
..\build\bin\Release\minigit.exe log --oneline

:: 清理
cd ..
rmdir /s /q test_enhanced_log

echo.
echo Enhanced log test completed!