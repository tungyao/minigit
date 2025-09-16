#include "protocol.h"

#include <cstring>

#include "sha256.h"

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
#include <cerrno>
#include <sys/time.h>
#endif
#include <client.h>
#include <crypto.h>

// 构造函数
ProtocolMessage::ProtocolMessage(MessageType type, const vector<uint8_t>& data) {
	header.type = type;
	setPayload(data);
}

// 序列化为字节流
vector<uint8_t> ProtocolMessage::serialize() const {
	vector<uint8_t> result;

	// 序列化消息头
	const uint8_t* header_ptr = reinterpret_cast<const uint8_t*>(&header);
	result.insert(result.end(), header_ptr, header_ptr + sizeof(MessageHeader));

	// 序列化负载
	result.insert(result.end(), payload.begin(), payload.end());

	return result;
}

// 从字节流反序列化
bool ProtocolMessage::deserialize(const vector<uint8_t>& data, ProtocolMessage& msg) {
	if (data.size() < sizeof(MessageHeader)) {
		return false;
	}

	// 反序列化消息头
	memcpy(&msg.header, data.data(), sizeof(MessageHeader));

	// 验证消息头
	if (!msg.validateHeader()) {
		return false;
	}

	// 检查负载大小
	if (data.size() < sizeof(MessageHeader) + msg.header.payload_size) {
		return false;
	}

	// 反序列化负载
	msg.payload.clear();
	if (msg.header.payload_size > 0) {
		msg.payload.insert(msg.payload.end(),
			data.begin() + sizeof(MessageHeader),
			data.begin() + sizeof(MessageHeader) + msg.header.payload_size);
	}
	Crypto cp;
	vector<uint8_t> enc_data = cp.decryptAES(msg.payload);
	msg.header.payload_size = enc_data.size();
	msg.payload = enc_data;
	return true;
}

// 验证消息头
bool ProtocolMessage::validateHeader() const {
	return header.magic == 0x4D474954 && header.version == PROTOCOL_VERSION;
}

// 设置负载数据
void ProtocolMessage::setPayload(const vector<uint8_t>& data) {
	Crypto cp;
	vector<uint8_t> enc_data = cp.encryptAES(data);
	payload = enc_data;
	header.payload_size = static_cast<uint32_t>(enc_data.size());
}

void ProtocolMessage::setStringPayload(const string& str) {
	StringMessagePayload string_payload;
	string_payload.string_length = static_cast<uint32_t>(str.size());

	vector<uint8_t> data;
	data.resize(sizeof(StringMessagePayload) + str.size());

	memcpy(data.data(), &string_payload, sizeof(StringMessagePayload));
	memcpy(data.data() + sizeof(StringMessagePayload), str.c_str(), str.size());

	setPayload(data);
}

// 获取负载数据
string ProtocolMessage::getStringPayload() const {
	if (payload.size() < sizeof(StringMessagePayload)) {
		return "";
	}

	StringMessagePayload string_payload;
	memcpy(&string_payload, payload.data(), sizeof(StringMessagePayload));

	if (payload.size() < sizeof(StringMessagePayload) + string_payload.string_length) {
		return "";
	}

	return string(reinterpret_cast<const char*>(payload.data() + sizeof(StringMessagePayload)),
		string_payload.string_length);
}

// 创建认证请求消息
ProtocolMessage ProtocolMessage::createAuthRequest(bool use_rsa, const vector<uint8_t>& auth_data) {
	AuthRequestPayload auth_payload;
	auth_payload.auth_type = use_rsa ? 1 : 0;
	auth_payload.data_size = static_cast<uint32_t>(auth_data.size());

	vector<uint8_t> data;
	data.resize(sizeof(AuthRequestPayload) + auth_data.size());

	memcpy(data.data(), &auth_payload, sizeof(AuthRequestPayload));
	memcpy(data.data() + sizeof(AuthRequestPayload), auth_data.data(), auth_data.size());

	return ProtocolMessage(MessageType::AUTH_REQUEST, data);
}

// 创建认证响应消息
ProtocolMessage ProtocolMessage::createAuthResponse(StatusCode status, const string& session_id, uint32_t timeout) {
	AuthResponsePayload auth_response;
	auth_response.status = status;
	auth_response.session_timeout = timeout;

	// 计算会话ID的SHA256哈希
	string hash = sha256_string(session_id);
	memset(auth_response.session_id, 0, sizeof(auth_response.session_id));
	memcpy(auth_response.session_id, hash.c_str(), min(hash.size(), sizeof(auth_response.session_id)));

	vector<uint8_t> data(sizeof(AuthResponsePayload));
	memcpy(data.data(), &auth_response, sizeof(AuthResponsePayload));

	return ProtocolMessage(MessageType::AUTH_RESPONSE, data);
}

