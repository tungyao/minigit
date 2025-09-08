@echo off
REM MiniGit完整功能演示（包含新的log命令）

echo ========================================
echo       MiniGit 完整功能演示
echo ========================================
echo.

REM 清理并创建演示目录
if exist demo_complete rmdir /s /q demo_complete
mkdir demo_complete
cd demo_complete

echo 1. 初始化新仓库
..\build\bin\Release\minigit.exe init
echo.

echo 2. 创建并提交第一个版本
echo 项目说明文档 > README.md
echo 主程序代码 > main.cpp
..\build\bin\Release\minigit.exe add README.md main.cpp
..\build\bin\Release\minigit.exe commit -m "初始版本：添加基本文件"
echo.

echo 3. 查看当前状态
..\build\bin\Release\minigit.exe status
echo.

echo 4. 查看提交历史（log命令演示）
echo === 详细格式 ===
..\build\bin\Release\minigit.exe log
echo.

echo 5. 添加更多功能并提交
echo 工具函数 > utils.cpp
echo 头文件 > utils.h
..\build\bin\Release\minigit.exe add utils.cpp utils.h
..\build\bin\Release\minigit.exe commit -m "新增：添加工具函数"
echo.

echo 6. 修改现有文件并提交
echo 更新后的主程序代码 > main.cpp
echo 配置文件 > config.txt
..\build\bin\Release\minigit.exe add main.cpp config.txt
..\build\bin\Release\minigit.exe commit -m "更新：主程序优化和配置"
echo.

echo 7. 查看提交历史（单行格式）
echo === 单行格式 ===
..\build\bin\Release\minigit.exe log --oneline
echo.

echo 8. 查看最近2次提交
echo === 最近2次提交 ===
..\build\bin\Release\minigit.exe log -n 2
echo.

echo 9. 演示reset功能（软重置）
echo === Reset演示 ===
..\build\bin\Release\minigit.exe reset --soft
..\build\bin\Release\minigit.exe status
echo.

echo 10. 查看当前提交历史
echo === 当前历史 ===
..\build\bin\Release\minigit.exe log --oneline
echo.

echo ========================================
echo       功能演示完成！
echo ========================================
echo.
echo 演示了以下功能：
echo ✓ init    - 初始化仓库
echo ✓ add     - 添加文件到暂存区
echo ✓ commit  - 提交更改
echo ✓ status  - 查看仓库状态
echo ✓ log     - 查看提交历史（详细、单行、限制数量）
echo ✓ reset   - 重置操作
echo.

cd ..
if exist demo_complete rmdir /s /q demo_complete
echo 演示完成，已清理演示目录
pause