#include "server.h"
#include "protocol.h"
#include "crypto.h"
#include "filesystem_utils.h"
#include "objects.h"
#include "commit.h"
#include <thread>
#include <map>
#include <set>
#include <chrono>
#include <cstring>
#include <mutex>
#include <memory>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#define INVALID_SOCKET_VALUE INVALID_SOCKET
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h> 
#define INVALID_SOCKET_VALUE -1
#endif

// 前向声明
class ClientSession;
class RepositoryManager;

/**
 * 客户端会话管理
 */
class ClientSession {
public:
	string session_id;
	bool authenticated;
	string current_repo;
	chrono::time_point<chrono::steady_clock> last_activity;
	int socket;

	ClientSession(int sock) : socket(sock), authenticated(false) {
		session_id = generateSessionId();
		last_activity = chrono::steady_clock::now();
	}

	void updateActivity() {
		last_activity = chrono::steady_clock::now();
	}

	bool isExpired(int timeout_seconds) const {
		auto now = chrono::steady_clock::now();
		auto duration = chrono::duration_cast<chrono::seconds>(now - last_activity);
		return duration.count() > timeout_seconds;
	}

private:
	string generateSessionId() {
		auto now = chrono::system_clock::now();
		auto timestamp = chrono::duration_cast<chrono::milliseconds>(now.time_since_epoch()).count();
		return Crypto::sha256Hash(to_string(timestamp) + to_string(socket));
	}
};

/**
 * 仓库管理器
 */
class RepositoryManager {
public:
	RepositoryManager(const string& root_path) : root_path_(root_path) {
		fs::create_directories(root_path_);
	}

	vector<string> listRepositories() const {
		vector<string> repos;
		try {
			for (const auto& entry : fs::directory_iterator(root_path_)) {
				if (entry.is_directory()) {
					fs::path repo_path = entry.path();
					if (fs::exists(repo_path / MARKNAME)) {
						repos.push_back(repo_path.filename().string());
					}
				}
			}
		}
		catch (const fs::filesystem_error&) {
			// 忽略文件系统错误
		}
		return repos;
	}

	bool createRepository(const string& repo_name) {
		if (repo_name.empty() || repo_name.find("..") != string::npos) {
			return false; // 无效仓库名
		}

		fs::path repo_path = root_path_ / repo_name;
		if (fs::exists(repo_path)) {
			return false; // 仓库已存在
		}

		try {
			fs::create_directories(repo_path);
			fs::create_directories(repo_path / MARKNAME / "objects");

			// 创建初始配置文件
			ofstream config_file(repo_path / MARKNAME / "config");
			config_file << "remote=\n";
			config_file.close();

			ofstream head_file(repo_path / MARKNAME / "HEAD");
			head_file << "";
			head_file.close();

			ofstream index_file(repo_path / MARKNAME / "index");
			index_file << "";
			index_file.close();

			return true;
		}
		catch (const exception&) {
			return false;
		}
	}

	bool removeRepository(const string& repo_name) {
		if (repo_name.empty() || repo_name.find("..") != string::npos) {
			return false; // 无效仓库名
		}

		fs::path repo_path = root_path_ / repo_name;
		if (!fs::exists(repo_path / MARKNAME)) {
			return false; // 不是有效的仓库
		}

		try {
			fs::remove_all(repo_path);
			return true;
		}
		catch (const exception&) {
			return false;
		}
	}

	bool repositoryExists(const string& repo_name) const {
		if (repo_name.empty() || repo_name.find("..") != string::npos) {
			return false;
		}

		fs::path repo_path = root_path_ / repo_name;
		return fs::exists(repo_path / MARKNAME);
	}

	fs::path getRepositoryPath(const string& repo_name) const {
		return root_path_ / repo_name;
	}

private:
	fs::path root_path_;
};

// Server类的私有成员
class Server::Impl {
public:
	Config config;
	bool running;
	map<int, shared_ptr<ClientSession>> sessions;
	unique_ptr<RepositoryManager> repo_manager;
	Crypto::SymmetricKey symmetric_key;
	Crypto::RSAKeyPair rsa_keypair;
	mutex sessions_mutex;

#ifdef _WIN32
	SOCKET server_socket;
#else
	int server_socket;
#endif

	Impl(const Config& cfg) : config(cfg), running(false) {
		repo_manager = make_unique<RepositoryManager>(config.root_path);

		if (!config.password.empty()) {
			symmetric_key = Crypto::generateKeyFromPassword(config.password);
		}

		if (!config.cert_path.empty()) {
			rsa_keypair = Crypto::loadRSAKeyPair(config.cert_path);
			config.use_ssl = !rsa_keypair.private_key.empty();
		}
	}
};

// 构造函数
Server::Server(const Config& config) : impl_(make_unique<Impl>(config)) {
	config_ = config;
	running_ = false;
}

// 析构函数
Server::~Server() {
	stop();
}

