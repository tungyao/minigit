@echo off
echo Testing historical file deletion with add, commit, and push operations...

:: 创建测试目录
if exist test_comprehensive_historical rmdir /s /q test_comprehensive_historical
mkdir test_comprehensive_historical
cd test_comprehensive_historical

:: 初始化仓库
..\build\bin\Release\minigit.exe init

echo.
echo === Step 1: Create initial commit with multiple files ===
echo Content A > fileA.txt
echo Content B > fileB.txt
echo Content C > fileC.txt
..\build\bin\Release\minigit.exe add .
..\build\bin\Release\minigit.exe commit -m "Initial commit with A, B, C"

echo.
echo === Step 2: Remove fileA from repository (simulate deletion) ===
del fileA.txt
..\build\bin\Release\minigit.exe add fileA.txt
..\build\bin\Release\minigit.exe commit -m "Remove fileA"

echo.
echo === Step 3: Add more files and commits ===
echo Content D > fileD.txt
echo Updated B content > fileB.txt
..\build\bin\Release\minigit.exe add .
..\build\bin\Release\minigit.exe commit -m "Add fileD and update fileB"

echo.
echo === Step 4: Now physically delete fileA from working directory ===
echo (fileA was removed in step 2, but now we simulate accidental deletion from workspace)
echo Current status - should show HD for fileA:
..\build\bin\Release\minigit.exe status

echo.
echo === Step 5: Test add command with historical deletion ===
echo Testing add . command with historical deletion detection:
..\build\bin\Release\minigit.exe add .

echo.
echo === Step 6: Delete current HEAD file and test ===
del fileB.txt
echo Status after deleting current file (should show both HD and D):
..\build\bin\Release\minigit.exe status

echo.
echo === Step 7: Test add command again ===
echo Testing add with both historical and current deletions:
..\build\bin\Release\minigit.exe add .

echo.
echo === Step 8: Test diff command ===
echo Testing diff command:
..\build\bin\Release\minigit.exe diff

echo.
echo === Step 9: Show commit log to verify history ===
..\build\bin\Release\minigit.exe log --oneline

:: 清理
cd ..
rmdir /s /q test_comprehensive_historical

echo.
echo Comprehensive test completed!