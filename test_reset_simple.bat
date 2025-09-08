@echo off
REM 简化的reset功能测试

echo 测试MiniGit Reset功能...
echo.

REM 清理和创建测试目录
if exist test_reset rmdir /s /q test_reset
mkdir test_reset
cd test_reset

echo ==================== 初始化仓库 ====================
..\build\bin\Release\minigit.exe init

echo ==================== 创建文件并提交 ====================
echo Hello World > test.txt
..\build\bin\Release\minigit.exe add test.txt
..\build\bin\Release\minigit.exe commit -m "初始提交"

echo ==================== 查看状态 ====================
..\build\bin\Release\minigit.exe status
echo.

echo ==================== 测试reset到HEAD (默认mixed模式) ====================
..\build\bin\Release\minigit.exe reset
if %errorlevel% neq 0 (
    echo FAILED: reset命令失败
    goto :cleanup
)
echo PASS: reset命令成功

echo ==================== 再次查看状态 ====================
..\build\bin\Release\minigit.exe status
echo.

echo ==================== 测试reset --soft ====================
..\build\bin\Release\minigit.exe reset --soft
if %errorlevel% neq 0 (
    echo FAILED: reset --soft命令失败
    goto :cleanup
)
echo PASS: reset --soft命令成功

echo ==================== 测试reset --hard ====================
..\build\bin\Release\minigit.exe reset --hard
if %errorlevel% neq 0 (
    echo FAILED: reset --hard命令失败
    goto :cleanup
)
echo PASS: reset --hard命令成功

echo.
echo 所有reset功能测试通过！

:cleanup
cd ..
if exist test_reset rmdir /s /q test_reset
echo.
echo 测试完成，清理测试目录
pause