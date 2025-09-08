#pragma once

#include "common.h"

/**
 * MiniGit二进制通信协议定义
 * 所有数据传输都使用小端字节序
 */

// 协议版本
const uint32_t PROTOCOL_VERSION = 1;

// 消息类型定义
enum class MessageType : uint8_t {
    // 认证相关
    AUTH_REQUEST = 0x01,        // 认证请求
    AUTH_RESPONSE = 0x02,       // 认证响应
    
    // 会话管理
    LOGIN_REQUEST = 0x10,       // 登录请求
    LOGIN_RESPONSE = 0x11,      // 登录响应
    LOGOUT_REQUEST = 0x12,      // 登出请求
    LOGOUT_RESPONSE = 0x13,     // 登出响应
    
    // 仓库管理
    LIST_REPOS_REQUEST = 0x20,  // 列出仓库请求
    LIST_REPOS_RESPONSE = 0x21, // 列出仓库响应
    USE_REPO_REQUEST = 0x22,    // 使用仓库请求
    USE_REPO_RESPONSE = 0x23,   // 使用仓库响应
    CREATE_REPO_REQUEST = 0x24, // 创建仓库请求
    CREATE_REPO_RESPONSE = 0x25,// 创建仓库响应
    REMOVE_REPO_REQUEST = 0x26, // 删除仓库请求
    REMOVE_REPO_RESPONSE = 0x27,// 删除仓库响应
    
    // Git操作
    PUSH_REQUEST = 0x30,        // 推送请求
    PUSH_RESPONSE = 0x31,       // 推送响应
    PULL_REQUEST = 0x32,        // 拉取请求
    PULL_RESPONSE = 0x33,       // 拉取响应
    CLONE_REQUEST = 0x34,       // 克隆请求
    CLONE_RESPONSE = 0x35,      // 克隆响应
    
    // 数据传输
    FILE_DATA = 0x40,           // 文件数据
    OBJECT_DATA = 0x41,         // Git对象数据
    CLONE_DATA_START = 0x42,    // 克隆数据开始
    CLONE_DATA_END = 0x43,      // 克隆数据结束
    CLONE_FILE = 0x44,          // 克隆文件
    
    // 控制消息
    HEARTBEAT = 0x50,           // 心跳
    ERROR_MSG = 0xFF            // 错误消息
};

// 状态码定义
enum class StatusCode : uint8_t {
    SUCCESS = 0x00,             // 成功
    AUTH_REQUIRED = 0x01,       // 需要认证
    AUTH_FAILED = 0x02,         // 认证失败
    INVALID_REPO = 0x03,        // 无效仓库
    REPO_EXISTS = 0x04,         // 仓库已存在
    REPO_NOT_FOUND = 0x05,      // 仓库不存在
    PERMISSION_DENIED = 0x06,   // 权限不足
    INVALID_REQUEST = 0x07,     // 无效请求
    SERVER_ERROR = 0x08,        // 服务器错误
    PROTOCOL_ERROR = 0x09,      // 协议错误
    CONNECTION_LOST = 0x0A      // 连接丢失
};

// 消息头结构（固定16字节）
#pragma pack(push, 1)
struct MessageHeader {
    uint32_t magic;             // 魔法数字 0x4D474954 ("MGIT")
    uint32_t version;           // 协议版本
    MessageType type;           // 消息类型
    uint8_t flags;              // 标志位（保留）
    uint16_t reserved;          // 保留字段
    uint32_t payload_size;      // 负载大小
    
    MessageHeader() : magic(0x4D474954), version(PROTOCOL_VERSION), 
                     type(MessageType::HEARTBEAT), flags(0), reserved(0), payload_size(0) {}
};
#pragma pack(pop)

// 认证请求负载
#pragma pack(push, 1)
struct AuthRequestPayload {
    uint8_t auth_type;          // 认证类型：0=密码，1=RSA证书
    uint32_t data_size;         // 认证数据大小
    // 接下来是认证数据：密码或证书数据
};
#pragma pack(pop)

