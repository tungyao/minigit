@echo off
REM MiniGit文件修改检测功能完整演示

echo ========================================
echo    MiniGit 文件修改检测功能演示
echo ========================================
echo.

REM 清理并创建演示目录
if exist demo_file_status rmdir /s /q demo_file_status
mkdir demo_file_status
cd demo_file_status

echo 1. 初始化仓库并创建基础文件
..\build\bin\Release\minigit.exe init
echo 项目文档 > README.md
echo 主要功能代码 > main.cpp
echo 工具函数 > utils.cpp
echo.

echo 2. 查看初始状态（未跟踪文件）
echo === Status命令（增强版）===
..\build\bin\Release\minigit.exe status
echo.

echo === Diff命令检测未跟踪文件 ===
..\build\bin\Release\minigit.exe diff
echo.

echo 3. 添加部分文件到暂存区
..\build\bin\Release\minigit.exe add README.md main.cpp
echo.

echo === 混合状态：部分暂存，部分未跟踪 ===
..\build\bin\Release\minigit.exe status
echo.

echo === 查看暂存区内容 ===
..\build\bin\Release\minigit.exe diff --cached
echo.

echo 4. 提交暂存的文件
..\build\bin\Release\minigit.exe commit -m "初始提交：添加README和main.cpp"
echo.

echo 5. 修改已提交的文件
echo 更新后的项目文档 > README.md
echo 添加了新功能的代码 > main.cpp
echo 配置文件 > config.ini
echo.

echo === 查看修改后的状态 ===
..\build\bin\Release\minigit.exe status
echo.

echo === 详细查看所有变化 ===
..\build\bin\Release\minigit.exe diff
echo.

echo === 只显示变更的文件名 ===
..\build\bin\Release\minigit.exe diff --name-only
echo.

echo 6. 删除文件测试
del main.cpp
echo.

echo === 查看文件删除后的状态 ===
..\build\bin\Release\minigit.exe status
echo.

echo === 差异显示包含删除操作 ===
..\build\bin\Release\minigit.exe diff
echo.

echo 7. 添加新修改到暂存区
..\build\bin\Release\minigit.exe add README.md utils.cpp config.ini
echo.

echo === 混合状态：暂存+工作目录变化 ===
..\build\bin\Release\minigit.exe status
echo.

echo === 分别查看暂存区和工作目录差异 ===
echo [暂存区差异]
..\build\bin\Release\minigit.exe diff --cached
echo.
echo [工作目录差异]
..\build\bin\Release\minigit.exe diff
echo.

echo ========================================
echo       功能演示完成！
echo ========================================
echo.
echo 演示了以下新功能：
echo ✓ 增强的status命令 - 显示工作目录状态
echo ✓ 新的diff命令 - 检测文件变化
echo ✓ 文件状态标识 - M(修改) D(删除) ??(未跟踪)
echo ✓ 多种diff模式 - 工作目录 vs 暂存区 vs HEAD
echo ✓ 参数支持 - --cached, --name-only
echo.

cd ..
if exist demo_file_status rmdir /s /q demo_file_status
echo 演示完成，已清理演示目录
pause