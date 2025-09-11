#include "client.h"

#include <cstring>

#include "crypto.h"
#include "commit.h"
#include <iostream>
#include <sstream>
#include "filesystem_utils.h"

#include "utils.h"

// 网络操作重试宏
#define NETWORK_OPERATION_WITH_RETRY(operation, max_retries) \
    do { \
        for (int retry = 0; retry < max_retries; ++retry) { \
            if (ensureConnected()) { \
                if (operation) { \
                    break; \
                } \
                if (retry < max_retries - 1) { \
                    cout << "Network operation failed, retrying (" << (retry + 1) << "/" << max_retries << ")...\n"; \
                    connected_ = false; \
                    authenticated_ = false; \
                } \
            } else { \
                cerr << "Failed to establish connection for operation\n"; \
                return false; \
            } \
        } \
    } while(0)

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#endif

// 构造函数
Client::Client(const Config& config) : config_(config), connected_(false), authenticated_(false), encryption_enabled_(false) {
#ifdef _WIN32
	client_socket_ = INVALID_SOCKET;
#else
	client_socket_ = -1;
#endif
}

// 析构函数
Client::~Client() {
	disconnect();
	cleanupNetwork();
}

// 连接到服务器
bool Client::connect() {
	if (!initializeNetwork()) {
		cerr << "Failed to initialize network\n";
		return false;
	}

	if (!createConnection()) {
		cerr << "Failed to connect to server\n";
		cleanupNetwork();
		return false;
	}

	connected_ = true;
	cout << "Connected to " << config_.server_host << ":" << config_.server_port << "\n";

	return true;
}

// 断开连接
void Client::disconnect() {
	if (connected_) {
#ifdef _WIN32
		if (client_socket_ != INVALID_SOCKET) {
			closesocket(client_socket_);
			client_socket_ = INVALID_SOCKET;
		}
#else
		if (client_socket_ >= 0) {
			close(client_socket_);
			client_socket_ = -1;
		}
#endif
		connected_ = false;
		authenticated_ = false;
		cout << "Disconnected from server\n";
	}
}

// 认证
bool Client::authenticate() {
	//if (!ensureConnected()) {
	//	cerr << "Cannot establish connection to server\n";
	//	return false;
	//}

	vector<uint8_t> auth_data;
	bool use_rsa = false;

	if (!config_.password.empty()) {
		auth_data = Crypto::stringToBytes(config_.password);
		cout << "Debug: Client sending password: '" << config_.password << "'" << endl;
		cout << "Debug: Password length: " << config_.password.length() << endl;
		cout << "Debug: Auth data size: " << auth_data.size() << endl;
	}
	else if (!config_.cert_path.empty()) {
		use_rsa = true;
		// 加载证书数据
		auto keypair = Crypto::loadRSAKeyPair(config_.cert_path);
		auth_data = Crypto::stringToBytes(keypair.public_key);
	}
	else {
		cerr << "No authentication method configured\n";
		return false;
	}

	auto auth_request = ProtocolMessage::createAuthRequest(use_rsa, auth_data);
	if (!NetworkUtils::sendMessage(client_socket_, auth_request)) {
		cerr << "Failed to send authentication request\n";
		connected_ = false;  // 标记连接断开
		return false;
	}

	ProtocolMessage response;
	if (!NetworkUtils::receiveMessage(client_socket_, response)) {
		cerr << "Failed to receive authentication response\n";
		connected_ = false;  // 标记连接断开
		return false;
	}

	if (response.header.type != MessageType::AUTH_RESPONSE) {
		cerr << "Invalid authentication response\n";
		return false;
	}

	if (response.payload.size() < sizeof(AuthResponsePayload)) {
		cerr << "Invalid authentication response payload\n";
		return false;
	}

	AuthResponsePayload auth_response{};
	memcpy(&auth_response, response.payload.data(), sizeof(AuthResponsePayload));

	if (auth_response.status == StatusCode::SUCCESS) {
		authenticated_ = true;
		
		// 建立加密会话 - 从密码生成对称密钥
		if (!config_.password.empty()) {
			session_key_ = Crypto::generateKeyFromPassword(config_.password);
			encryption_enabled_ = Crypto::isKeyValid(session_key_);
			if (encryption_enabled_) {
				cout << "Authentication successful - encryption enabled\n";
			} else {
				cout << "Authentication successful - encryption setup failed\n";
			}
		} else {
			cout << "Authentication successful - no encryption (no password)\n";
		}
		
		return true;
	}
	else {
		cerr << "Authentication failed\n";
		return false;
	}
}

// 登录
bool Client::login() {
	if (!authenticated_) {
		cerr << "Not authenticated\n";
		return false;
	}

	if (!ensureConnected()) {
		cerr << "Cannot establish connection to server\n";
		return false;
	}

	auto login_request = ProtocolMessage(MessageType::LOGIN_REQUEST);
	if (!sendMessage(login_request)) {
		cerr << "Failed to send login request\n";
		connected_ = false;
		return false;
	}

	ProtocolMessage response;
	if (!receiveMessage(response)) {
		cerr << "Failed to receive login response\n";
		connected_ = false;
		return false;
	}
	cout << "Debug: Received message type: " << static_cast<int>(response.header.type) << endl;
	if (response.header.type == MessageType::LOGIN_RESPONSE) {
		cout << "Login successful\n";
		return true;
	}
	else {
		cerr << "Login failed\n";
		return false;
	}
}

// 列出仓库
vector<string> Client::listRepositories() {
	vector<string> repos;

	if (!authenticated_) {
		cerr << "Not authenticated\n";
		return repos;
	}

	if (!ensureConnected()) {
		cerr << "Cannot establish connection to server\n";
		return repos;
	}

	auto request = ProtocolMessage(MessageType::LIST_REPOS_REQUEST);
	if (!sendMessage(request)) {
		cerr << "Failed to send list repositories request\n";
		return repos;
	}

	ProtocolMessage response;
	if (!receiveMessage(response)) {
		cerr << "Failed to receive list repositories response\n";
		return repos;
	}

	if (response.header.type == MessageType::LIST_REPOS_RESPONSE) {
		// 解析仓库列表
		if (response.payload.size() >= sizeof(uint32_t)) {
			uint32_t repo_count;
			memcpy(&repo_count, response.payload.data(), sizeof(uint32_t));

			size_t offset = sizeof(uint32_t);
			for (uint32_t i = 0; i < repo_count && offset < response.payload.size(); ++i) {
				if (offset + sizeof(RepoListItem) <= response.payload.size()) {
					RepoListItem item;
					memcpy(&item, response.payload.data() + offset, sizeof(RepoListItem));
					offset += sizeof(RepoListItem);

					if (offset + item.name_length <= response.payload.size()) {
						string repo_name(reinterpret_cast<const char*>(response.payload.data() + offset), item.name_length);
						repos.push_back(repo_name);
						offset += item.name_length;
					}
				}
			}
		}
	}

	return repos;
}