// 启动服务器
int Server::run() {
	if (!initializeNetwork()) {
		cerr << "Failed to initialize network\n";
		return 1;
	}

	if (!createServerSocket()) {
		cerr << "Failed to create server socket\n";
		cleanupNetwork();
		return 1;
	}

	cout << "MiniGit server started on port " << config_.port << "\n";
	cout << "Root path: " << config_.root_path << "\n";

	if (!config_.password.empty()) {
		cout << "Authentication: Password-based\n";
	}
	else if (config_.use_ssl) {
		cout << "Authentication: RSA certificate\n";
	}
	else {
		cout << "Authentication: None (WARNING: Insecure!)\n";
	}

	running_ = true;

	// 主服务循环
	while (running_) {
		fd_set read_fds;
		FD_ZERO(&read_fds);
		FD_SET(server_socket_, &read_fds);

		struct timeval timeout;
		timeout.tv_sec = 1;
		timeout.tv_usec = 0;

		int activity = select(server_socket_ + 1, &read_fds, nullptr, nullptr, &timeout);

		if (activity < 0) {
			if (running_) {
				cerr << "Select error\n";
			}
			break;
		}

		if (activity > 0 && FD_ISSET(server_socket_, &read_fds)) {
			// 接受新连接
			struct sockaddr_in client_addr;
			socklen_t client_len = sizeof(client_addr);

#ifdef _WIN32
			SOCKET client_socket = accept(server_socket_, (struct sockaddr*)&client_addr, &client_len);
			if (client_socket == INVALID_SOCKET) {
				continue;
			}
#else
			int client_socket = accept(server_socket_, (struct sockaddr*)&client_addr, &client_len);
			if (client_socket < 0) {
				continue;
			}
#endif

			// 设置socket选项
			NetworkUtils::setSocketTimeout(client_socket, 30);

			// 创建新线程处理客户端
			std::thread([this, client_socket]() {
				handleClient(client_socket);
				}).detach();
		}

		// 清理过期会话
		cleanupExpiredSessions();
	}

	cleanupNetwork();
	return 0;
}

// 停止服务器
void Server::stop() {
	running_ = false;

	if (server_socket_ != INVALID_SOCKET_VALUE) {
#ifdef _WIN32
		closesocket(server_socket_);
#else
		close(server_socket_);
#endif
		server_socket_ = INVALID_SOCKET_VALUE;
	}
}

// 初始化网络环境
bool Server::initializeNetwork() {
#ifdef _WIN32
	WSADATA wsaData;
	return WSAStartup(MAKEWORD(2, 2), &wsaData) == 0;
#else
	return true;
#endif
}

// 清理网络环境
void Server::cleanupNetwork() {
#ifdef _WIN32
	WSACleanup();
#endif
}

