@echo off
REM 简单的功能测试脚本

echo 测试MiniGit基本功能...
echo.

REM 创建测试目录
if exist test_repo rmdir /s /q test_repo
mkdir test_repo
cd test_repo

echo 1. 测试 init 命令
..\build\bin\Release\minigit.exe init
if %errorlevel% neq 0 (
    echo FAILED: init命令失败
    goto :cleanup
)
echo PASS: init命令成功
echo.

echo 2. 创建测试文件
echo Hello MiniGit > test.txt
echo 这是一个测试文件 > test2.txt

echo 3. 测试 add 命令
..\build\bin\Release\minigit.exe add test.txt test2.txt
if %errorlevel% neq 0 (
    echo FAILED: add命令失败
    goto :cleanup
)
echo PASS: add命令成功
echo.

echo 4. 测试 status 命令
..\build\bin\Release\minigit.exe status
if %errorlevel% neq 0 (
    echo FAILED: status命令失败
    goto :cleanup
)
echo PASS: status命令成功
echo.

echo 5. 测试 commit 命令
..\build\bin\Release\minigit.exe commit -m "第一次提交"
if %errorlevel% neq 0 (
    echo FAILED: commit命令失败
    goto :cleanup
)
echo PASS: commit命令成功
echo.

echo 6. 再次查看状态
..\build\bin\Release\minigit.exe status
echo.

echo 所有基本功能测试通过！
goto :cleanup

:cleanup
cd ..
if exist test_repo rmdir /s /q test_repo
echo.
echo 测试完成，清理测试目录
pause