// 日志查询
vector<string> Client::log(int max_count, bool line) {
	vector<string> log_entries;

	if (!authenticated_) {
		cerr << "Not authenticated\n";
		return log_entries;
	}

	if (current_repo_.empty()) {
		cerr << "No repository selected. Use 'use <repo>' command first.\n";
		return log_entries;
	}

	if (!ensureConnected()) {
		cerr << "Cannot establish connection to server\n";
		return log_entries;
	}

	auto request = ProtocolMessage::createLogRequest(max_count, line);
	if (!sendMessage(request)) {
		cerr << "Failed to send log request\n";
		return log_entries;
	}

	ProtocolMessage response;
	if (!receiveMessage(response)) {
		cerr << "Failed to receive log response\n";
		return log_entries;
	}

	if (response.header.type == MessageType::ERROR_MSG) {
		cerr << "Error: " << response.getStringPayload() << "\n";
		return log_entries;
	}

	if (response.header.type == MessageType::LOG_RESPONSE) {
		// 解析日志响应
		if (response.payload.size() >= sizeof(LogResponsePayload)) {
			LogResponsePayload log_payload;
			memcpy(&log_payload, response.payload.data(), sizeof(LogResponsePayload));

			size_t offset = sizeof(LogResponsePayload);
			for (uint32_t i = 0; i < log_payload.commits_count && offset < response.payload.size(); ++i) {
				// 读取commit_id
				if (offset + sizeof(uint32_t) <= response.payload.size()) {
					uint32_t commit_id_length;
					memcpy(&commit_id_length, response.payload.data() + offset, sizeof(uint32_t));
					offset += sizeof(uint32_t);

					if (offset + commit_id_length <= response.payload.size()) {
						string commit_id(reinterpret_cast<const char*>(response.payload.data() + offset), commit_id_length);
						offset += commit_id_length;

						// 读取message
						if (offset + sizeof(uint32_t) <= response.payload.size()) {
							uint32_t message_length;
							memcpy(&message_length, response.payload.data() + offset, sizeof(uint32_t));
							offset += sizeof(uint32_t);

							if (offset + message_length <= response.payload.size()) {
								string message(reinterpret_cast<const char*>(response.payload.data() + offset), message_length);
								offset += message_length;

								// 格式化输出
								if (line) {
									log_entries.push_back(commit_id.substr(0, 12) + " " + message);
								}
								else {
									log_entries.push_back("commit " + commit_id);
									log_entries.push_back("    " + message);
									log_entries.push_back(""); // 空行分隔
								}
							}
						}
					}
				}
			}
		}
	}

	return log_entries;
}

// 使用仓库
bool Client::useRepository(const string& repo_name) {
	if (!authenticated_) {
		cerr << "Not authenticated\n";
		return false;
	}

	if (!ensureConnected()) {
		cerr << "Cannot establish connection to server\n";
		return false;
	}

	auto request = ProtocolMessage::createStringMessage(MessageType::USE_REPO_REQUEST, repo_name);
	if (!sendMessage(request)) {
		cerr << "Failed to send use repository request\n";
		return false;
	}

	ProtocolMessage response;
	if (!receiveMessage(response)) {
		cerr << "Failed to receive use repository response\n";
		return false;
	}

	if (response.header.type == MessageType::USE_REPO_RESPONSE) {
		current_repo_ = repo_name;
		cout << "Using repository: " << repo_name << "\n";
		return true;
	}
	else if (response.header.type == MessageType::ERROR_MSG) {
		cerr << "Error: " << response.getStringPayload() << "\n";
		return false;
	}

	return false;
}

// 创建仓库
bool Client::createRepository(const string& repo_name) {
	if (!authenticated_) {
		cerr << "Not authenticated\n";
		return false;
	}

	if (!ensureConnected()) {
		cerr << "Cannot establish connection to server\n";
		return false;
	}

	auto request = ProtocolMessage::createStringMessage(MessageType::CREATE_REPO_REQUEST, repo_name);
	if (!sendMessage(request)) {
		cerr << "Failed to send create repository request\n";
		return false;
	}

	ProtocolMessage response;
	if (!receiveMessage(response)) {
		cerr << "Failed to receive create repository response\n";
		return false;
	}

	if (response.header.type == MessageType::CREATE_REPO_RESPONSE) {
		cout << "Repository created: " << repo_name << "\n";
		return true;
	}
	else if (response.header.type == MessageType::ERROR_MSG) {
		cerr << "Error: " << response.getStringPayload() << "\n";
		return false;
	}

	return false;
}

// 删除仓库
bool Client::removeRepository(const string& repo_name) {
	if (!authenticated_) {
		cerr << "Not authenticated\n";
		return false;
	}

	if (!ensureConnected()) {
		cerr << "Cannot establish connection to server\n";
		return false;
	}

	auto request = ProtocolMessage::createStringMessage(MessageType::REMOVE_REPO_REQUEST, repo_name);
	if (!sendMessage(request)) {
		cerr << "Failed to send remove repository request\n";
		return false;
	}

	ProtocolMessage response;
	if (!receiveMessage(response)) {
		cerr << "Failed to receive remove repository response\n";
		return false;
	}

	if (response.header.type == MessageType::REMOVE_REPO_RESPONSE) {
		cout << "Repository removed: " << repo_name << "\n";
		if (current_repo_ == repo_name) {
			current_repo_.clear();
		}
		return true;
	}
	else if (response.header.type == MessageType::ERROR_MSG) {
		cerr << "Error: " << response.getStringPayload() << "\n";
		return false;
	}

	return false;
}

