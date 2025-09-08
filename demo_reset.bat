@echo off
REM MiniGit Reset功能演示

echo ========================================
echo          MiniGit Reset功能演示
echo ========================================
echo.

REM 清理并创建演示目录
if exist demo_reset rmdir /s /q demo_reset
mkdir demo_reset
cd demo_reset

echo 1. 初始化新仓库
..\build\bin\Release\minigit.exe init
echo.

echo 2. 创建初始文件
echo 这是初始内容 > demo.txt
echo Version 1 > version.txt
..\build\bin\Release\minigit.exe add demo.txt version.txt
..\build\bin\Release\minigit.exe commit -m "初始提交"
echo.

echo 当前仓库状态：
..\build\bin\Release\minigit.exe status
echo.

echo 3. 修改文件并提交
echo 这是修改后的内容 > demo.txt
echo Version 2 > version.txt
echo 新文件 > new.txt
..\build\bin\Release\minigit.exe add demo.txt version.txt new.txt
..\build\bin\Release\minigit.exe commit -m "第二次提交"
echo.

echo 当前仓库状态：
..\build\bin\Release\minigit.exe status
echo 当前文件：
dir /b *.txt
echo.

echo 4. 演示 reset --mixed（默认模式）
echo 这将重置暂存区但保留工作目录...
..\build\bin\Release\minigit.exe reset --mixed
echo.

echo 重置后的状态：
..\build\bin\Release\minigit.exe status
echo.

echo 5. 重新暂存文件
..\build\bin\Release\minigit.exe add demo.txt version.txt new.txt
..\build\bin\Release\minigit.exe commit -m "重新提交"
echo.

echo 6. 演示 reset --soft
echo 这将只重置HEAD指针...
..\build\bin\Release\minigit.exe reset --soft
echo.

echo Soft reset后的状态：
..\build\bin\Release\minigit.exe status
echo.

echo 7. 演示 reset --hard
echo 这将重置HEAD、暂存区和工作目录...
..\build\bin\Release\minigit.exe reset --hard
echo.

echo Hard reset后的状态：
..\build\bin\Release\minigit.exe status
echo 当前文件：
dir /b *.txt
echo.

echo ========================================
echo        Reset功能演示完成！
echo ========================================
echo.
echo 功能说明：
echo --soft  : 只重置HEAD指针
echo --mixed : 重置HEAD和暂存区（默认）
echo --hard  : 重置HEAD、暂存区和工作目录
echo.

cd ..
if exist demo_reset rmdir /s /q demo_reset
echo 演示完成，已清理演示目录
pause