// 创建字符串消息
ProtocolMessage ProtocolMessage::createStringMessage(MessageType type, const string& str) {
	ProtocolMessage msg(type);
	msg.setStringPayload(str);
	return msg;
}

// 创建错误消息
ProtocolMessage ProtocolMessage::createErrorMessage(StatusCode status, const string& error_msg) {
	vector<uint8_t> data;
	data.push_back(static_cast<uint8_t>(status));
	data.insert(data.end(), error_msg.begin(), error_msg.end());

	return ProtocolMessage(MessageType::ERROR_MSG, data);
}

// 创建仓库列表响应
ProtocolMessage ProtocolMessage::createRepoListResponse(const vector<pair<string, RepoListItem>>& repos) {
	vector<uint8_t> data;

	// 写入仓库数量
	uint32_t repo_count = static_cast<uint32_t>(repos.size());
	data.insert(data.end(), reinterpret_cast<const uint8_t*>(&repo_count),
		reinterpret_cast<const uint8_t*>(&repo_count) + sizeof(uint32_t));

	// 写入每个仓库的信息
	for (const auto& repo : repos) {
		// 写入RepoListItem结构
		data.insert(data.end(), reinterpret_cast<const uint8_t*>(&repo.second),
			reinterpret_cast<const uint8_t*>(&repo.second) + sizeof(RepoListItem));

		// 写入仓库名
		data.insert(data.end(), repo.first.begin(), repo.first.end());
	}

	return ProtocolMessage(MessageType::LIST_REPOS_RESPONSE, data);
}

// 创建克隆数据开始消息
ProtocolMessage ProtocolMessage::createCloneDataStart(const string& repo_name, uint32_t total_files, uint64_t total_size) {
	CloneDataStartPayload payload;
	payload.total_files = total_files;
	payload.total_size = total_size;
	payload.repo_name_length = static_cast<uint32_t>(repo_name.size());

	vector<uint8_t> data;
	data.resize(sizeof(CloneDataStartPayload) + repo_name.size());

	memcpy(data.data(), &payload, sizeof(CloneDataStartPayload));
	memcpy(data.data() + sizeof(CloneDataStartPayload), repo_name.c_str(), repo_name.size());

	return ProtocolMessage(MessageType::CLONE_DATA_START, data);
}

// 创建克隆文件消息
ProtocolMessage ProtocolMessage::createCloneFile(const string& file_path, const vector<uint8_t>& file_data, uint8_t file_type) {
	CloneFilePayload payload;
	payload.file_path_length = static_cast<uint32_t>(file_path.size());
	payload.file_size = static_cast<uint64_t>(file_data.size());
	payload.checksum = calculateCRC32(file_data);
	payload.file_type = file_type;

	vector<uint8_t> data;
	data.resize(sizeof(CloneFilePayload) + file_path.size() + file_data.size());

	size_t offset = 0;
	memcpy(data.data() + offset, &payload, sizeof(CloneFilePayload));
	offset += sizeof(CloneFilePayload);

	memcpy(data.data() + offset, file_path.c_str(), file_path.size());
	offset += file_path.size();

	memcpy(data.data() + offset, file_data.data(), file_data.size());

	return ProtocolMessage(MessageType::CLONE_FILE, data);
}

// 创建克隆数据结束消息
ProtocolMessage ProtocolMessage::createCloneDataEnd() {
	return ProtocolMessage(MessageType::CLONE_DATA_END);
}

// 创建推送检查请求消息
ProtocolMessage ProtocolMessage::createPushCheckRequest(const string& local_head, const string& new_commit_id, const string& commit_parent) {
	PushCheckRequestPayload payload;
	payload.local_head_length = static_cast<uint32_t>(local_head.size());
	payload.new_commit_id_length = static_cast<uint32_t>(new_commit_id.size());
	payload.commit_parent_length = static_cast<uint32_t>(commit_parent.size());

	vector<uint8_t> data;
	data.resize(sizeof(PushCheckRequestPayload) + local_head.size() + new_commit_id.size() + commit_parent.size());

	size_t offset = 0;
	memcpy(data.data() + offset, &payload, sizeof(PushCheckRequestPayload));
	offset += sizeof(PushCheckRequestPayload);

	memcpy(data.data() + offset, local_head.c_str(), local_head.size());
	offset += local_head.size();

	memcpy(data.data() + offset, new_commit_id.c_str(), new_commit_id.size());
	offset += new_commit_id.size();

	memcpy(data.data() + offset, commit_parent.c_str(), commit_parent.size());

	return ProtocolMessage(MessageType::PUSH_CHECK_REQUEST, data);
}