// Push操作
bool Client::push() {
	if (!authenticated_) {
		cerr << "Not authenticated\n";
		return false;
	}

	if (current_repo_.empty()) {
		cerr << "No repository selected. Use 'use <repo>' first\n";
		return false;
	}

	if (!ensureConnected()) {
		cerr << "Cannot establish connection to server\n";
		return false;
	}

	// 获取本地HEAD
	string local_head = FileSystemUtils::readText(FileSystemUtils::headPath());
	if (local_head.empty()) {
		cerr << "No commits to push\n";
		return false;
	}

	cout << "Checking what to push (local HEAD: " << local_head.substr(0, 12) << ")...\n";

	// 首先获取远程HEAD来计算需要推送的commit
	// 这里我们做一个简化的检查：只检查最新的commit
	string new_commit_id = local_head;
	string commit_parent;

	// 获取当前本地最新commit的父节点
	if (!local_head.empty()) {
		auto commit_opt = CommitManager::loadCommit(local_head);
		if (commit_opt) {
			commit_parent = commit_opt->parent;
		}
	}

	// 发送push检查请求，包含将要推送的commit信息
	auto check_request = ProtocolMessage::createPushCheckRequest(local_head, new_commit_id, commit_parent);
	if (!sendMessage(check_request)) {
		cerr << "Failed to send push check request\n";
		return false;
	}

	// 接收push检查响应
	ProtocolMessage check_response;
	if (!receiveMessage(check_response)) {
		cerr << "Failed to receive push check response\n";
		return false;
	}

	if (check_response.header.type == MessageType::ERROR_MSG) {
		cerr << "Error: " << check_response.getStringPayload() << "\n";
		return false;
	}

	if (check_response.header.type != MessageType::PUSH_CHECK_RESPONSE) {
		cerr << "Invalid push check response\n";
		return false;
	}

	// 解析检查响应
	if (check_response.payload.size() < sizeof(PushCheckResponsePayload)) {
		cerr << "Invalid push check response payload\n";
		return false;
	}

	PushCheckResponsePayload check_payload;
	memcpy(&check_payload, check_response.payload.data(), sizeof(PushCheckResponsePayload));

	string remote_head;
	if (check_payload.remote_head_length > 0) {
		if (check_response.payload.size() < sizeof(PushCheckResponsePayload) + check_payload.remote_head_length) {
			cerr << "Invalid remote head data\n";
			return false;
		}
		remote_head = string(reinterpret_cast<const char*>(check_response.payload.data() + sizeof(PushCheckResponsePayload)),
			check_payload.remote_head_length);
	}

	cout << "Remote HEAD: " << (remote_head.empty() ? "(none)" : remote_head.substr(0, 12)) << "\n";

	// 检查是否需要推送
	if (check_payload.needs_update == 0) {
		cout << "Everything up-to-date\n";
		return true;
	}

	// 计算需要推送的commit列表
	vector<string> commits_to_upload = getCommitsToUpload(local_head, remote_head);
	if (commits_to_upload.empty()) {
		cout << "No commits to push\n";
		return true;
	}

	cout << "Pushing " << commits_to_upload.size() << " commit(s)...\n";

	// 上传每个commit和其对象
	std::string last_commit_id;
	for (const string& commit_id : commits_to_upload) {
		cout << "Uploading commit " << commit_id.substr(0, 12) << "...\n";
		last_commit_id = commit_id;
		if (!uploadCommit(commit_id)) {
			cerr << "Failed to upload commit " << commit_id << "\n";
			return false;
		}

		// 上传commit中的所有对象
		auto commit_opt = CommitManager::loadCommit(commit_id);
		if (!commit_opt) {
			cerr << "Cannot load commit " << commit_id << "\n";
			return false;
		}

		for (const auto& tree_item : commit_opt->tree) {
			const string& object_id = tree_item.second;
			if (!uploadObject(object_id)) {
				cerr << "Failed to upload object " << object_id << "\n";
				return false;
			}
		}
	}

	// 发送推送请求（通知服务器更新HEAD）
	auto push_request = ProtocolMessage::createPushRequest(last_commit_id);
	if (!sendMessage(push_request)) {
		cerr << "Failed to send push request\n";
		return false;
	}

	// 接收推送响应
	ProtocolMessage push_response;
	if (!receiveMessage(push_response)) {
		cerr << "Failed to receive push response\n";
		return false;
	}

	if (push_response.header.type == MessageType::PUSH_RESPONSE) {
		cout << "Push completed successfully!\n";
		return true;
	}
	else if (push_response.header.type == MessageType::ERROR_MSG) {
		cerr << "Error: " << push_response.getStringPayload() << "\n";
		return false;
	}

	return false;
}

