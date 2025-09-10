@echo off
echo Testing basic reset --hard functionality...

:: 创建测试目录
if exist test_reset_basic rmdir /s /q test_reset_basic
mkdir test_reset_basic
cd test_reset_basic

:: 初始化仓库
..\build\bin\Release\minigit.exe init

echo.
echo === Step 1: Create first commit ===
echo Content A > fileA.txt
echo Content B > fileB.txt
..\build\bin\Release\minigit.exe add .
..\build\bin\Release\minigit.exe commit -m "First commit"

echo.
echo === Step 2: Create second commit ===
echo Updated A > fileA.txt
echo Content C > fileC.txt
..\build\bin\Release\minigit.exe add .
..\build\bin\Release\minigit.exe commit -m "Second commit"

echo.
echo === Step 3: Show commits ===
..\build\bin\Release\minigit.exe log --oneline

echo.
echo === Step 4: Delete files manually ===
del fileA.txt
del fileB.txt
echo Status after deletion:
..\build\bin\Release\minigit.exe status

echo.
echo === Step 5: Reset --hard to HEAD ===
..\build\bin\Release\minigit.exe reset --hard
echo Status after reset:
..\build\bin\Release\minigit.exe status

echo.
echo === Step 6: Verify files are restored ===
echo Files present:
dir /b *.txt

:: 清理
cd ..
rmdir /s /q test_reset_basic

echo.
echo Basic reset test completed!