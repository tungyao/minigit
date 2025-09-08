@echo off
REM 测试log命令功能

echo 测试MiniGit Log功能...
echo.

REM 清理和创建测试目录
if exist test_log rmdir /s /q test_log
mkdir test_log
cd test_log

echo ==================== 初始化仓库 ====================
..\build\bin\Release\minigit.exe init
if %errorlevel% neq 0 goto :error

echo ==================== 创建第一次提交 ====================
echo 第一个文件内容 > file1.txt
echo 另一个文件内容 > file2.txt
..\build\bin\Release\minigit.exe add file1.txt file2.txt
..\build\bin\Release\minigit.exe commit -m "第一次提交：添加两个文件"
if %errorlevel% neq 0 goto :error

echo ==================== 创建第二次提交 ====================
echo 修改后的内容 > file1.txt
echo 新文件 > file3.txt
..\build\bin\Release\minigit.exe add file1.txt file3.txt
..\build\bin\Release\minigit.exe commit -m "第二次提交：修改file1并添加file3"
if %errorlevel% neq 0 goto :error

echo ==================== 创建第三次提交 ====================
echo 最新内容 > file2.txt
..\build\bin\Release\minigit.exe add file2.txt
..\build\bin\Release\minigit.exe commit -m "第三次提交：更新file2"
if %errorlevel% neq 0 goto :error

echo ==================== 测试log命令（详细格式） ====================
echo.
echo 显示所有提交历史（详细格式）：
..\build\bin\Release\minigit.exe log
if %errorlevel% neq 0 goto :error

echo.
echo ==================== 测试log命令（单行格式） ====================
echo.
echo 显示所有提交历史（单行格式）：
..\build\bin\Release\minigit.exe log --oneline
if %errorlevel% neq 0 goto :error

echo.
echo ==================== 测试log命令（限制数量） ====================
echo.
echo 显示最近2次提交：
..\build\bin\Release\minigit.exe log -n 2
if %errorlevel% neq 0 goto :error

echo.
echo ==================== 测试log命令（限制数量+单行） ====================
echo.
echo 显示最近1次提交（单行格式）：
..\build\bin\Release\minigit.exe log --oneline -n 1
if %errorlevel% neq 0 goto :error

echo.
echo 所有log命令测试通过！
goto :cleanup

:error
echo 测试失败，错误代码: %errorlevel%
goto :cleanup

:cleanup
cd ..
if exist test_log rmdir /s /q test_log
echo.
echo 测试完成，清理测试目录
pause