// Pull操作
bool Client::pull() {
	if (!authenticated_) {
		cerr << "Not authenticated\n";
		return false;
	}

	if (current_repo_.empty()) {
		cerr << "No repository selected. Use 'use <repo>' first\n";
		return false;
	}

	if (!ensureConnected()) {
		cerr << "Cannot establish connection to server\n";
		return false;
	}

	// 获取本地HEAD
	string local_head;
	try {
		local_head = FileSystemUtils::readText(FileSystemUtils::headPath());
	}
	catch (exception& e) {
		// 本地可能没有提交，这是正常的
		local_head = "";
	}

	cout << "Checking what to pull (local HEAD: " << (local_head.empty() ? "(none)" : local_head.substr(0, 12)) << ")...\n";

	// 发送pull检查请求
	auto check_request = ProtocolMessage::createPullCheckRequest(local_head);
	if (!sendMessage(check_request)) {
		cerr << "Failed to send pull check request\n";
		return false;
	}

	// 接收pull检查响应
	ProtocolMessage check_response;
	if (!receiveMessage(check_response)) {
		cerr << "Failed to receive pull check response\n";
		return false;
	}

	if (check_response.header.type == MessageType::ERROR_MSG) {
		cerr << "Error: " << check_response.getStringPayload() << "\n";
		return false;
	}

	if (check_response.header.type != MessageType::PULL_CHECK_RESPONSE) {
		cerr << "Invalid pull check response\n";
		return false;
	}

	// 解析检查响应
	if (check_response.payload.size() < sizeof(PullCheckResponsePayload)) {
		cerr << "Invalid pull check response payload\n";
		return false;
	}

	PullCheckResponsePayload check_payload;
	memcpy(&check_payload, check_response.payload.data(), sizeof(PullCheckResponsePayload));

	string remote_head;
	if (check_payload.remote_head_length > 0) {
		if (check_response.payload.size() < sizeof(PullCheckResponsePayload) + check_payload.remote_head_length) {
			cerr << "Invalid remote head data\n";
			return false;
		}
		remote_head = string(reinterpret_cast<const char*>(check_response.payload.data() + sizeof(PullCheckResponsePayload)),
			check_payload.remote_head_length);
	}

	cout << "Remote HEAD: " << (remote_head.empty() ? "(none)" : remote_head.substr(0, 12)) << "\n";

	// 检查是否需要拉取
	if (check_payload.has_updates == 0) {
		cout << "Already up to date\n";
		return true;
	}

	cout << "Receiving " << check_payload.commits_count << " commit(s)...\n";
	ProtocolMessage pull_response;

	// 接收提交和对象数据
	for (uint32_t i = 0; i < check_payload.commits_count; i++) {
		// 接收提交数据
		ProtocolMessage commit_msg;
		if (!receiveMessage(commit_msg)) {
			cerr << "Failed to receive commit data\n";
			return false;
		}

		if (commit_msg.header.type == MessageType::ERROR_MSG) {
			cerr << "Error: " << commit_msg.getStringPayload() << "\n";
			return false;
		}

		if (commit_msg.header.type != MessageType::PULL_COMMIT_DATA) {
			cerr << "Expected commit data, got message type: " << static_cast<int>(commit_msg.header.type) << "\n";
			return false;
		}

		if (!receiveCommitData(commit_msg)) {
			cerr << "Failed to process commit data\n";
			return false;
		}

		// 接收该提交的所有对象
		while (true) {
			ProtocolMessage obj_msg;
			if (!receiveMessage(obj_msg)) {
				cerr << "Failed to receive message\n";
				return false;
			}

			if (obj_msg.header.type == MessageType::ERROR_MSG) {
				cerr << "Error: " << obj_msg.getStringPayload() << "\n";
				return false;
			}
			else if (obj_msg.header.type == MessageType::PULL_OBJECT_DATA) {
				if (!receiveObjectData(obj_msg)) {
					cerr << "Failed to process object data\n";
					return false;
				}
			}
			else if (obj_msg.header.type == MessageType::PULL_COMMIT_DATA) {
				// 下一个提交开始了，退出对象循环
				// 将消息放回去处理
				if (!receiveCommitData(obj_msg)) {
					cerr << "Failed to process commit data\n";
					return false;
				}
				break;
			}
			else if (obj_msg.header.type == MessageType::PULL_RESPONSE) {
				// 拉取完成
				goto pull_completed_comp;
			}
			else {
				cerr << "Unexpected message type: " << static_cast<int>(obj_msg.header.type) << "\n";
				return false;
			}
		}
	}

	// 接收最后的拉取响应
	if (!receiveMessage(pull_response)) {
		cerr << "Failed to receive pull response\n";
		return false;
	}

	if (pull_response.header.type != MessageType::PULL_RESPONSE) {
		if (pull_response.header.type == MessageType::ERROR_MSG) {
			cerr << "Error: " << pull_response.getStringPayload() << "\n";
		}
		else {
			cerr << "Expected pull response\n";
		}
		return false;
	}
	goto pull_completed_comp;

pull_completed_comp:
	// 更新本地HEAD
	if (!remote_head.empty()) {
		FileSystemUtils::writeText(FileSystemUtils::headPath(), remote_head);
		cout << "Updated HEAD to " << remote_head.substr(0, 12) << "\n";

		// 更新工作目录（类似 checkout）
		auto oc = CommitManager::loadCommit(remote_head);
		if (oc) {
			// 将文件写入工作目录
			for (auto& kv : oc->tree) {
				fs::path out = FileSystemUtils::repoRoot() / kv.first;
				fs::create_directories(out.parent_path());
				fs::copy_file(FileSystemUtils::objectsDir() / kv.second, out,
					fs::copy_options::overwrite_existing);
			}
			cout << "Updated working directory\n";
		}
	}

	cout << "Pull completed successfully!\n";
	return true;
}

// Clone操作
bool Client::clone(const string& repo_name) {
	if (!authenticated_) {
		cerr << "Not authenticated\n";
		return false;
	}

	if (!ensureConnected()) {
		cerr << "Cannot establish connection to server\n";
		return false;
	}

	auto request = ProtocolMessage::createStringMessage(MessageType::CLONE_REQUEST, repo_name);
	if (!sendMessage(request)) {
		cerr << "Failed to send clone request\n";
		return false;
	}

	// 接收克隆数据
	return receiveCloneData(repo_name);
}

// 交互式命令行
int Client::runInteractive() {
	if (!connect()) {
		return 1;
	}

	if (!authenticate()) {
		return 1;
	}

	if (!login()) {
		return 1;
	}

	cout << "\nMiniGit client connected. Type 'help' for available commands.\n";

	string input;
	while (connected_) {
		printPrompt();

		if (!getline(cin, input)) {
			break;
		}

		if (input.empty()) {
			continue;
		}

		vector<string> tokens = parseCommand(input);
		if (tokens.empty()) {
			continue;
		}

		string command = tokens[0];
		vector<string> args(tokens.begin() + 1, tokens.end());

		if (command == "quit" || command == "exit") {
			break;
		}

		if (!processInteractiveCommand(command, args)) {
			// 命令执行失败，但继续运行
		}
	}

	disconnect();
	return 0;
}

// 获取连接状态
bool Client::isConnected() const {
	return connected_;
}

// 网络初始化
bool Client::initializeNetwork() {
#ifdef _WIN32
	WSADATA wsaData;
	return WSAStartup(MAKEWORD(2, 2), &wsaData) == 0;
#else
	return true;
#endif
}

// 清理网络
void Client::cleanupNetwork() {
#ifdef _WIN32
	WSACleanup();
#endif
}

// 创建连接
bool Client::createConnection() {
	struct addrinfo hints, * res;
	int status;

	// 设置 addrinfo hints
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET; // 使用 IPv4
	hints.ai_socktype = SOCK_STREAM;

	// 获取服务器地址信息
	status = getaddrinfo(config_.server_host.c_str(), std::to_string(config_.server_port).c_str(), &hints, &res);
	if (status != 0) {
		std::cerr << "getaddrinfo failed: " << gai_strerror(status) << std::endl;
		return false;
	}

	// 创建套接字
	client_socket_ = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (client_socket_ == -1) {
		freeaddrinfo(res);
		return false;
	}

	// 连接到服务器
	if (::connect(client_socket_, res->ai_addr, res->ai_addrlen) < 0) {
		freeaddrinfo(res);
		return false;
	}

	// 释放 addrinfo 资源
	freeaddrinfo(res);

	// 设置超时
	NetworkUtils::setSocketTimeout(client_socket_, 30);

	return true;
}