// 创建推送检查响应消息
ProtocolMessage ProtocolMessage::createPushCheckResponse(const string& remote_head, bool needs_update) {
	PushCheckResponsePayload payload;
	payload.remote_head_length = static_cast<uint32_t>(remote_head.size());
	payload.needs_update = needs_update ? 1 : 0;

	vector<uint8_t> data;
	data.resize(sizeof(PushCheckResponsePayload) + remote_head.size());

	memcpy(data.data(), &payload, sizeof(PushCheckResponsePayload));
	memcpy(data.data() + sizeof(PushCheckResponsePayload), remote_head.c_str(), remote_head.size());

	return ProtocolMessage(MessageType::PUSH_CHECK_RESPONSE, data);
}

// 创建推送提交数据消息
ProtocolMessage ProtocolMessage::createPushCommitData(const string& commit_id, const vector<uint8_t>& commit_data) {
	PushCommitDataPayload payload;
	payload.commit_id_length = static_cast<uint32_t>(commit_id.size());
	payload.commit_data_length = static_cast<uint32_t>(commit_data.size());

	vector<uint8_t> data;
	data.resize(sizeof(PushCommitDataPayload) + commit_id.size() + commit_data.size());

	size_t offset = 0;
	memcpy(data.data() + offset, &payload, sizeof(PushCommitDataPayload));
	offset += sizeof(PushCommitDataPayload);

	memcpy(data.data() + offset, commit_id.c_str(), commit_id.size());
	offset += commit_id.size();

	memcpy(data.data() + offset, commit_data.data(), commit_data.size());

	return ProtocolMessage(MessageType::PUSH_COMMIT_DATA, data);
}

// 创建推送对象数据消息
ProtocolMessage ProtocolMessage::createPushObjectData(const string& object_id, const vector<uint8_t>& object_data) {
	PushObjectDataPayload payload;
	payload.object_id_length = static_cast<uint32_t>(object_id.size());
	payload.object_data_length = static_cast<uint32_t>(object_data.size());
	payload.checksum = calculateCRC32(object_data);

	vector<uint8_t> data;
	data.resize(sizeof(PushObjectDataPayload) + object_id.size() + object_data.size());

	size_t offset = 0;
	memcpy(data.data() + offset, &payload, sizeof(PushObjectDataPayload));
	offset += sizeof(PushObjectDataPayload);

	memcpy(data.data() + offset, object_id.c_str(), object_id.size());
	offset += object_id.size();

	memcpy(data.data() + offset, object_data.data(), object_data.size());

	return ProtocolMessage(MessageType::PUSH_OBJECT_DATA, data);
}
// 创建推送压缩对象数据消息
ProtocolMessage ProtocolMessage::createPushObjectDataCompressed(MessageType type,
	uint32_t operation_type, const vector<uint8_t>& compressed_data,
	uint64_t original_size, uint32_t file_count) {
	PushObjectDataPayloadCompressed payload;
	payload.operation_type = operation_type;
	payload.original_size = original_size;
	payload.compressed_size = static_cast<uint64_t>(compressed_data.size());
	payload.checksum = calculateCRC32(compressed_data); // 对压缩数据计算校验和
	payload.file_count = file_count;


	// 组装完整数据
	vector<uint8_t> data;
	data.resize(sizeof(PushObjectDataPayloadCompressed) + compressed_data.size());

	// 复制头部
	memcpy(data.data(), &payload, sizeof(PushObjectDataPayloadCompressed));

	// 复制压缩数据
	memcpy(data.data() + sizeof(PushObjectDataPayloadCompressed),
		compressed_data.data(), compressed_data.size());

	return ProtocolMessage(type, data);
}
// 创建推送请求处理 更新远程HEAD
ProtocolMessage ProtocolMessage::createPushRequest(const string& remote_head) {
	PushRequestPayload payload;
	payload.remote_head_length = static_cast<uint32_t>(remote_head.size());

	vector<uint8_t> data;
	data.resize(sizeof(PushRequestPayload) + remote_head.size());

	memcpy(data.data(), &payload, sizeof(PushRequestPayload));
	memcpy(data.data() + sizeof(PushRequestPayload), remote_head.c_str(), remote_head.size());

	return ProtocolMessage(MessageType::PUSH_REQUEST, data);
}

