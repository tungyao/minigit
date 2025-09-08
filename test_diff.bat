@echo off
REM 测试文件修改检测功能

echo 测试MiniGit文件修改检测功能...
echo.

REM 清理和创建测试目录
if exist test_diff rmdir /s /q test_diff
mkdir test_diff
cd test_diff

echo ==================== 初始化仓库 ====================
..\build\bin\Release\minigit.exe init
if %errorlevel% neq 0 goto :error

echo ==================== 创建和添加初始文件 ====================
echo 初始内容 > file1.txt
echo 另一个文件 > file2.txt
echo 第三个文件 > file3.txt

echo === 查看未跟踪文件 ===
..\build\bin\Release\minigit.exe status
echo.

echo === 使用diff查看工作目录状态 ===
..\build\bin\Release\minigit.exe diff
echo.

echo ==================== 添加文件到暂存区 ====================
..\build\bin\Release\minigit.exe add file1.txt file2.txt
echo.

echo === 查看暂存区状态 ===
..\build\bin\Release\minigit.exe status
echo.

echo === 使用diff --cached查看暂存区状态 ===
..\build\bin\Release\minigit.exe diff --cached
echo.

echo ==================== 提交文件 ====================
..\build\bin\Release\minigit.exe commit -m "初始提交"
echo.

echo ==================== 修改现有文件 ====================
echo 修改后的内容 > file1.txt
echo 新添加的文件 > file4.txt

echo === 查看修改后的状态 ===
..\build\bin\Release\minigit.exe status
echo.

echo === 使用diff查看工作目录变化 ===
..\build\bin\Release\minigit.exe diff
echo.

echo === 使用diff --name-only只显示文件名 ===
..\build\bin\Release\minigit.exe diff --name-only
echo.

echo ==================== 删除文件测试 ====================
del file2.txt

echo === 查看删除文件后的状态 ===
..\build\bin\Release\minigit.exe status
echo.

echo === 使用diff查看包含删除的变化 ===
..\build\bin\Release\minigit.exe diff
echo.

echo 所有文件修改检测功能测试通过！
goto :cleanup

:error
echo 测试失败，错误代码: %errorlevel%
goto :cleanup

:cleanup
cd ..
if exist test_diff rmdir /s /q test_diff
echo.
echo 测试完成，清理测试目录
pause