// 重新连接
bool Client::reconnect() {
	cout << "Attempting to reconnect to server...\n";

	// 先断开当前连接
	disconnect();

	// 尝试重新连接
	for (int attempt = 1; attempt <= max_retry_attempts_; ++attempt) {
		cout << "Reconnection attempt " << attempt << "/" << max_retry_attempts_ << "...\n";

		if (connect()) {
			cout << "Reconnection successful!\n";

			// 重新认证
			if (authenticate()) {
				cout << "Re-authentication successful!\n";
				return true;
			}
			else {
				cerr << "Re-authentication failed\n";
				disconnect();
			}
		}

		if (attempt < max_retry_attempts_) {
			cout << "Waiting " << retry_delay_ms_ << "ms before next attempt...\n";
#ifdef _WIN32
			Sleep(retry_delay_ms_);
#else
			usleep(retry_delay_ms_ * 1000);
#endif
		}
	}

	cerr << "Reconnection failed after " << max_retry_attempts_ << " attempts\n";
	return false;
}

// 确保连接可用
bool Client::ensureConnected() {
	// 先检查连接状态
	if (!connected_) {
		return reconnect();
	}

	// 检查socket是否仍然有效
	if (!NetworkUtils::isSocketConnected(client_socket_)) {
		cout << "Connection lost, attempting to reconnect...\n";
		return reconnect();
	}

	return true;
}

// 带重试的网络操作包装器
template<typename Operation>
bool Client::performNetworkOperation(Operation operation, const string& operation_name, int max_retries) {
	for (int attempt = 1; attempt <= max_retries; ++attempt) {
		if (!ensureConnected()) {
			cerr << "Cannot establish connection for " << operation_name << "\n";
			return false;
		}

		if (operation()) {
			return true;
		}

		// 操作失败，检查是否是连接问题
		if (!NetworkUtils::isSocketConnected(client_socket_)) {
			cout << operation_name << " failed due to connection issue, retrying (" << attempt << "/" << max_retries << ")...\n";
			connected_ = false;
			authenticated_ = false;

			if (attempt < max_retries) {
#ifdef _WIN32
				Sleep(retry_delay_ms_);
#else
				usleep(retry_delay_ms_ * 1000);
#endif
			}
		}
		else {
			// 不是连接问题，直接返回失败
			break;
		}
	}

	cerr << operation_name << " failed after " << max_retries << " attempts\n";
	return false;
}

// 发送心跳
bool Client::sendHeartbeat() {
	auto heartbeat = ProtocolMessage(MessageType::HEARTBEAT);
	return sendMessage(heartbeat);
}

// 处理交互式命令
bool Client::processInteractiveCommand(const string& command, const vector<string>& args) {
	if (command == "help") {
		printHelp();
		return true;
	}
	else if (command == "ls") {
		auto repos = listRepositories();
		cout << "Repositories:\n";
		for (const string& repo : repos) {
			cout << "  " << repo;
			if (repo == current_repo_) {
				cout << " (current)";
			}
			cout << "\n";
		}
		return true;
	}
	else if (command == "use") {
		if (args.empty()) {
			cerr << "Usage: use <repository>\n";
			return false;
		}
		return useRepository(args[0]);
	}
	else if (command == "create") {
		if (args.empty()) {
			cerr << "Usage: create <repository>\n";
			return false;
		}
		return createRepository(args[0]);
	}
	else if (command == "rm") {
		if (args.empty()) {
			cerr << "Usage: rm <repository>\n";
			return false;
		}
		return removeRepository(args[0]);
	}
	else if (command == "push") {
		return push();
	}
	else if (command == "pull") {
		return pull();
	}
	else if (command == "clone") {
		if (args.empty()) {
			cerr << "Usage: clone <repository>\n";
			return false;
		}
		return clone(args[0]);
	}
	else if (command == "status") {
		cout << "Connected to: " << config_.server_host << ":" << config_.server_port << "\n";
		cout << "Current repository: " << (current_repo_.empty() ? "(none)" : current_repo_) << "\n";
		return true;
	}
	else if (command == "log") {
		// 解析参数
		int max_count = -1;
		bool line = false;

		for (size_t i = 0; i < args.size(); ++i) {
			if (args[i] == "--line") {
				line = true;
			}
			else if (args[i] == "-n" && i + 1 < args.size()) {
				try {
					max_count = stoi(args[i + 1]);
					i++; // 跳过下一个参数
				}
				catch (const exception&) {
					cerr << "Invalid number: " << args[i + 1] << "\n";
					return false;
				}
			}
		}

		auto log_entries = log(max_count, line);
		for (const string& entry : log_entries) {
			cout << entry << "\n";
		}
		return true;
	}
	else {
		cerr << "Unknown command: " << command << ". Type 'help' for available commands.\n";
		return false;
	}
}

// 打印帮助
void Client::printHelp() {
	cout << "Available commands:\n";
	cout << "  help                 - Show this help message\n";
	cout << "  ls                   - List all repositories\n";
	cout << "  use <repo>           - Use a repository\n";
	cout << "  create <repo>        - Create a new repository\n";
	cout << "  rm <repo>            - Remove a repository\n";
	cout << "  push                 - Push to current repository\n";
	cout << "  pull                 - Pull from current repository\n";
	cout << "  clone <repo>         - Clone a repository\n";
	cout << "  log [--line] [-n <count>] - Show commit log\n";
	cout << "  status               - Show connection status\n";
	cout << "  quit, exit           - Disconnect and exit\n";
}

// 打印提示符
void Client::printPrompt() {
	cout << "minigit";
	if (!current_repo_.empty()) {
		cout << ":" << current_repo_;
	}
	cout << "> ";
}

// 解析命令
vector<string> Client::parseCommand(const string& input) {
	vector<string> tokens;
	istringstream iss(input);
	string token;

	while (iss >> token) {
		tokens.push_back(token);
	}

	return tokens;
}

