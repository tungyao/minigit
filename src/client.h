#pragma once

#include "common.h"
#include "protocol.h"

#ifdef _WIN32
#include <winsock2.h>
#endif

/**
 * MiniGit客户端
 * 支持与服务器的长连接交互
 */
class Client {
public:
	explicit Client();

	~Client();

	// 连接到服务器
	bool connect();

	// 断开连接
	void disconnect();

	// 重新连接（自动重连）
	bool reconnect();

	// 确保连接可用（检测连接状态，必要时重连）
	bool ensureConnected();

	// 认证
	bool authenticate();

	// 登录
	bool login();

	// 列出仓库
	vector<string> listRepositories();

	// 使用仓库
	bool useRepository(const string &repo_name);

	// 创建仓库
	bool createRepository(const string &repo_name);

	// 删除仓库
	bool removeRepository(const string &repo_name);

	// Git操作
	bool push();

	bool pull();

	bool clone(const string &repo_name);

	// 日志操作
	vector<string> log(int max_count = -1, bool line = false);

	// 交互式命令行
	int runInteractive();

	// 获取连接状态
	bool isConnected() const;

private:
	bool connected_;
	bool authenticated_;
	string current_repo_;


	// 重连参数
	int max_retry_attempts_ = 3;
	int retry_delay_ms_ = 1000;

#ifdef _WIN32
	SOCKET client_socket_;
#else
	int client_socket_;
#endif

	// 网络初始化
	bool initializeNetwork();

	void cleanupNetwork();

	// 创建连接
	bool createConnection();

	// 发送心跳
	bool sendHeartbeat();

	// 带重试的网络操作包装器
	template <typename Operation>
	bool performNetworkOperation(Operation operation, const string &operation_name,
	                             int max_retries = 2);

	// 处理交互式命令
	bool processInteractiveCommand(const string &command, const vector<string> &args);

	// 工具方法
	void printHelp();

	void printPrompt();

	vector<string> parseCommand(const string &input);

	bool receiveCloneData(const string &repo_name);

	bool processCloneFile(const fs::path &local_repo_path, const ProtocolMessage &file_msg);

	bool processCloneCompressedData(const fs::path &local_repo_path, const ProtocolMessage &msg);

	void setRemoteConfigForClone(const fs::path &local_repo_path, const string &repo_name);

	// 智能push相关的辅助方法
	vector<string> getCommitsToUpload(const string &local_head, const string &remote_head);

	bool uploadCommit(const string &commit_id);

	bool uploadObject(const string &object_id);

	// 智能pull相关的辅助方法
	vector<string> getCommitsToDownload(const string &local_head, const string &remote_head);

	bool downloadCommit(const string &commit_id);

	bool downloadObject(const string &object_id);

	bool receiveCommitData(const ProtocolMessage &msg);

	bool receiveObjectData(const ProtocolMessage &msg);

	bool receiveCompressedObjectData(const ProtocolMessage &msg);
};

/**
 * 客户端命令处理
 */
class ClientCommand {
public:
	static int parseAndRun(const vector<string> &args);

private:
	static void parseConfig(const vector<string> &args);

	static void printUsage();
};

/**
 * 独立克隆命令处理
 */
class CloneCommand {
public:
	static int parseAndRun(const vector<string> &args);

private:
	struct CloneTarget {
		string host;
		int port;
		string repo_name;
		string password;
		string cert_path;
	};

	static CloneTarget parseCloneURL(const string &url);

	static void printUsage();
};