// 创建拉取检查请求消息
ProtocolMessage ProtocolMessage::createPullCheckRequest(const string& local_head) {
	PullCheckRequestPayload payload;
	payload.local_head_length = static_cast<uint32_t>(local_head.size());

	vector<uint8_t> data;
	data.resize(sizeof(PullCheckRequestPayload) + local_head.size());

	memcpy(data.data(), &payload, sizeof(PullCheckRequestPayload));
	memcpy(data.data() + sizeof(PullCheckRequestPayload), local_head.c_str(), local_head.size());

	return ProtocolMessage(MessageType::PULL_CHECK_REQUEST, data);
}

// 创建拉取检查响应消息
ProtocolMessage ProtocolMessage::createPullCheckResponse(const string& remote_head, bool has_updates, uint32_t commits_count) {
	PullCheckResponsePayload payload;
	payload.remote_head_length = static_cast<uint32_t>(remote_head.size());
	payload.has_updates = has_updates ? 1 : 0;
	payload.commits_count = commits_count;

	vector<uint8_t> data;
	data.resize(sizeof(PullCheckResponsePayload) + remote_head.size());

	memcpy(data.data(), &payload, sizeof(PullCheckResponsePayload));
	memcpy(data.data() + sizeof(PullCheckResponsePayload), remote_head.c_str(), remote_head.size());

	return ProtocolMessage(MessageType::PULL_CHECK_RESPONSE, data);
}

// 创建拉取提交数据消息
ProtocolMessage ProtocolMessage::createPullCommitData(const string& commit_id, const vector<uint8_t>& commit_data) {
	PullCommitDataPayload payload;
	payload.commit_id_length = static_cast<uint32_t>(commit_id.size());
	payload.commit_data_length = static_cast<uint32_t>(commit_data.size());

	vector<uint8_t> data;
	data.resize(sizeof(PullCommitDataPayload) + commit_id.size() + commit_data.size());

	size_t offset = 0;
	memcpy(data.data() + offset, &payload, sizeof(PullCommitDataPayload));
	offset += sizeof(PullCommitDataPayload);

	memcpy(data.data() + offset, commit_id.c_str(), commit_id.size());
	offset += commit_id.size();

	memcpy(data.data() + offset, commit_data.data(), commit_data.size());

	return ProtocolMessage(MessageType::PULL_COMMIT_DATA, data);
}

// 创建拉取对象数据消息
ProtocolMessage ProtocolMessage::createPullObjectData(const string& object_id, const vector<uint8_t>& object_data) {
	PullObjectDataPayload payload;
	payload.object_id_length = static_cast<uint32_t>(object_id.size());
	payload.object_data_length = static_cast<uint32_t>(object_data.size());
	payload.checksum = calculateCRC32(object_data);

	vector<uint8_t> data;
	data.resize(sizeof(PullObjectDataPayload) + object_id.size() + object_data.size());

	size_t offset = 0;
	memcpy(data.data() + offset, &payload, sizeof(PullObjectDataPayload));
	offset += sizeof(PullObjectDataPayload);

	memcpy(data.data() + offset, object_id.c_str(), object_id.size());
	offset += object_id.size();

	memcpy(data.data() + offset, object_data.data(), object_data.size());

	return ProtocolMessage(MessageType::PULL_OBJECT_DATA, data);
}

// 创建拉取压缩对象数据消息
ProtocolMessage ProtocolMessage::createPullObjectDataCompressed(uint32_t operation_type, const vector<uint8_t>& compressed_data,
	uint64_t original_size, uint32_t file_count) {
	PullObjectDataPayloadCompressed payload;
	payload.operation_type = operation_type;
	payload.original_size = original_size;
	payload.compressed_size = static_cast<uint64_t>(compressed_data.size());
	payload.checksum = calculateCRC32(compressed_data);
	payload.file_count = file_count;

	// 组装完整数据
	vector<uint8_t> data;
	data.resize(sizeof(PullObjectDataPayloadCompressed) + compressed_data.size());

	// 复制头部
	memcpy(data.data(), &payload, sizeof(PullObjectDataPayloadCompressed));

	// 复制压缩数据
	memcpy(data.data() + sizeof(PullObjectDataPayloadCompressed),
		compressed_data.data(), compressed_data.size());

	return ProtocolMessage(MessageType::PULL_OBJECT_DATA_COMPRESSED, data);
}