// 认证响应负载
#pragma pack(push, 1)
struct AuthResponsePayload {
    StatusCode status;          // 认证状态
    uint8_t session_id[32];     // 会话ID（SHA256哈希）
    uint32_t session_timeout;   // 会话超时时间（秒）
};
#pragma pack(pop)

// 字符串消息负载（用于仓库名等）
#pragma pack(push, 1)
struct StringMessagePayload {
    uint32_t string_length;     // 字符串长度
    // 接下来是字符串数据
};
#pragma pack(pop)

// 文件传输负载
#pragma pack(push, 1)
struct FileTransferPayload {
    uint32_t filename_length;   // 文件名长度
    uint64_t file_size;         // 文件大小
    uint32_t checksum;          // 文件校验和（CRC32）
    // 接下来是文件名，然后是文件数据
};
#pragma pack(pop)

// 克隆数据起始负载
#pragma pack(push, 1)
struct CloneDataStartPayload {
    uint32_t total_files;       // 总文件数量
    uint64_t total_size;        // 总数据大小
    uint32_t repo_name_length;  // 仓库名长度
    // 接下来是仓库名
};
#pragma pack(pop)

// 克隆文件负载
#pragma pack(push, 1)
struct CloneFilePayload {
    uint32_t file_path_length;  // 文件路径长度
    uint64_t file_size;         // 文件大小
    uint32_t checksum;          // 文件校验和
    uint8_t file_type;          // 文件类型: 0=文件, 1=目录
    // 接下来是文件路径，然后是文件数据
};
#pragma pack(pop)

// 仓库列表响应项
#pragma pack(push, 1)
struct RepoListItem {
    uint32_t name_length;       // 仓库名长度
    uint64_t last_modified;     // 最后修改时间（Unix时间戳）
    uint32_t commit_count;      // 提交数量
    // 接下来是仓库名字符串
};
#pragma pack(pop)

/**
 * 协议消息类
 * 封装消息的序列化和反序列化
 */
class ProtocolMessage {
public:
    MessageHeader header;
    vector<uint8_t> payload;
    
    ProtocolMessage() = default;
    ProtocolMessage(MessageType type, const vector<uint8_t>& data = {});
    
    // 序列化为字节流
    vector<uint8_t> serialize() const;
    
    // 从字节流反序列化
    static bool deserialize(const vector<uint8_t>& data, ProtocolMessage& msg);
    
    // 验证消息头
    bool validateHeader() const;
    
    // 设置负载数据
    void setPayload(const vector<uint8_t>& data);
    void setStringPayload(const string& str);
    
    // 获取负载数据
    string getStringPayload() const;
    
    // 创建特定类型的消息
    static ProtocolMessage createAuthRequest(bool use_rsa, const vector<uint8_t>& auth_data);
    static ProtocolMessage createAuthResponse(StatusCode status, const string& session_id, uint32_t timeout);
    static ProtocolMessage createStringMessage(MessageType type, const string& str);
    static ProtocolMessage createErrorMessage(StatusCode status, const string& error_msg);
    static ProtocolMessage createRepoListResponse(const vector<pair<string, RepoListItem>>& repos);
    static ProtocolMessage createCloneDataStart(const string& repo_name, uint32_t total_files, uint64_t total_size);
    static ProtocolMessage createCloneFile(const string& file_path, const vector<uint8_t>& file_data, uint8_t file_type);
    static ProtocolMessage createCloneDataEnd();
    
    // 工具方法
    static uint32_t calculateCRC32(const vector<uint8_t>& data);
};

/**
 * 网络传输工具类
 */
class NetworkUtils {
public:
    // 发送完整消息
    static bool sendMessage(int socket, const ProtocolMessage& msg);
    
    // 接收完整消息
    static bool receiveMessage(int socket, ProtocolMessage& msg);
    
    // 发送原始数据
    static bool sendData(int socket, const void* data, size_t size);
    
    // 接收原始数据
    static bool receiveData(int socket, void* buffer, size_t size);
    
    // 设置socket超时
    static bool setSocketTimeout(int socket, int timeout_seconds);
    
    // 检查socket状态
    static bool isSocketConnected(int socket);
    
private:
    static const int MAX_RETRY_COUNT = 3;
    static const int CHUNK_SIZE = 65536; // 64KB
};