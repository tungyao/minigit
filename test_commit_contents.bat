@echo off
echo Investigating commit contents...

:: 创建测试目录
if exist test_commit_contents rmdir /s /q test_commit_contents
mkdir test_commit_contents
cd test_commit_contents

:: 初始化仓库
..\build\bin\Release\minigit.exe init

echo.
echo === Step 1: Create files and commit ===
echo Content A > fileA.txt
echo Content B > fileB.txt
echo Content C > fileC.txt
..\build\bin\Release\minigit.exe add .
..\build\bin\Release\minigit.exe commit -m "Commit 1: added A, B, C"

echo.
echo === Step 2: Modify one file and commit (NOTE: only adding the modified file) ===
echo Modified B >> fileB.txt
..\build\bin\Release\minigit.exe add fileB.txt
..\build\bin\Release\minigit.exe commit -m "Commit 2: modified B only"

echo.
echo === Step 3: Check what files are in each commit ===
echo.
echo Log showing all commits:
..\build\bin\Release\minigit.exe log

echo.
echo === Step 4: Try alternative - add all files in second commit ===
echo Modified B again >> fileB.txt
..\build\bin\Release\minigit.exe add .
..\build\bin\Release\minigit.exe commit -m "Commit 3: modified B and included all files"

echo.
echo === Step 5: Delete files and reset to commit 3 ===
del fileA.txt
del fileC.txt
echo Status after deletion:
..\build\bin\Release\minigit.exe status

echo.
echo Reset to commit 3:
..\build\bin\Release\minigit.exe reset --hard

echo.
echo Files after reset:
dir /b *.txt

:: 清理
cd ..
rmdir /s /q test_commit_contents

echo.
echo Test completed.