// 创建克隆压缩数据消息
ProtocolMessage ProtocolMessage::createCloneDataCompressed(uint32_t operation_type, const vector<uint8_t>& compressed_data,
	uint64_t original_size, uint32_t file_count) {
	CloneDataCompressedPayload payload;
	payload.operation_type = operation_type;
	payload.original_size = original_size;
	payload.compressed_size = static_cast<uint64_t>(compressed_data.size());
	payload.checksum = calculateCRC32(compressed_data);
	payload.file_count = file_count;

	// 组装完整数据
	vector<uint8_t> data;
	data.resize(sizeof(CloneDataCompressedPayload) + compressed_data.size());

	// 复制头部
	memcpy(data.data(), &payload, sizeof(CloneDataCompressedPayload));

	// 复制压缩数据
	memcpy(data.data() + sizeof(CloneDataCompressedPayload),
		compressed_data.data(), compressed_data.size());

	return ProtocolMessage(MessageType::CLONE_DATA_COMPRESSED, data);
}

// 创建日志请求消息
ProtocolMessage ProtocolMessage::createLogRequest(int max_count, bool line) {
	LogRequestPayload payload;
	payload.max_count = static_cast<uint32_t>(max_count);
	payload.line = line ? 1 : 0;

	vector<uint8_t> data(sizeof(LogRequestPayload));
	memcpy(data.data(), &payload, sizeof(LogRequestPayload));

	return ProtocolMessage(MessageType::LOG_REQUEST, data);
}

// 创建日志响应消息
ProtocolMessage ProtocolMessage::createLogResponse(const vector<pair<string, string>>& commits) {
	// 计算总数据大小
	size_t total_size = sizeof(LogResponsePayload);
	for (const auto& commit : commits) {
		total_size += sizeof(uint32_t) + commit.first.size();  // commit_id_length + commit_id
		total_size += sizeof(uint32_t) + commit.second.size(); // message_length + message
	}

	vector<uint8_t> data;
	data.reserve(total_size);

	// 写入响应头
	LogResponsePayload payload;
	payload.commits_count = static_cast<uint32_t>(commits.size());

	data.resize(sizeof(LogResponsePayload));
	memcpy(data.data(), &payload, sizeof(LogResponsePayload));

	// 写入每个提交的数据
	for (const auto& commit : commits) {
		// 写入commit_id_length和commit_id
		uint32_t commit_id_length = static_cast<uint32_t>(commit.first.size());
		size_t old_size = data.size();
		data.resize(old_size + sizeof(uint32_t) + commit.first.size());
		memcpy(data.data() + old_size, &commit_id_length, sizeof(uint32_t));
		memcpy(data.data() + old_size + sizeof(uint32_t), commit.first.c_str(), commit.first.size());

		// 写入message_length和message
		uint32_t message_length = static_cast<uint32_t>(commit.second.size());
		old_size = data.size();
		data.resize(old_size + sizeof(uint32_t) + commit.second.size());
		memcpy(data.data() + old_size, &message_length, sizeof(uint32_t));
		memcpy(data.data() + old_size + sizeof(uint32_t), commit.second.c_str(), commit.second.size());
	}

	return ProtocolMessage(MessageType::LOG_RESPONSE, data);
}

// CRC32计算（简单实现）
uint32_t ProtocolMessage::calculateCRC32(const vector<uint8_t>& data) {
	uint32_t crc = 0xFFFFFFFF;

	for (uint8_t byte : data) {
		crc ^= byte;
		for (int i = 0; i < 8; i++) {
			if (crc & 1) {
				crc = (crc >> 1) ^ 0xEDB88320;
			}
			else {
				crc >>= 1;
			}
		}
	}

	return ~crc;
}

// NetworkUtils实现

// 发送完整消息
bool NetworkUtils::sendMessage(int socket, const ProtocolMessage& msg) {
	vector<uint8_t> data = msg.serialize();
#ifdef DEBUG
	cout << "send msg size " << data.size() << endl;
#endif // DEBUG
	return sendData(socket, data.data(), data.size());
}