// 接收克隆数据
bool Client::receiveCloneData(const string& repo_name) {
	// 创建本地仓库目录
	fs::path local_repo_path = fs::current_path() / repo_name;

	try {
		// 先接收克隆开始消息
		ProtocolMessage start_msg;
		if (!receiveMessage(start_msg)) {
			cerr << "Failed to receive clone start message\n";
			return false;
		}

		if (start_msg.header.type != MessageType::CLONE_DATA_START) {
			if (start_msg.header.type == MessageType::ERROR_MSG) {
				cerr << "Clone error: " << start_msg.getStringPayload() << "\n";
			}
			return false;
		}

		// 解析克隆开始消息
		if (start_msg.payload.size() < sizeof(CloneDataStartPayload)) {
			cerr << "Invalid clone start message\n";
			return false;
		}

		CloneDataStartPayload start_payload;
		memcpy(&start_payload, start_msg.payload.data(), sizeof(CloneDataStartPayload));

		cout << "Cloning repository '" << repo_name << "'...\n";
		cout << "Total files: " << start_payload.total_files << "\n";
		cout << "Total size: " << start_payload.total_size << " bytes\n";

		// 创建本地目录
		fs::create_directories(local_repo_path);

		// 接收文件
		uint32_t files_received = 0;
		while (files_received < start_payload.total_files) {
			ProtocolMessage file_msg;
			if (!receiveMessage(file_msg)) {
				cerr << "Failed to receive file message\n";
				return false;
			}

			if (file_msg.header.type == MessageType::CLONE_FILE) {
				if (!processCloneFile(local_repo_path, file_msg)) {
					cerr << "Failed to process file\n";
					return false;
				}
				files_received++;

				// 显示进度
				cout << "Progress: " << files_received << "/" << start_payload.total_files
					<< " files received\r";
				cout.flush();

			}
			else if (file_msg.header.type == MessageType::CLONE_DATA_END) {
				break;
			}
			else if (file_msg.header.type == MessageType::ERROR_MSG) {
				cerr << "\nClone error: " << file_msg.getStringPayload() << "\n";
				return false;
			}
		}

		// 接收结束消息
		ProtocolMessage end_msg;
		if (!receiveMessage(end_msg)) {
			cerr << "\nFailed to receive clone end message\n";
			return false;
		}

		if (end_msg.header.type == MessageType::CLONE_DATA_END) {
			cout << "\nClone completed successfully!\n";
			cout << "Repository cloned to: " << local_repo_path << "\n";

			// 设置远程仓库地址到config文件中
			setRemoteConfigForClone(local_repo_path, repo_name);

			return true;
		}

		// 最后接收响应消息
		ProtocolMessage response;
		if (!receiveMessage(response)) {
			cerr << "\nFailed to receive clone response\n";
			return false;
		}

		if (response.header.type == MessageType::CLONE_RESPONSE) {
			cout << "\nClone completed: " << repo_name << "\n";
			return true;
		}
		else if (response.header.type == MessageType::ERROR_MSG) {
			cerr << "\nError: " << response.getStringPayload() << "\n";
			return false;
		}

	}
	catch (const exception& e) {
		cerr << "\nClone failed: " << e.what() << "\n";
		return false;
	}

	return false;
}

// 处理克隆文件
bool Client::processCloneFile(const fs::path& local_repo_path, const ProtocolMessage& file_msg) {
	if (file_msg.payload.size() < sizeof(CloneFilePayload)) {
		return false;
	}

	CloneFilePayload file_payload;
	memcpy(&file_payload, file_msg.payload.data(), sizeof(CloneFilePayload));

	// 提取文件路径
	if (file_msg.payload.size() < sizeof(CloneFilePayload) + file_payload.file_path_length) {
		return false;
	}

	string file_path(reinterpret_cast<const char*>(file_msg.payload.data() + sizeof(CloneFilePayload)),
		file_payload.file_path_length);

	// 提取文件数据
	size_t file_data_offset = sizeof(CloneFilePayload) + file_payload.file_path_length;
	if (file_msg.payload.size() < file_data_offset + file_payload.file_size) {
		return false;
	}

	vector<uint8_t> file_data(file_msg.payload.begin() + file_data_offset,
		file_msg.payload.begin() + file_data_offset + file_payload.file_size);

	// 验证校验和
	uint32_t calculated_crc = ProtocolMessage::calculateCRC32(file_data);
	if (calculated_crc != file_payload.checksum) {
		cerr << "CRC mismatch for file: " << file_path << "\n";
		return false;
	}

	// 写入本地文件
	fs::path full_path = local_repo_path / file_path;

	try {
		// 创建父目录
		fs::create_directories(full_path.parent_path());

		// 写入文件
		ofstream outfile(full_path, ios::binary);
		if (!outfile.is_open()) {
			cerr << "Failed to create file: " << full_path << "\n";
			return false;
		}

		outfile.write(reinterpret_cast<const char*>(file_data.data()), file_data.size());
		outfile.close();

		return true;

	}
	catch (const exception& e) {
		cerr << "Failed to write file " << file_path << ": " << e.what() << "\n";
		return false;
	}
}

// ClientCommand实现
int ClientCommand::parseAndRun(const vector<string>& args) {
	Client::Config config = parseConfig(args);

	Client client(config);
	return client.runInteractive();
}

Client::Config ClientCommand::parseConfig(const vector<string>& args) {
	Client::Config config;

	for (size_t i = 0; i < args.size(); ++i) {
		if (args[i] == "--host" && i + 1 < args.size()) {
			config.server_host = args[i + 1];
			i++;
		}
		else if (args[i] == "--port" && i + 1 < args.size()) {
			config.server_port = stoi(args[i + 1]);
			i++;
		}
		else if (args[i] == "--password" && i + 1 < args.size()) {
			config.password = args[i + 1];
			i++;
		}
		else if (args[i] == "--cert" && i + 1 < args.size()) {
			config.cert_path = args[i + 1];
			i++;
		}
	}

	return config;
}

void ClientCommand::printUsage() {
	cout << "Usage: minigit connect [--host <host>] [--port <port>] [--password <password>] [--cert <cert_path>]\n";
	cout << "Options:\n";
	cout << "  --host <host>         Server hostname (default: localhost)\n";
	cout << "  --port <port>         Server port (default: 8080)\n";
	cout << "  --password <password> Password for authentication\n";
	cout << "  --cert <cert_path>    Path to RSA certificate directory\n";
}

