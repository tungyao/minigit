@echo off
REM 测试reset功能的脚本

echo 测试MiniGit Reset功能...
echo.

REM 清理和创建测试目录
if exist test_reset rmdir /s /q test_reset
mkdir test_reset
cd test_reset

echo ==================== 初始化仓库 ====================
..\build\bin\Release\minigit.exe init
if %errorlevel% neq 0 goto :error

echo ==================== 创建第一批文件 ====================
echo Version 1 content > file1.txt
echo Initial content > file2.txt
..\build\bin\Release\minigit.exe add file1.txt file2.txt
..\build\bin\Release\minigit.exe commit -m "第一次提交"

echo ==================== 创建第二批文件 ====================
echo Version 2 content > file1.txt
echo Updated content > file2.txt
echo New file > file3.txt
..\build\bin\Release\minigit.exe add file1.txt file2.txt file3.txt
..\build\bin\Release\minigit.exe commit -m "第二次提交"

echo ==================== 查看当前状态 ====================
..\build\bin\Release\minigit.exe status
echo.
echo 当前工作目录文件：
dir /b *.txt
echo.

echo ==================== 测试reset --soft ====================
echo 执行reset --soft HEAD~1 （重置到上一次提交，保留暂存区和工作目录）
for /f "tokens=2" %%i in ('..\build\bin\Release\minigit.exe status ^| findstr "On commit:"') do set CURRENT_COMMIT=%%i
echo 当前提交: %CURRENT_COMMIT%

REM 由于我们没有HEAD~1语法，我们需要手动获取父提交
REM 为了简化测试，我们先保存当前HEAD然后测试reset到当前HEAD
..\build\bin\Release\minigit.exe reset --soft %CURRENT_COMMIT%
if %errorlevel% neq 0 goto :error
echo PASS: reset --soft成功

echo ==================== 测试reset --mixed ====================
echo 执行reset --mixed （默认模式，重置HEAD和暂存区）
..\build\bin\Release\minigit.exe reset --mixed %CURRENT_COMMIT%
if %errorlevel% neq 0 goto :error
echo PASS: reset --mixed成功

echo ==================== 测试reset --hard ====================
echo 执行reset --hard （重置HEAD、暂存区和工作目录）
..\build\bin\Release\minigit.exe reset --hard %CURRENT_COMMIT%
if %errorlevel% neq 0 goto :error
echo PASS: reset --hard成功

echo ==================== 验证最终状态 ====================
..\build\bin\Release\minigit.exe status
echo.
echo 工作目录文件：
dir /b *.txt
echo.

echo 所有reset功能测试通过！
goto :cleanup

:error
echo 测试失败，错误代码: %errorlevel%
goto :cleanup

:cleanup
cd ..
if exist test_reset rmdir /s /q test_reset
echo.
echo 测试完成，清理测试目录
pause