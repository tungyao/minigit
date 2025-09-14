#pragma once

#include "common.h"
#include "crypto.h"
#include "protocol.h"

// Forward declarations for modular components
class ServerNetwork;
class ServerAuth;
class ServerRepository;
class ServerGitOps;

#ifdef _WIN32
#include <winsock2.h>
#endif

/**
 * MiniGit TCP服务器 - 组合式设计
 * 使用组件化的方式管理不同功能模块
 */
class Server {
public:
	explicit Server();
	~Server();

	// 核心服务器操作
	int run();
	void stop();

	// 网络管理 - 委托给 ServerNetwork 模块
	bool initializeNetwork();
	void cleanupNetwork();
	bool createServerSocket();
	void handleClient(int client_socket);

	// 认证和会话管理 - 委托给 ServerAuth 模块
	bool handleAuthRequest(int client_socket, shared_ptr<class ClientSession> session, const ProtocolMessage& msg);
	bool handleLoginRequest(int client_socket, shared_ptr<class ClientSession> session, const ProtocolMessage& msg);
	bool handleHeartbeat(int client_socket, shared_ptr<class ClientSession> session);
	void cleanupExpiredSessions();
	bool processMessage(int client_socket, shared_ptr<class ClientSession> session, const ProtocolMessage& msg);
	void sendErrorResponse(int client_socket, StatusCode status, const string& message);

	// 仓库管理 - 委托给 ServerRepository 模块
	bool handleListReposRequest(int client_socket, shared_ptr<class ClientSession> session, const ProtocolMessage& msg);
	bool handleUseRepoRequest(int client_socket, shared_ptr<class ClientSession> session, const ProtocolMessage& msg);
	bool handleCreateRepoRequest(int client_socket, shared_ptr<class ClientSession> session, const ProtocolMessage& msg);
	bool handleRemoveRepoRequest(int client_socket, shared_ptr<class ClientSession> session, const ProtocolMessage& msg);

	// Git操作管理 - 委托给 ServerGitOps 模块
	bool handlePushRequest(int client_socket, shared_ptr<class ClientSession> session, const ProtocolMessage& msg);
	bool handlePushCheckRequest(int client_socket, shared_ptr<class ClientSession> session, const ProtocolMessage& msg);
	bool handlePushCommitData(int client_socket, shared_ptr<class ClientSession> session, const ProtocolMessage& msg);
	bool handlePushObjectData(int client_socket, shared_ptr<class ClientSession> session, const ProtocolMessage& msg);
	bool handlePushObjectDataCompressed(int client_socket, shared_ptr<class ClientSession> session, const ProtocolMessage& msg);
	bool handlePullRequest(int client_socket, shared_ptr<class ClientSession> session, const ProtocolMessage& msg);
	bool handlePullCheckRequest(int client_socket, shared_ptr<class ClientSession> session, const ProtocolMessage& msg);
	bool handleCloneRequest(int client_socket, shared_ptr<class ClientSession> session, const ProtocolMessage& msg);
	bool handleLogRequest(int client_socket, shared_ptr<class ClientSession> session, const ProtocolMessage& msg);

	// Git操作辅助方法
	bool validatePushCommitIsLatest(const string& repo_name, const string& client_commit_parent, const string& current_remote_head);
	bool sendCloneFile(int client_socket, const string& relative_path, const fs::path& file_path);

private:
	bool running_;

#ifdef _WIN32
	SOCKET server_socket_;
#else
	int server_socket_;
#endif

	// 私有实现类 - 现在使用组合模式
	class Impl;
	unique_ptr<Impl> impl_;
};

/**
 * Server命令处理
 */
class ServerCommand {
public:
	static int parseAndRun(const vector<string>& args);

private:
	static void parseConfig(const vector<string>& args);
	static void printUsage();
};