// CloneCommand实现
int CloneCommand::parseAndRun(const vector<string>& args) {
	if (args.empty()) {
		cerr << "Error: Clone URL required\n";
		printUsage();
		return 1;
	}

	string clone_url = args[0];
	CloneTarget target = parseCloneURL(clone_url);

	if (target.host.empty() || target.repo_name.empty()) {
		cerr << "Error: Invalid clone URL format\n";
		printUsage();
		return 1;
	}

	// 解析额外参数
	for (size_t i = 1; i < args.size(); ++i) {
		if (args[i] == "--password" && i + 1 < args.size()) {
			target.password = args[i + 1];
			i++;
		}
		else if (args[i] == "--cert" && i + 1 < args.size()) {
			target.cert_path = args[i + 1];
			i++;
		}
	}

	if (target.password.empty() && target.cert_path.empty()) {
		cerr << "Error: Authentication required (--password or --cert)\n";
		return 1;
	}

	// 创建客户端配置
	Client::Config config;
	config.server_host = target.host;
	config.server_port = target.port;
	config.password = target.password;
	config.cert_path = target.cert_path;

	// 执行克隆操作
	Client client(config);

	cout << "Connecting to " << target.host << ":" << target.port << "...\n";
	if (!client.connect()) {
		cerr << "Failed to connect to server\n";
		return 1;
	}

	if (!client.authenticate()) {
		cerr << "Authentication failed\n";
		return 1;
	}

	cout << "Cloning repository '" << target.repo_name << "'...\n";
	if (!client.clone(target.repo_name)) {
		cerr << "Clone failed\n";
		return 1;
	}

	cout << "Clone completed successfully!\n";
	return 0;
}

CloneCommand::CloneTarget CloneCommand::parseCloneURL(const string& url) {
	CloneTarget target;

	// 格式: host:port/repo 或 host/repo
	size_t slash_pos = url.find('/');
	if (slash_pos == string::npos) {
		return target; // 无效格式
	}

	string host_port = url.substr(0, slash_pos);
	target.repo_name = url.substr(slash_pos + 1);

	// 解析 host:port
	size_t colon_pos = host_port.find(':');
	if (colon_pos != string::npos) {
		target.host = host_port.substr(0, colon_pos);
		try {
			target.port = stoi(host_port.substr(colon_pos + 1));
		}
		catch (const exception&) {
			target.port = 8080; // 默认端口
		}
	}
	else {
		target.host = host_port;
		target.port = 8080; // 默认端口
	}

	return target;
}

void CloneCommand::printUsage() {
	cout << "Usage: minigit clone <host:port/repo> [--password <password>] [--cert <cert_path>]\n";
	cout << "Examples:\n";
	cout << "  minigit clone localhost:8080/myrepo --password mypass\n";
	cout << "  minigit clone server.com/myrepo --cert /path/to/cert\n";
	cout << "Options:\n";
	cout << "  --password <password> Password for authentication\n";
	cout << "  --cert <cert_path>    Path to RSA certificate directory\n";
}

// 设置远程仓库地址到config文件中
void Client::setRemoteConfigForClone(const fs::path& local_repo_path, const string& repo_name) {
	try {
		fs::path config_file = local_repo_path / MARKNAME / "config";

		// 确保.minigit目录存在
		fs::create_directories(config_file.parent_path());

		// 构建远程地址：server://host:port/repo
		string remote_url = "server://" + config_.server_host + ":" + to_string(config_.server_port) + "/" + repo_name;

		// 写入config文件
		ofstream config(config_file);
		if (config.is_open()) {
			config << "remote=" << remote_url << "\n";
			config.close();
			cout << "Remote configured: " << remote_url << "\n";
		}
		else {
			cerr << "Warning: Failed to write remote config to " << config_file << "\n";
		}

	}
	catch (const exception& e) {
		cerr << "Warning: Failed to set remote config: " << e.what() << "\n";
	}
}

// 智能push相关的辅助方法实现

// 计算需要上传的commit列表
vector<string> Client::getCommitsToUpload(const string& local_head, const string& remote_head) {
	vector<string> commits_to_upload;

	// 如果远程仓库为空，上传从local_head到根的所有commit
	if (remote_head.empty()) {
		string current_commit = local_head;
		while (!current_commit.empty()) {
			commits_to_upload.push_back(current_commit);

			auto commit_opt = CommitManager::loadCommit(current_commit);
			if (!commit_opt) {
				break;
			}
			current_commit = commit_opt->parent;
		}
		// 反转以便从最早的commit开始上传
		reverse(commits_to_upload.begin(), commits_to_upload.end());
		return commits_to_upload;
	}

	// 如果local_head和remote_head相同，不需要上传
	if (local_head == remote_head) {
		return commits_to_upload;
	}

	// 寻找从remote_head到local_head的路径
	string current_commit = local_head;
	while (!current_commit.empty() && current_commit != remote_head) {
		commits_to_upload.push_back(current_commit);

		auto commit_opt = CommitManager::loadCommit(current_commit);
		if (!commit_opt) {
			break;
		}
		current_commit = commit_opt->parent;
	}

	// 检查是否找到了remote_head
	if (current_commit != remote_head) {
		cerr << "Warning: Remote commit not found in local history. This may indicate diverged branches.\n";
		cerr << "Uploading all commits from local HEAD.\n";
	}

	// 反转以便从最早的commit开始上传
	reverse(commits_to_upload.begin(), commits_to_upload.end());
	return commits_to_upload;
}

// 上传commit数据
bool Client::uploadCommit(const string& commit_id) {
	auto commit_opt = CommitManager::loadCommit(commit_id);
	if (!commit_opt) {
		cerr << "Cannot load commit " << commit_id << "\n";
		return false;
	}

	// 序列化commit数据
	string commit_json = CommitManager::serializeCommit(*commit_opt);
	// 移除第一行的commit ID，只发送JSON数据
	auto newline_pos = commit_json.find('\n');
	if (newline_pos != string::npos) {
		commit_json = commit_json.substr(newline_pos + 1);
	}

	vector<uint8_t> commit_data(commit_json.begin(), commit_json.end());

	// 创建并发送commit数据消息
	auto commit_msg = ProtocolMessage::createPushCommitData(commit_id, commit_data);
	if (!sendMessage(commit_msg)) {
		cerr << "Failed to send commit data for " << commit_id << "\n";
		return false;
	}

	return true;
}

// 上传对象数据
bool Client::uploadObject(const string& object_id) {
	fs::path object_path = FileSystemUtils::objectsDir() / object_id;
	if (!fs::exists(object_path)) {
		cerr << "Object file not found: " << object_id << "\n";
		return false;
	}

	// 读取对象数据
	ifstream object_file(object_path, ios::binary);
	if (!object_file.is_open()) {
		cerr << "Cannot open object file: " << object_path << "\n";
		return false;
	}

	vector<uint8_t> object_data((istreambuf_iterator<char>(object_file)), {});
	object_file.close();

	// 创建并发送对象数据消息
	auto object_msg = ProtocolMessage::createPushObjectData(object_id, object_data);
	if (!sendMessage(object_msg)) {
		cerr << "Failed to send object data for " << object_id << "\n";
		return false;
	}

	return true;
}

