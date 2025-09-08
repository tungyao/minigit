@echo off
REM 测试SHA-1算法性能

echo 测试MiniGit SHA-1算法性能...
echo.

REM 清理和创建测试目录
if exist test_performance rmdir /s /q test_performance
mkdir test_performance
cd test_performance

echo ==================== 创建测试文件 ====================
echo 创建1MB测试文件...
fsutil file createnew test_1mb.bin 1048576
if %errorlevel% neq 0 (
    echo 创建1MB文件失败，使用PowerShell创建...
    powershell -Command "$data = New-Object byte[] 1048576; [System.IO.File]::WriteAllBytes('test_1mb.bin', $data)"
)

echo 创建5MB测试文件...
fsutil file createnew test_5mb.bin 5242880
if %errorlevel% neq 0 (
    echo 创建5MB文件失败，使用PowerShell创建...
    powershell -Command "$data = New-Object byte[] 5242880; [System.IO.File]::WriteAllBytes('test_5mb.bin', $data)"
)

echo 创建10MB测试文件...
fsutil file createnew test_10mb.bin 10485760
if %errorlevel% neq 0 (
    echo 创建10MB文件失败，使用PowerShell创建...
    powershell -Command "$data = New-Object byte[] 10485760; [System.IO.File]::WriteAllBytes('test_10mb.bin', $data)"
)

echo ==================== 初始化仓库并测试性能 ====================
..\build\bin\Release\minigit.exe init

echo === 测试1MB文件哈希性能 ===
echo 开始时间: %time%
..\build\bin\Release\minigit.exe add test_1mb.bin
echo 结束时间: %time%
echo.

echo === 测试5MB文件哈希性能 ===
echo 开始时间: %time%
..\build\bin\Release\minigit.exe add test_5mb.bin
echo 结束时间: %time%
echo.

echo === 测试10MB文件哈希性能 ===
echo 开始时间: %time%
..\build\bin\Release\minigit.exe add test_10mb.bin
echo 结束时间: %time%
echo.

echo ==================== 查看暂存区状态 ====================
..\build\bin\Release\minigit.exe status

echo 性能测试完成！
goto :cleanup

:cleanup
cd ..
if exist test_performance rmdir /s /q test_performance
echo.
echo 测试完成，清理测试目录
pause