// 接收完整消息
bool NetworkUtils::receiveMessage(int socket, ProtocolMessage& msg) {
	// 首先接收消息头
	MessageHeader header;
	if (!receiveData(socket, &header, sizeof(MessageHeader))) {
		return false;
	}
	while (header.type == MessageType::HEARTBEAT) {
		if (!receiveData(socket, &header, sizeof(MessageHeader))) {
			return false;
		}
	}
	// 验证消息头
	if (header.magic != 0x4D474954 || header.version != PROTOCOL_VERSION) {
		return false;
	}

	// 接收负载数据
	vector<uint8_t> payload(header.payload_size);
	if (header.payload_size > 0) {
		if (!receiveData(socket, payload.data(), header.payload_size)) {
			return false;
		}
	}

	// 构造完整消息
	vector<uint8_t> full_data;
	full_data.insert(full_data.end(), reinterpret_cast<const uint8_t*>(&header),
		reinterpret_cast<const uint8_t*>(&header) + sizeof(MessageHeader));
	full_data.insert(full_data.end(), payload.begin(), payload.end());
	return ProtocolMessage::deserialize(full_data, msg);
}

// 发送原始数据
bool NetworkUtils::sendData(int socket, const void* data, size_t size) {
	const char* ptr = static_cast<const char*>(data);
	size_t remaining = size;
	int retry_count = 0;

	while (remaining > 0) {
		size_t chunk_size = min(remaining, static_cast<size_t>(CHUNK_SIZE));

#ifdef _WIN32
		int sent = send(socket, ptr, static_cast<int>(chunk_size), 0);
		if (sent == SOCKET_ERROR) {
			int error = WSAGetLastError();
			if (error == WSAEWOULDBLOCK && retry_count < MAX_RETRY_COUNT) {
				Sleep(10);
				retry_count++;
				continue;
			}
			return false;
		}
#else
		ssize_t sent = send(socket, ptr, chunk_size, MSG_NOSIGNAL);
		if (sent < 0) {
			if ((errno == EAGAIN || errno == EWOULDBLOCK) && retry_count < MAX_RETRY_COUNT) {
				usleep(10000);
				retry_count++;
				continue;
			}
			return false;
		}
#endif

		ptr += sent;
		remaining -= sent;
		retry_count = 0;
	}

	return true;
}

// 接收原始数据
bool NetworkUtils::receiveData(int socket, void* buffer, size_t size) {
	char* ptr = static_cast<char*>(buffer);
	size_t remaining = size;
	int retry_count = 0;

	while (remaining > 0) {
		size_t chunk_size = min(remaining, static_cast<size_t>(CHUNK_SIZE));
		int get_size = static_cast<int>(chunk_size);
#ifdef DEBUG
		cout << "ready to get size " << get_size << endl;
#endif // DEBUG
#ifdef _WIN32
		int received = recv(socket, ptr, get_size, 0);
		if (received == SOCKET_ERROR) {
			int error = WSAGetLastError();
			if (error == WSAEWOULDBLOCK && retry_count < MAX_RETRY_COUNT) {
				Sleep(10);
				retry_count++;
				continue;
			}
			return false;
		}
		if (received == 0) {
			return false; // 连接关闭
		}
#else
		ssize_t received = recv(socket, ptr, chunk_size, 0);
		if (received < 0) {
			if ((errno == EAGAIN || errno == EWOULDBLOCK) && retry_count < MAX_RETRY_COUNT) {
				usleep(10000);
				retry_count++;
				continue;
			}
			return false;
		}
		if (received == 0) {
			return false; // 连接关闭
		}
#endif

		ptr += received;
		remaining -= received;
		retry_count = 0;
	}

	return true;
}

// 设置socket超时
bool NetworkUtils::setSocketTimeout(int socket, int timeout_seconds) {
#ifdef _WIN32
	DWORD timeout = timeout_seconds * 1000;
	return setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout)) == 0 &&
		setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout)) == 0;
#else
	struct timeval timeout;
	timeout.tv_sec = timeout_seconds;
	timeout.tv_usec = 0;
	return setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == 0 &&
		setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) == 0;
#endif
}

// 检查socket连接状态
bool NetworkUtils::isSocketConnected(int socket) {
#ifdef _WIN32
	//int result = recv(socket, buffer, 1, MSG_PEEK);
	//auto heartbeat = ProtocolMessage(MessageType::HEARTBEAT);
	//return NetworkUtils::sendMessage(socket, heartbeat);;
	return 1;
#else
	char buffer[1];
	ssize_t result = recv(socket, buffer, 1, MSG_PEEK | MSG_DONTWAIT);
	return result >= 0 || (errno == EAGAIN || errno == EWOULDBLOCK);
#endif
}