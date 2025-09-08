@echo off
echo ===========================================
echo MiniGit Server Demo
echo ===========================================
echo.

echo 该演示说明如何使用MiniGit的server功能
echo.

echo 1. 启动服务器：
echo    minigit server --port 8080 --root "D:\git_repos" --password "mypassword"
echo.

echo 2. 客户端连接：
echo    minigit connect --host localhost --port 8080 --password "mypassword"
echo.

echo 3. 在客户端交互模式中可以使用以下命令：
echo    - login                  : 登录到服务器
echo    - ls                     : 列出所有仓库
echo    - create myrepo         : 创建新仓库
echo    - use myrepo            : 使用指定仓库
echo    - rm myrepo             : 删除仓库
echo    - push                  : 推送到当前仓库
echo    - pull                  : 从当前仓库拉取
echo    - clone myrepo          : 克隆仓库
echo    - status                : 显示连接状态
echo    - help                  : 显示帮助
echo    - quit                  : 退出
echo.

echo 4. 服务器功能特点：
echo    - 支持TCP长连接
echo    - 支持密码认证和RSA证书认证
echo    - 二进制协议通信
echo    - 多仓库管理
echo    - 会话管理和超时控制
echo.

echo 5. 认证方式：
echo    密码认证：  --password "your_password"
echo    证书认证：  --cert "path/to/cert/dir"
echo.

echo 注意：编译前请确保已安装所需依赖（CMake、Visual Studio等）
echo 使用build.bat进行编译

pause