// 创建服务器socket
bool Server::createServerSocket() {
#ifdef _WIN32
	server_socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (server_socket_ == INVALID_SOCKET_VALUE) {
		return false;
	}
#else
	server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
	if (server_socket_ < 0) {
		return false;
	}
#endif

	// 设置socket选项
	int opt = 1;
	setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

	// 绑定地址
	struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(config_.port);

	if (bind(server_socket_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
		return false;
	}

	// 开始监听
	if (listen(server_socket_, 10) < 0) {
		return false;
	}

	return true;
}

// Push验证方法：检查客户端提交的commit是否是最新的
bool Server::validatePushCommitIsLatest(const string& repo_name, const string& client_commit_parent, const string& current_remote_head) {
	// 如果远程仓库是空的（没有HEAD），则允许任何提交
	if (current_remote_head.empty()) {
		return true;
	}

	// 如果客户端的commit父节点与当前远程HEAD相同，则是最新的
	if (client_commit_parent == current_remote_head) {
		return true;
	}

	// 如果客户端的commit没有父节点（初始提交），但远程已经有提交了，则不允许
	if (client_commit_parent.empty() && !current_remote_head.empty()) {
		return false;
	}

	// 其他情况下，需要检查客户端的父节点是否在远程的提交历史中
	// 这里进行简化处理：只检查直接父子关系
	// 在更复杂的实现中，需要遍历整个提交历史树
	return false;
}

// 处理客户端连接
void Server::handleClient(int client_socket) {
	auto session = make_shared<ClientSession>(client_socket);

	{
		lock_guard<mutex> lock(impl_->sessions_mutex);
		impl_->sessions[client_socket] = session;
	}

	cout << "Client connected: " << client_socket << "\n";

	try {
		// 主消息处理循环
		while (running_ && NetworkUtils::isSocketConnected(client_socket)) {
			ProtocolMessage msg;
			if (!NetworkUtils::receiveMessage(client_socket, msg)) {
				break;
			}

			session->updateActivity();

			// 处理消息
			if (!processMessage(client_socket, session, msg)) {
				break;
			}
		}
	}
	catch (const exception& e) {
		cerr << "Client handler error: " << e.what() << "\n";
	}

	// 清理会话
	{
		lock_guard<mutex> lock(impl_->sessions_mutex);
		impl_->sessions.erase(client_socket);
	}

#ifdef _WIN32
	closesocket(client_socket);
#else
	close(client_socket);
#endif

	cout << "Client disconnected: " << client_socket << "\n";
}

// 处理协议消息
bool Server::processMessage(int client_socket, shared_ptr<ClientSession> session, const ProtocolMessage& msg) {
	switch (msg.header.type) {
	case MessageType::AUTH_REQUEST:
		return handleAuthRequest(client_socket, session, msg);

	case MessageType::LOGIN_REQUEST:
		return handleLoginRequest(client_socket, session, msg);

	case MessageType::LIST_REPOS_REQUEST:
		return handleListReposRequest(client_socket, session, msg);

	case MessageType::USE_REPO_REQUEST:
		return handleUseRepoRequest(client_socket, session, msg);

	case MessageType::CREATE_REPO_REQUEST:
		return handleCreateRepoRequest(client_socket, session, msg);

	case MessageType::REMOVE_REPO_REQUEST:
		return handleRemoveRepoRequest(client_socket, session, msg);

	case MessageType::PUSH_REQUEST:
		return handlePushRequest(client_socket, session, msg);

	case MessageType::PUSH_CHECK_REQUEST:
		return handlePushCheckRequest(client_socket, session, msg);

	case MessageType::PUSH_COMMIT_DATA:
		return handlePushCommitData(client_socket, session, msg);

	case MessageType::PUSH_OBJECT_DATA:
		return handlePushObjectData(client_socket, session, msg);

	case MessageType::PULL_REQUEST:
		return handlePullRequest(client_socket, session, msg);

	case MessageType::PULL_CHECK_REQUEST:
		return handlePullCheckRequest(client_socket, session, msg);

	case MessageType::CLONE_REQUEST:
		return handleCloneRequest(client_socket, session, msg);

	case MessageType::LOG_REQUEST:
		return handleLogRequest(client_socket, session, msg);

	case MessageType::HEARTBEAT:
		return 1;

	default:
		sendErrorResponse(client_socket, StatusCode::INVALID_REQUEST, "Unknown message type");
		return false;
	}
}

// 清理过期会话
void Server::cleanupExpiredSessions() {
	lock_guard<mutex> lock(impl_->sessions_mutex);

	vector<int> expired_sockets;
	for (const auto& pair : impl_->sessions) {
		if (pair.second->isExpired(300)) { // 5分钟超时
			expired_sockets.push_back(pair.first);
		}
	}

	for (int socket : expired_sockets) {
		impl_->sessions.erase(socket);
#ifdef _WIN32
		closesocket(socket);
#else
		close(socket);
#endif
	}
}

// 发送错误响应
void Server::sendErrorResponse(int client_socket, StatusCode status, const string& message) {
	auto error_msg = ProtocolMessage::createErrorMessage(status, message);
	NetworkUtils::sendMessage(client_socket, error_msg);
}

// 发送克隆文件
bool Server::sendCloneFile(int client_socket, const string& relative_path, const fs::path& file_path) {
	try {
		// 读取文件数据
		ifstream file(file_path, ios::binary);
		if (!file.is_open()) {
			return false;
		}

		vector<uint8_t> file_data((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
		file.close();

		// 创建并发送文件消息
		auto file_msg = ProtocolMessage::createCloneFile(relative_path, file_data, 0); // 0 = 文件类型
		return NetworkUtils::sendMessage(client_socket, file_msg);

	}
	catch (const exception&) {
		return false;
	}
}

// 认证请求处理
bool Server::handleAuthRequest(int client_socket, shared_ptr<ClientSession> session, const ProtocolMessage& msg) {
	if (msg.payload.size() < sizeof(AuthRequestPayload)) {
		sendErrorResponse(client_socket, StatusCode::INVALID_REQUEST, "Invalid auth request");
		return false;
	}

	AuthRequestPayload auth_payload;
	memcpy(&auth_payload, msg.payload.data(), sizeof(AuthRequestPayload));

	// 提取认证数据
	if (msg.payload.size() < sizeof(AuthRequestPayload) + auth_payload.data_size) {
		sendErrorResponse(client_socket, StatusCode::INVALID_REQUEST, "Invalid auth data");
		return false;
	}

	vector<uint8_t> auth_data(msg.payload.begin() + sizeof(AuthRequestPayload),
		msg.payload.begin() + sizeof(AuthRequestPayload) + auth_payload.data_size);

	bool auth_success = false;

	if (auth_payload.auth_type == 0) {
		// 密码认证
		if (!config_.password.empty()) {
			string provided_password = Crypto::bytesToString(auth_data);
			cout << "Debug: Server password: '" << config_.password << "'" << endl;
			cout << "Debug: Provided password: '" << provided_password << "'" << endl;
			cout << "Debug: Lengths - server: " << config_.password.length() << ", provided: " << provided_password.length() << endl;
			auth_success = (provided_password == config_.password);
			cout << "Debug: Auth result: " << (auth_success ? "SUCCESS" : "FAILED") << endl;
		}
	}
	else if (auth_payload.auth_type == 1) {
		// RSA证书认证
		if (config_.use_ssl) {
			// 简化的证书验证
			auth_success = !auth_data.empty();
		}
	}

	StatusCode status = auth_success ? StatusCode::SUCCESS : StatusCode::AUTH_FAILED;
	uint32_t timeout = auth_success ? 3600 : 0; // 1小时超时

	if (auth_success) {
		session->authenticated = true;
	}

	auto response = ProtocolMessage::createAuthResponse(status, session->session_id, timeout);
	return NetworkUtils::sendMessage(client_socket, response);
}

// 登录请求处理
bool Server::handleLoginRequest(int client_socket, shared_ptr<ClientSession> session, const ProtocolMessage& msg) {
	if (!session->authenticated) {
		sendErrorResponse(client_socket, StatusCode::AUTH_REQUIRED, "Authentication required");
		return false;
	}

	auto response = ProtocolMessage::createStringMessage(MessageType::LOGIN_RESPONSE, "Login successful");
	return NetworkUtils::sendMessage(client_socket, response);
}

// 列出仓库请求处理
bool Server::handleListReposRequest(int client_socket, shared_ptr<ClientSession> session, const ProtocolMessage& msg) {
	if (!session->authenticated) {
		sendErrorResponse(client_socket, StatusCode::AUTH_REQUIRED, "Authentication required");
		return false;
	}

	vector<string> repos = impl_->repo_manager->listRepositories();
	vector<pair<string, RepoListItem>> repo_list;

	for (const string& repo_name : repos) {
		RepoListItem item;
		item.name_length = static_cast<uint32_t>(repo_name.size());
		item.last_modified = static_cast<uint64_t>(chrono::system_clock::now().time_since_epoch().count());
		item.commit_count = 0; // TODO: 计算实际提交数量

		repo_list.push_back({ repo_name, item });
	}

	auto response = ProtocolMessage::createRepoListResponse(repo_list);
	return NetworkUtils::sendMessage(client_socket, response);
}

// 使用仓库请求处理
bool Server::handleUseRepoRequest(int client_socket, shared_ptr<ClientSession> session, const ProtocolMessage& msg) {
	if (!session->authenticated) {
		sendErrorResponse(client_socket, StatusCode::AUTH_REQUIRED, "Authentication required");
		return false;
	}

	string repo_name = msg.getStringPayload();
	if (repo_name.empty()) {
		sendErrorResponse(client_socket, StatusCode::INVALID_REQUEST, "Repository name required");
		return false;
	}

	if (!impl_->repo_manager->repositoryExists(repo_name)) {
		sendErrorResponse(client_socket, StatusCode::REPO_NOT_FOUND, "Repository not found");
		return false;
	}

	session->current_repo = repo_name;

	auto response = ProtocolMessage::createStringMessage(MessageType::USE_REPO_RESPONSE,
		"Using repository: " + repo_name);
	return NetworkUtils::sendMessage(client_socket, response);
}

// 创建仓库请求处理
bool Server::handleCreateRepoRequest(int client_socket, shared_ptr<ClientSession> session, const ProtocolMessage& msg) {
	if (!session->authenticated) {
		sendErrorResponse(client_socket, StatusCode::AUTH_REQUIRED, "Authentication required");
		return false;
	}

	string repo_name = msg.getStringPayload();
	if (repo_name.empty()) {
		sendErrorResponse(client_socket, StatusCode::INVALID_REQUEST, "Repository name required");
		return false;
	}

	if (impl_->repo_manager->repositoryExists(repo_name)) {
		sendErrorResponse(client_socket, StatusCode::REPO_EXISTS, "Repository already exists");
		return false;
	}

	if (impl_->repo_manager->createRepository(repo_name)) {
		auto response = ProtocolMessage::createStringMessage(MessageType::CREATE_REPO_RESPONSE,
			"Repository created: " + repo_name);
		return NetworkUtils::sendMessage(client_socket, response);
	}
	else {
		sendErrorResponse(client_socket, StatusCode::SERVER_ERROR, "Failed to create repository");
		return false;
	}
}

// 删除仓库请求处理
bool Server::handleRemoveRepoRequest(int client_socket, shared_ptr<ClientSession> session, const ProtocolMessage& msg) {
	if (!session->authenticated) {
		sendErrorResponse(client_socket, StatusCode::AUTH_REQUIRED, "Authentication required");
		return false;
	}

	string repo_name = msg.getStringPayload();
	if (repo_name.empty()) {
		sendErrorResponse(client_socket, StatusCode::INVALID_REQUEST, "Repository name required");
		return false;
	}

	if (!impl_->repo_manager->repositoryExists(repo_name)) {
		sendErrorResponse(client_socket, StatusCode::REPO_NOT_FOUND, "Repository not found");
		return false;
	}

	if (impl_->repo_manager->removeRepository(repo_name)) {
		// 如果删除的是当前使用的仓库，清除会话中的当前仓库
		if (session->current_repo == repo_name) {
			session->current_repo.clear();
		}

		auto response = ProtocolMessage::createStringMessage(MessageType::REMOVE_REPO_RESPONSE,
			"Repository removed: " + repo_name);
		return NetworkUtils::sendMessage(client_socket, response);
	}
	else {
		sendErrorResponse(client_socket, StatusCode::SERVER_ERROR, "Failed to remove repository");
		return false;
	}
}

// 推送请求处理（最终的push请求，更新远程HEAD）
bool Server::handlePushRequest(int client_socket, shared_ptr<ClientSession> session,
	const ProtocolMessage& msg) {
	if (!session->authenticated) {
		sendErrorResponse(client_socket, StatusCode::AUTH_REQUIRED, "Authentication required");
		return false;
	}

	if (session->current_repo.empty()) {
		sendErrorResponse(client_socket, StatusCode::INVALID_REQUEST, "No repository selected");
		return false;
	}

	// 获取仓库路径
	fs::path repo_path = impl_->repo_manager->getRepositoryPath(session->current_repo);
	if (!fs::exists(repo_path)) {
		sendErrorResponse(client_socket, StatusCode::REPO_NOT_FOUND, "Repository not found");
		return false;
	}
	// 获取当前远程HEAD用于验证
	string current_remote_head;
	fs::path remote_head_path = repo_path / MARKNAME / "HEAD";
	if (fs::exists(remote_head_path)) {
		try {
			ifstream head_file(remote_head_path);
			if (head_file.is_open()) {
				getline(head_file, current_remote_head);
				head_file.close();
				// 移除尾部的换行符
				while (!current_remote_head.empty() && (current_remote_head.back() == '\n' || current_remote_head.back() == '\r')) {
					current_remote_head.pop_back();
				}
			}
		}
		catch (const exception& e) {
			// 如果读取失败，视为空仓库
			current_remote_head.clear();
		}
	}

	// 解析客户端的HEAD
	if (msg.payload.size() < sizeof(PushRequestPayload)) {
		sendErrorResponse(client_socket, StatusCode::INVALID_REQUEST, "Invalid push request request");
		return false;
	}

	PushRequestPayload request_payload;
	memcpy(&request_payload, msg.payload.data(), sizeof(PushRequestPayload));

	string new_remote_head;
	if (request_payload.remote_head_length > 0) {
		if (msg.payload.size() < sizeof(PushRequestPayload) + request_payload.remote_head_length) {
			sendErrorResponse(client_socket, StatusCode::INVALID_REQUEST, "Invalid local head data");
			return false;
		}
		new_remote_head = string(reinterpret_cast<const char*>(msg.payload.data() + sizeof(PushRequestPayload)),
			request_payload.remote_head_length);
	}

	// 验证客户端提交的commit是否是最新的
	if (!new_remote_head.empty()) {
		// 从服务器的objects目录中读取客户端提交的commit数据来获取其父节点
		string client_commit_parent;
		fs::path commit_obj_path = repo_path / MARKNAME / "objects" / new_remote_head;
		if (fs::exists(commit_obj_path)) {
			try {
				ifstream commit_file(commit_obj_path, ios::binary);
				if (commit_file.is_open()) {
					vector<uint8_t> commit_data((istreambuf_iterator<char>(commit_file)), {});
					commit_file.close();

					// 解析commit数据获取父节点
					string commit_content = string(commit_data.begin(), commit_data.end());
					Commit commit = CommitManager::deserializeCommit(new_remote_head + "\n" + commit_content);
					client_commit_parent = commit.parent;
				}
			}
			catch (const exception& e) {
				// 如果无法解析commit，拒绝push
				sendErrorResponse(client_socket, StatusCode::INVALID_REQUEST, "Cannot parse commit data");
				return false;
			}
		}
		else {
			// 如果commit对象不存在，说明客户端没有先上传commit数据
			sendErrorResponse(client_socket, StatusCode::INVALID_REQUEST, "Commit object not found, please upload commit data first");
			return false;
		}

		// 验证提交是否基于最新版本
		if (!validatePushCommitIsLatest(session->current_repo, client_commit_parent, current_remote_head)) {
			sendErrorResponse(client_socket, StatusCode::INVALID_REQUEST,
				"Push rejected: commit is not based on the latest version. Please pull the latest changes first.");
			return false;
		}
	}

	// 检查提交的commit是不是最新的版本 - 验证通过，更新HEAD
	if (!new_remote_head.empty()) {
		try {
			ofstream head_file(remote_head_path);
			if (head_file.is_open()) {
				head_file << new_remote_head;
				head_file.close();
			}
		}
		catch (const exception& e) {
			sendErrorResponse(client_socket, StatusCode::SERVER_ERROR, "Failed to update remote HEAD");
			return false;
		}
	}

	auto response = ProtocolMessage::createStringMessage(MessageType::PUSH_RESPONSE, "Push completed");
	return NetworkUtils::sendMessage(client_socket, response);
}

// 推送检查请求处理
bool Server::handlePushCheckRequest(int client_socket, shared_ptr<ClientSession> session,
	const ProtocolMessage& msg) {
	if (!session->authenticated) {
		sendErrorResponse(client_socket, StatusCode::AUTH_REQUIRED, "Authentication required");
		return false;
	}

	if (session->current_repo.empty()) {
		sendErrorResponse(client_socket, StatusCode::INVALID_REQUEST, "No repository selected");
		return false;
	}

	// 解析客户端的检查请求数据
	if (msg.payload.size() < sizeof(PushCheckRequestPayload)) {
		sendErrorResponse(client_socket, StatusCode::INVALID_REQUEST, "Invalid push check request");
		return false;
	}

	PushCheckRequestPayload check_payload;
	memcpy(&check_payload, msg.payload.data(), sizeof(PushCheckRequestPayload));

	// 检查数据完整性
	size_t required_size = sizeof(PushCheckRequestPayload) + 
						check_payload.local_head_length + 
						check_payload.new_commit_id_length + 
						check_payload.commit_parent_length;
	if (msg.payload.size() < required_size) {
		sendErrorResponse(client_socket, StatusCode::INVALID_REQUEST, "Incomplete push check request data");
		return false;
	}

	// 解析各个字段
	size_t offset = sizeof(PushCheckRequestPayload);
	
	string local_head;
	if (check_payload.local_head_length > 0) {
		local_head = string(reinterpret_cast<const char*>(msg.payload.data() + offset), check_payload.local_head_length);
	}
	offset += check_payload.local_head_length;

	string new_commit_id;
	if (check_payload.new_commit_id_length > 0) {
		new_commit_id = string(reinterpret_cast<const char*>(msg.payload.data() + offset), check_payload.new_commit_id_length);
	}
	offset += check_payload.new_commit_id_length;

	string commit_parent;
	if (check_payload.commit_parent_length > 0) {
		commit_parent = string(reinterpret_cast<const char*>(msg.payload.data() + offset), check_payload.commit_parent_length);
	}

	// 获取远程仓库的HEAD
	fs::path repo_path = impl_->repo_manager->getRepositoryPath(session->current_repo);
	string remote_head;
	bool needs_update = true;

	fs::path remote_head_path = repo_path / MARKNAME / "HEAD";
	if (fs::exists(remote_head_path)) {
		try {
			ifstream head_file(remote_head_path);
			if (head_file.is_open()) {
				getline(head_file, remote_head);
				head_file.close();
				// 移除尾部的换行符
				while (!remote_head.empty() && (remote_head.back() == '\n' || remote_head.back() == '\r')) {
					remote_head.pop_back();
				}
			}
		}
		catch (const exception& e) {
			// 如果读取失败，视为空仓库
			remote_head.clear();
		}
	}

	// 检查是否需要更新
	if (!remote_head.empty() && remote_head == local_head) {
		needs_update = false;
	}

	// 如果需要更新，在这里验证客户端的提交是否是最新的
	if (needs_update && !new_commit_id.empty()) {
		if (!validatePushCommitIsLatest(session->current_repo, commit_parent, remote_head)) {
			sendErrorResponse(client_socket, StatusCode::INVALID_REQUEST, 
				"Push rejected: commit is not based on the latest version. Please pull the latest changes first.");
			return false;
		}
	}

	// 发送检查响应
	auto response = ProtocolMessage::createPushCheckResponse(remote_head, needs_update);
	return NetworkUtils::sendMessage(client_socket, response);
}

// 推送提交数据处理
bool Server::handlePushCommitData(int client_socket, shared_ptr<ClientSession> session, const ProtocolMessage& msg) {
	if (!session->authenticated) {
		sendErrorResponse(client_socket, StatusCode::AUTH_REQUIRED, "Authentication required");
		return false;
	}

	if (session->current_repo.empty()) {
		sendErrorResponse(client_socket, StatusCode::INVALID_REQUEST, "No repository selected");
		return false;
	}

	// 解析commit数据
	if (msg.payload.size() < sizeof(PushCommitDataPayload)) {
		sendErrorResponse(client_socket, StatusCode::INVALID_REQUEST, "Invalid commit data");
		return false;
	}

	PushCommitDataPayload commit_payload;
	memcpy(&commit_payload, msg.payload.data(), sizeof(PushCommitDataPayload));

	if (msg.payload.size() < sizeof(PushCommitDataPayload) + commit_payload.commit_id_length + commit_payload.commit_data_length) {
		sendErrorResponse(client_socket, StatusCode::INVALID_REQUEST, "Incomplete commit data");
		return false;
	}

	// 提取commit ID
	string commit_id(reinterpret_cast<const char*>(msg.payload.data() + sizeof(PushCommitDataPayload)),
		commit_payload.commit_id_length);

	// 提取commit数据
	vector<uint8_t> commit_data(msg.payload.begin() + sizeof(PushCommitDataPayload) + commit_payload.commit_id_length,
		msg.payload.begin() + sizeof(PushCommitDataPayload) + commit_payload.commit_id_length + commit_payload.commit_data_length);

	// 将commit保存到远程仓库
	fs::path repo_path = impl_->repo_manager->getRepositoryPath(session->current_repo);
	fs::path objects_dir = repo_path / MARKNAME / "objects";
	fs::create_directories(objects_dir);

	fs::path commit_file = objects_dir / commit_id;
	try {
		ofstream outfile(commit_file, ios::binary);
		if (!outfile.is_open()) {
			sendErrorResponse(client_socket, StatusCode::SERVER_ERROR, "Failed to create commit file");
			return false;
		}
		outfile.write(reinterpret_cast<const char*>(commit_data.data()), commit_data.size());
		outfile.close();
	}
	catch (const exception& e) {
		sendErrorResponse(client_socket, StatusCode::SERVER_ERROR, "Failed to write commit data");
		return false;
	}

	return true; // 不发送响应，等待更多数据或最终的PUSH_REQUEST
}

// 推送对象数据处理
bool Server::handlePushObjectData(int client_socket, shared_ptr<ClientSession> session, const ProtocolMessage& msg) {
	if (!session->authenticated) {
		sendErrorResponse(client_socket, StatusCode::AUTH_REQUIRED, "Authentication required");
		return false;
	}

	if (session->current_repo.empty()) {
		sendErrorResponse(client_socket, StatusCode::INVALID_REQUEST, "No repository selected");
		return false;
	}

	// 解析对象数据
	if (msg.payload.size() < sizeof(PushObjectDataPayload)) {
		sendErrorResponse(client_socket, StatusCode::INVALID_REQUEST, "Invalid object data");
		return false;
	}

	PushObjectDataPayload object_payload;
	memcpy(&object_payload, msg.payload.data(), sizeof(PushObjectDataPayload));

	if (msg.payload.size() < sizeof(PushObjectDataPayload) + object_payload.object_id_length + object_payload.object_data_length) {
		sendErrorResponse(client_socket, StatusCode::INVALID_REQUEST, "Incomplete object data");
		return false;
	}

	// 提取对象ID
	string object_id(reinterpret_cast<const char*>(msg.payload.data() + sizeof(PushObjectDataPayload)),
		object_payload.object_id_length);

	// 提取对象数据
	vector<uint8_t> object_data(msg.payload.begin() + sizeof(PushObjectDataPayload) + object_payload.object_id_length,
		msg.payload.begin() + sizeof(PushObjectDataPayload) + object_payload.object_id_length + object_payload.object_data_length);

	// 验证数据校验和
	uint32_t calculated_checksum = ProtocolMessage::calculateCRC32(object_data);
	if (calculated_checksum != object_payload.checksum) {
		sendErrorResponse(client_socket, StatusCode::INVALID_REQUEST, "Object data checksum mismatch");
		return false;
	}

	// 将对象保存到远程仓库
	fs::path repo_path = impl_->repo_manager->getRepositoryPath(session->current_repo);
	fs::path objects_dir = repo_path / MARKNAME / "objects";
	fs::create_directories(objects_dir);

	fs::path object_file = objects_dir / object_id;
	// 如果对象已存在，跳过
	if (fs::exists(object_file)) {
		return true;
	}

	try {
		ofstream outfile(object_file, ios::binary);
		if (!outfile.is_open()) {
			sendErrorResponse(client_socket, StatusCode::SERVER_ERROR, "Failed to create object file");
			return false;
		}
		outfile.write(reinterpret_cast<const char*>(object_data.data()), object_data.size());
		outfile.close();
	}
	catch (const exception& e) {
		sendErrorResponse(client_socket, StatusCode::SERVER_ERROR, "Failed to write object data");
		return false;
	}

	return true; // 不发送响应，等待更多数据或最终的PUSH_REQUEST
}
bool Server::handlePullRequest(int client_socket, shared_ptr<ClientSession> session, const ProtocolMessage& msg) {
	if (!session->authenticated) {
		sendErrorResponse(client_socket, StatusCode::AUTH_REQUIRED, "Authentication required");
		return false;
	}

	if (session->current_repo.empty()) {
		sendErrorResponse(client_socket, StatusCode::INVALID_REQUEST, "No repository selected");
		return false;
	}

	// 发送拉取完成响应即可，因为实际数据传输在PULL_CHECK阶段完成
	auto response = ProtocolMessage::createStringMessage(MessageType::PULL_RESPONSE, "Pull completed");
	return NetworkUtils::sendMessage(client_socket, response);
}

// 拉取检查请求处理
bool Server::handlePullCheckRequest(int client_socket, shared_ptr<ClientSession> session, const ProtocolMessage& msg) {
	if (!session->authenticated) {
		sendErrorResponse(client_socket, StatusCode::AUTH_REQUIRED, "Authentication required");
		return false;
	}

	if (session->current_repo.empty()) {
		sendErrorResponse(client_socket, StatusCode::INVALID_REQUEST, "No repository selected");
		return false;
	}

	// 解析客户端的HEAD
	if (msg.payload.size() < sizeof(PullCheckRequestPayload)) {
		sendErrorResponse(client_socket, StatusCode::INVALID_REQUEST, "Invalid pull check request");
		return false;
	}

	PullCheckRequestPayload check_payload;
	memcpy(&check_payload, msg.payload.data(), sizeof(PullCheckRequestPayload));

	string local_head;
	if (check_payload.local_head_length > 0) {
		if (msg.payload.size() < sizeof(PullCheckRequestPayload) + check_payload.local_head_length) {
			sendErrorResponse(client_socket, StatusCode::INVALID_REQUEST, "Invalid local head data");
			return false;
		}
		local_head = string(reinterpret_cast<const char*>(msg.payload.data() + sizeof(PullCheckRequestPayload)),
			check_payload.local_head_length);
	}

	// 获取远程仓库的HEAD
	fs::path repo_path = impl_->repo_manager->getRepositoryPath(session->current_repo);
	string remote_head;
	try {
		fs::path remote_head_path = repo_path / MARKNAME / "HEAD";
		if (fs::exists(remote_head_path)) {
			ifstream head_file(remote_head_path);
			if (head_file.is_open()) {
				getline(head_file, remote_head);
				head_file.close();
			}
		}
	}
	catch (const exception& e) {
		sendErrorResponse(client_socket, StatusCode::SERVER_ERROR, "Failed to read remote HEAD");
		return false;
	}

	// 判断是否需要更新
	bool has_updates = false;
	uint32_t commits_count = 0;

	if (remote_head != local_head) {
		has_updates = true;
		// 计算需要发送的提交数量
		// 简化处理：如果不同就发送整个历史
		commits_count = 1; // 简化为只发送一个提交

		if (!remote_head.empty()) {
			// 发送检查响应
			auto check_response = ProtocolMessage::createPullCheckResponse(remote_head, has_updates, commits_count);
			if (!NetworkUtils::sendMessage(client_socket, check_response)) {
				return false;
			}

			// 发送commit数据
			fs::path commit_obj_path = repo_path / MARKNAME / "objects" / remote_head;
			if (fs::exists(commit_obj_path)) {
				// 读取commit数据
				ifstream commit_file(commit_obj_path, ios::binary);
				if (commit_file.is_open()) {
					vector<uint8_t> commit_data((istreambuf_iterator<char>(commit_file)), {});
					commit_file.close();

					// 发送commit数据
					auto commit_msg = ProtocolMessage::createPullCommitData(remote_head, commit_data);
					if (!NetworkUtils::sendMessage(client_socket, commit_msg)) {
						return false;
					}

					// 发送该commit的所有对象
					// 简化处理：解析commit数据获取tree信息
					try {
						string commit_content = string(commit_data.begin(), commit_data.end());
						Commit commit = CommitManager::deserializeCommit(remote_head + "\n" + commit_content);

						// 发送所有tree中的对象
						for (const auto& tree_item : commit.tree) {
							const string& object_id = tree_item.second;
							fs::path obj_path = repo_path / MARKNAME / "objects" / object_id;
							if (fs::exists(obj_path)) {
								ifstream obj_file(obj_path, ios::binary);
								if (obj_file.is_open()) {
									vector<uint8_t> obj_data((istreambuf_iterator<char>(obj_file)), {});
									obj_file.close();

									auto obj_msg = ProtocolMessage::createPullObjectData(object_id, obj_data);
									if (!NetworkUtils::sendMessage(client_socket, obj_msg)) {
										return false;
									}
								}
							}
						}
					}
					catch (const exception& e) {
						// 如果解析commit失败，继续处理
					}

					// 发送拉取完成响应
					auto pull_response = ProtocolMessage::createStringMessage(MessageType::PULL_RESPONSE, "Pull completed");
					return NetworkUtils::sendMessage(client_socket, pull_response);
				}
			}
		}
	}

	// 无需更新
	auto response = ProtocolMessage::createPullCheckResponse(remote_head, has_updates, commits_count);
	return NetworkUtils::sendMessage(client_socket, response);
}

// 克隆请求处理
bool Server::handleCloneRequest(int client_socket, shared_ptr<ClientSession> session, const ProtocolMessage& msg) {
	if (!session->authenticated) {
		sendErrorResponse(client_socket, StatusCode::AUTH_REQUIRED, "Authentication required");
		return false;
	}

	string repo_name = msg.getStringPayload();
	if (repo_name.empty()) {
		sendErrorResponse(client_socket, StatusCode::INVALID_REQUEST, "Repository name required");
		return false;
	}

	if (!impl_->repo_manager->repositoryExists(repo_name)) {
		sendErrorResponse(client_socket, StatusCode::REPO_NOT_FOUND, "Repository not found");
		return false;
	}

	// 获取仓库路径
	fs::path repo_path = impl_->repo_manager->getRepositoryPath(repo_name);

	// 统计仓库文件
	vector<pair<string, fs::path>> files_to_clone;
	uint64_t total_size = 0;

	try {
		// 扫描仓库目录
		for (auto& entry : fs::recursive_directory_iterator(repo_path)) {
			if (entry.is_regular_file()) {
				string relative_path = fs::relative(entry.path(), repo_path).generic_string();
				files_to_clone.push_back({ relative_path, entry.path() });
				total_size += entry.file_size();
			}
		}

		// 发送克隆开始消息
		auto start_msg = ProtocolMessage::createCloneDataStart(repo_name,
			static_cast<uint32_t>(files_to_clone.size()),
			total_size);
		if (!NetworkUtils::sendMessage(client_socket, start_msg)) {
			return false;
		}

		// 传输每个文件
		for (const auto& file_info : files_to_clone) {
			if (!sendCloneFile(client_socket, file_info.first, file_info.second)) {
				sendErrorResponse(client_socket, StatusCode::SERVER_ERROR, "Failed to send file: " + file_info.first);
				return false;
			}
		}

		// 发送克隆结束消息
		auto end_msg = ProtocolMessage::createCloneDataEnd();
		if (!NetworkUtils::sendMessage(client_socket, end_msg)) {
			return false;
		}

		auto response = ProtocolMessage::createStringMessage(MessageType::CLONE_RESPONSE,
			"Clone completed: " + repo_name);
		return NetworkUtils::sendMessage(client_socket, response);

	}
	catch (const exception& e) {
		sendErrorResponse(client_socket, StatusCode::SERVER_ERROR,
			"Clone failed: " + string(e.what()));
		return false;
	}
}

// 处理日志请求
bool Server::handleLogRequest(int client_socket, shared_ptr<ClientSession> session, const ProtocolMessage& msg) {
	if (!session->authenticated) {
		sendErrorResponse(client_socket, StatusCode::AUTH_REQUIRED, "Authentication required");
		return false;
	}

	if (session->current_repo.empty()) {
		sendErrorResponse(client_socket, StatusCode::INVALID_REQUEST, "No repository selected");
		return false;
	}

	try {
		// 解析请求参数
		if (msg.payload.size() < sizeof(LogRequestPayload)) {
			sendErrorResponse(client_socket, StatusCode::INVALID_REQUEST, "Invalid log request payload");
			return false;
		}

		LogRequestPayload log_payload;
		memcpy(&log_payload, msg.payload.data(), sizeof(LogRequestPayload));

		int max_count = static_cast<int>(log_payload.max_count);
		bool line = log_payload.line != 0;

		// 切换到仓库目录
		fs::path repo_path = fs::path(config_.root_path) / session->current_repo;
		if (!fs::exists(repo_path / ".minigit")) {
			sendErrorResponse(client_socket, StatusCode::INVALID_REPO, "Repository not found");
			return false;
		}

		// 备份当前工作目录
		fs::path original_cwd = fs::current_path();
		fs::current_path(repo_path);

		vector<pair<string, string>> commits; // commit_id, message pairs

		// 获取当前HEAD
		fs::path head_path = ".minigit/HEAD";  // 相对路径，因为我们已经在repo目录了
		if (!fs::exists(head_path)) {
			// 没有提交历史
			fs::current_path(original_cwd);
			auto response = ProtocolMessage::createLogResponse(commits);
			return NetworkUtils::sendMessage(client_socket, response);
		}

		string current_id;
		ifstream head_file(head_path);
		if (head_file.is_open()) {
			getline(head_file, current_id);
			head_file.close();
		}

		if (current_id.empty()) {
			// 没有提交历史
			fs::current_path(original_cwd);
			auto response = ProtocolMessage::createLogResponse(commits);
			return NetworkUtils::sendMessage(client_socket, response);
		}

		// 遍历提交历史
		string commit_id = current_id;
		int count = 0;

		while (!commit_id.empty() && (max_count == -1 || count < max_count)) {
			// 使用CommitManager加载提交对象
			auto commit_opt = CommitManager::loadCommit(commit_id);
			if (!commit_opt) {
				break;
			}

			commits.push_back(make_pair(commit_id, commit_opt->message));
			commit_id = commit_opt->parent;
			count++;
		}

		// 恢复工作目录
		fs::current_path(original_cwd);

		// 发送响应
		auto response = ProtocolMessage::createLogResponse(commits);
		return NetworkUtils::sendMessage(client_socket, response);

	} catch (const exception& e) {
		sendErrorResponse(client_socket, StatusCode::SERVER_ERROR,
			"Log failed: " + string(e.what()));
		return false;
	}
}

// 心跳处理
bool Server::handleHeartbeat(int client_socket, shared_ptr<ClientSession> session) {
	auto response = ProtocolMessage(MessageType::HEARTBEAT);


	return 1;
}

// ServerCommand实现
int ServerCommand::parseAndRun(const vector<string>& args) {
	Server::Config config = parseConfig(args);

	if (config.port <= 0 || config.root_path.empty()) {
		printUsage();
		return 1;
	}

	if (config.password.empty() && config.cert_path.empty()) {
		cout << "Warning: No authentication configured. Server will run in insecure mode.\n";
	}

	Server server(config);
	return server.run();
}

Server::Config ServerCommand::parseConfig(const vector<string>& args) {
	Server::Config config;

	for (size_t i = 0; i < args.size(); ++i) {
		if (args[i] == "--port" && i + 1 < args.size()) {
			config.port = stoi(args[i + 1]);
			i++;
		}
		else if (args[i] == "--root" && i + 1 < args.size()) {
			config.root_path = args[i + 1];
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

void ServerCommand::printUsage() {
	cout << "Usage: minigit server --port <port> --root <path> [--password <password>] [--cert <cert_path>]\n";
	cout << "Options:\n";
	cout << "  --port <port>         Server port (required)\n";
	cout << "  --root <path>         Repository root path (required)\n";
	cout << "  --password <password> Password for authentication\n";
	cout << "  --cert <cert_path>    Path to RSA certificate directory\n";
}