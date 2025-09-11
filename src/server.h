#pragma once

#include "common.h"
#include "crypto.h"
#include "protocol.h"

#ifdef _WIN32
#include <winsock2.h>
#endif

/**
 * MiniGit TCP服务器
 * 支持pull/push操作的网络传输
 */
class Server {
public:
    struct Config {
        int port = 8080;
        string root_path;
        string password;
        string cert_path;
        bool use_ssl = false;
    };

    explicit Server(const Config& config);
    ~Server();

    // 启动服务器（阻塞运行）
    int run();
    
    // 停止服务器
    void stop();

private:
    Config config_;
    bool running_;
    
#ifdef _WIN32
    SOCKET server_socket_;
#else
    int server_socket_;
#endif

    // 初始化网络环境
    bool initializeNetwork();
    void cleanupNetwork();
    
    // 创建并绑定服务器socket
    bool createServerSocket();
    
    // 处理客户端连接
    void handleClient(int client_socket);
    
    // 私有实现类
    class Impl;
    unique_ptr<Impl> impl_;
    
    // 消息处理方法
    bool processMessage(int client_socket, shared_ptr<class ClientSession> session, const ProtocolMessage& msg);
    bool handleAuthRequest(int client_socket, shared_ptr<class ClientSession> session, const ProtocolMessage& msg);
    bool handleLoginRequest(int client_socket, shared_ptr<class ClientSession> session, const ProtocolMessage& msg);
    bool handleListReposRequest(int client_socket, shared_ptr<class ClientSession> session, const ProtocolMessage& msg);
    bool handleUseRepoRequest(int client_socket, shared_ptr<class ClientSession> session, const ProtocolMessage& msg);
    bool handleCreateRepoRequest(int client_socket, shared_ptr<class ClientSession> session, const ProtocolMessage& msg);
    bool handleRemoveRepoRequest(int client_socket, shared_ptr<class ClientSession> session, const ProtocolMessage& msg);
    bool handlePushRequest(int client_socket, shared_ptr<class ClientSession> session, const ProtocolMessage& msg);
    bool handlePushCheckRequest(int client_socket, shared_ptr<class ClientSession> session, const ProtocolMessage& msg);
    bool handlePushCommitData(int client_socket, shared_ptr<class ClientSession> session, const ProtocolMessage& msg);
    bool handlePushObjectData(int client_socket, shared_ptr<class ClientSession> session, const ProtocolMessage& msg);
    bool handlePullRequest(int client_socket, shared_ptr<class ClientSession> session, const ProtocolMessage& msg);
    bool handlePullCheckRequest(int client_socket, shared_ptr<class ClientSession> session, const ProtocolMessage& msg);
    bool handleCloneRequest(int client_socket, shared_ptr<class ClientSession> session, const ProtocolMessage& msg);
    bool handleLogRequest(int client_socket, shared_ptr<class ClientSession> session, const ProtocolMessage& msg);
    bool handleHeartbeat(int client_socket, shared_ptr<class ClientSession> session);
    
    // 工具方法
    void cleanupExpiredSessions();
    void sendErrorResponse(int client_socket, StatusCode status, const string& message);
    bool sendCloneFile(int client_socket, const string& relative_path, const fs::path& file_path);
    
    // Push验证方法
    bool validatePushCommitIsLatest(const string& repo_name, const string& client_commit_parent, const string& current_remote_head);

};

/**
 * Server命令处理
 */
class ServerCommand {
public:
    static int parseAndRun(const vector<string>& args);
    
private:
    static Server::Config parseConfig(const vector<string>& args);
    static void printUsage();
};