// 获取需要下载的commit列表
vector<string> Client::getCommitsToDownload(const string& local_head, const string& remote_head) {
	vector<string> commits_to_download;

	// 如果本地没有HEAD，需要下载所有commit
	if (local_head.empty()) {
		// 从远程HEAD开始向后遍历所有commit
		string current_commit = remote_head;
		while (!current_commit.empty()) {
			commits_to_download.push_back(current_commit);
			// 加载提交并获取父提交
			auto commit_opt = CommitManager::loadCommit(current_commit);
			if (!commit_opt) {
				break; // 提交不存在，可能还没有下载
			}
			current_commit = commit_opt->parent;
		}
		// 反转顺序，从最早的commit开始
		reverse(commits_to_download.begin(), commits_to_download.end());
		return commits_to_download;
	}

	// 如果本地和远程相同，不需要下载
	if (local_head == remote_head) {
		return commits_to_download;
	}

	// 找到分叉点，只下载远程有而本地没有的commit
	set<string> local_commits;
	string current_commit = local_head;
	while (!current_commit.empty()) {
		local_commits.insert(current_commit);
		auto commit_opt = CommitManager::loadCommit(current_commit);
		if (!commit_opt) break;
		current_commit = commit_opt->parent;
	}

	// 从远程HEAD开始，收集需要的commit
	current_commit = remote_head;
	while (!current_commit.empty() && local_commits.find(current_commit) == local_commits.end()) {
		commits_to_download.push_back(current_commit);
		// 这里我们需要从服务器获取commit信息，但由于这是计算阶段，
		// 实际的commit数据会在后面的流程中接收
		break; // 简化处理，只获取一个commit
	}

	// 反转顺序，从最早的commit开始
	reverse(commits_to_download.begin(), commits_to_download.end());
	return commits_to_download;
}

// 接收提交数据
bool Client::receiveCommitData(const ProtocolMessage& msg) {
	if (msg.payload.size() < sizeof(PullCommitDataPayload)) {
		cerr << "Invalid commit data payload\n";
		return false;
	}

	PullCommitDataPayload commit_payload;
	memcpy(&commit_payload, msg.payload.data(), sizeof(PullCommitDataPayload));

	// 解析提交ID
	size_t offset = sizeof(PullCommitDataPayload);
	if (msg.payload.size() < offset + commit_payload.commit_id_length + commit_payload.commit_data_length) {
		cerr << "Invalid commit data size\n";
		return false;
	}

	string commit_id = string(reinterpret_cast<const char*>(msg.payload.data() + offset), commit_payload.commit_id_length);
	offset += commit_payload.commit_id_length;

	// 解析提交数据
	vector<uint8_t> commit_data(msg.payload.begin() + offset, msg.payload.begin() + offset + commit_payload.commit_data_length);

	cout << "Receiving commit " << commit_id.substr(0, 12) << "...\n";

	// 保存提交对象
	fs::path commit_path = FileSystemUtils::objectsDir() / commit_id;
	if (!fs::exists(commit_path)) {
		fs::create_directories(FileSystemUtils::objectsDir());
		try {
			ofstream commit_file(commit_path, ios::binary);
			if (!commit_file.is_open()) {
				cerr << "Failed to create commit file: " << commit_path << "\n";
				return false;
			}
			commit_file.write(reinterpret_cast<const char*>(commit_data.data()), commit_data.size());
			commit_file.close();
		}
		catch (const exception& e) {
			cerr << "Failed to write commit file: " << e.what() << "\n";
			return false;
		}
	}

	return true;
}

// 接收对象数据
bool Client::receiveObjectData(const ProtocolMessage& msg) {
	if (msg.payload.size() < sizeof(PullObjectDataPayload)) {
		cerr << "Invalid object data payload\n";
		return false;
	}

	PullObjectDataPayload object_payload;
	memcpy(&object_payload, msg.payload.data(), sizeof(PullObjectDataPayload));

	// 解析对象ID
	size_t offset = sizeof(PullObjectDataPayload);
	if (msg.payload.size() < offset + object_payload.object_id_length + object_payload.object_data_length) {
		cerr << "Invalid object data size\n";
		return false;
	}

	string object_id = string(reinterpret_cast<const char*>(msg.payload.data() + offset), object_payload.object_id_length);
	offset += object_payload.object_id_length;

	// 解析对象数据
	vector<uint8_t> object_data(msg.payload.begin() + offset, msg.payload.begin() + offset + object_payload.object_data_length);

	// 验证校验和
	uint32_t calculated_crc = ProtocolMessage::calculateCRC32(object_data);
	if (calculated_crc != object_payload.checksum) {
		cerr << "CRC mismatch for object: " << object_id << "\n";
		return false;
	}

	cout << "Receiving object " << object_id.substr(0, 12) << "...\n";

	// 保存对象
	fs::path object_path = FileSystemUtils::objectsDir() / object_id;
	if (!fs::exists(object_path)) {
		fs::create_directories(FileSystemUtils::objectsDir());
		try {
			ofstream object_file(object_path, ios::binary);
			if (!object_file.is_open()) {
				cerr << "Failed to create object file: " << object_path << "\n";
				return false;
			}
			object_file.write(reinterpret_cast<const char*>(object_data.data()), object_data.size());
			object_file.close();
		}
		catch (const exception& e) {
			cerr << "Failed to write object file: " << e.what() << "\n";
			return false;
		}
	}

	return true;
}

// 加密发送消息
bool Client::sendMessage(const ProtocolMessage& msg) {
	if (encryption_enabled_ && authenticated_) {
		return NetworkUtils::sendEncryptedMessage(client_socket_, msg, session_key_);
	} else {
		return NetworkUtils::sendMessage(client_socket_, msg);
	}
}

// 加密接收消息
bool Client::receiveMessage(ProtocolMessage& msg) {
	if (encryption_enabled_ && authenticated_) {
		return NetworkUtils::receiveEncryptedMessage(client_socket_, msg, session_key_);
	} else {
		return NetworkUtils::receiveMessage(client_socket_, msg);
	}
}