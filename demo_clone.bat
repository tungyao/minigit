@echo off
echo ===========================================
echo MiniGit Clone Function Demo
echo ===========================================
echo.

echo 该演示说明如何使用MiniGit的clone功能
echo.

echo 1. 启动服务器：
echo    minigit server --port 8080 --root "D:\git_repos" --password "test123"
echo.

echo 2. 创建测试仓库（在服务器端）：
echo    在另一个终端连接服务器并创建仓库：
echo    minigit connect --host localhost --port 8080 --password "test123"
echo    然后在交互模式中：
echo    ^> create testRepo
echo    ^> quit
echo.

echo 3. 使用交互式客户端clone：
echo    minigit connect --host localhost --port 8080 --password "test123"
echo    在交互模式中：
echo    ^> clone testRepo
echo.

echo 4. 使用独立clone命令：
echo    minigit clone localhost:8080/testRepo --password "test123"
echo.

echo 5. Clone功能特点：
echo    - 完整仓库克隆，包括所有文件和目录结构
echo    - 二进制传输，支持文件完整性校验（CRC32）
echo    - 进度显示，实时反馈传输状态
echo    - 支持密码和证书认证
echo    - 自动创建本地目录结构
echo.

echo 6. Clone URL格式：
echo    host:port/repository    - 指定主机、端口和仓库
echo    host/repository         - 使用默认端口8080
echo.

echo 示例：
echo    minigit clone server.example.com:9090/myproject --password "mypass"
echo    minigit clone localhost/testRepo --cert "/path/to/cert"
echo.

echo 注意：确保服务器正在运行且仓库存在

pause