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
#include <mutex>
#include <memory>

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
#include <fcntl.h>
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
			thread([this, client_socket]() {
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

	if (server_socket_ != INVALID_SOCKET) {
#ifdef _WIN32
		closesocket(server_socket_);
#else
		close(server_socket_);
#endif
		server_socket_ = INVALID_SOCKET;
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
	if (server_socket_ == INVALID_SOCKET) {
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

	case MessageType::PULL_REQUEST:
		return handlePullRequest(client_socket, session, msg);

	case MessageType::CLONE_REQUEST:
		return handleCloneRequest(client_socket, session, msg);

	case MessageType::HEARTBEAT:
		return handleHeartbeat(client_socket, session);

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

// 推送请求处理
bool Server::handlePushRequest(int client_socket, shared_ptr<ClientSession> session, const ProtocolMessage& msg) {
	if (!session->authenticated) {
		sendErrorResponse(client_socket, StatusCode::AUTH_REQUIRED, "Authentication required");
		return false;
	}

	if (session->current_repo.empty()) {
		sendErrorResponse(client_socket, StatusCode::INVALID_REQUEST, "No repository selected");
		return false;
	}

	// TODO: 实现推送逻辑
	auto response = ProtocolMessage::createStringMessage(MessageType::PUSH_RESPONSE, "Push completed");
	return NetworkUtils::sendMessage(client_socket, response);
}

// 拉取请求处理
bool Server::handlePullRequest(int client_socket, shared_ptr<ClientSession> session, const ProtocolMessage& msg) {
	if (!session->authenticated) {
		sendErrorResponse(client_socket, StatusCode::AUTH_REQUIRED, "Authentication required");
		return false;
	}

	if (session->current_repo.empty()) {
		sendErrorResponse(client_socket, StatusCode::INVALID_REQUEST, "No repository selected");
		return false;
	}

	// TODO: 实现拉取逻辑
	auto response = ProtocolMessage::createStringMessage(MessageType::PULL_RESPONSE, "Pull completed");
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

// 心跳处理
bool Server::handleHeartbeat(int client_socket, shared_ptr<ClientSession> session) {
	auto response = ProtocolMessage(MessageType::HEARTBEAT);
	return NetworkUtils::sendMessage(client_socket, response);
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