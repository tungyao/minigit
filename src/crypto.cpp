#include "crypto.h"
#include "sha256.h"
#include <random>
#include <cstring>

#ifdef HAVE_OPENSSL
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/aes.h>
#include <openssl/err.h>
#endif

// 从密码生成对称密钥
Crypto::SymmetricKey Crypto::generateKeyFromPassword(const string& password) {
    SymmetricKey key;
    
    // 使用密码的哈希作为固定盐（确保客户端和服务器生成相同的密钥）
    string salt_str = sha256Hash(password + "salt");
    vector<uint8_t> salt = stringToBytes(salt_str.substr(0, 16)); // 取前16字节作为盐
    
    // 使用PBKDF2生成密钥和IV
    key.key = pbkdf2(password, salt, 10000, 32);  // AES-256需要32字节密钥
    key.iv = pbkdf2(password + "iv", salt, 1000, 16);  // AES需要16字节IV
    
    return key;
}

// AES-256-CBC加密
vector<uint8_t> Crypto::encryptAES(const vector<uint8_t>& data, const SymmetricKey& key) {
#ifdef HAVE_OPENSSL
    // 使用OpenSSL进行真正的AES-256-CBC加密
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return vector<uint8_t>();
    
    // 初始化加密操作
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key.key.data(), key.iv.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return vector<uint8_t>();
    }
    
    // 计算输出缓冲区大小（需要考虑填充）
    vector<uint8_t> result;
    result.reserve(data.size() + AES_BLOCK_SIZE + 16); // 预留IV空间
    
    // 添加IV到结果开头
    result.insert(result.end(), key.iv.begin(), key.iv.end());
    
    // 加密数据
    int len;
    int ciphertext_len;
    vector<uint8_t> ciphertext(data.size() + AES_BLOCK_SIZE);
    
    if (EVP_EncryptUpdate(ctx, ciphertext.data(), &len, data.data(), data.size()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return vector<uint8_t>();
    }
    ciphertext_len = len;
    
    // 完成加密（处理填充）
    if (EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return vector<uint8_t>();
    }
    ciphertext_len += len;
    
    // 添加加密后的数据
    result.insert(result.end(), ciphertext.begin(), ciphertext.begin() + ciphertext_len);
    
    EVP_CIPHER_CTX_free(ctx);
    return result;
#else
    // 回退到简化的XOR加密实现
    vector<uint8_t> result;
    result.reserve(data.size() + 16);
    
    // 添加IV到结果开头
    result.insert(result.end(), key.iv.begin(), key.iv.end());
    
    // 简化的加密：使用密钥与数据进行XOR
    for (size_t i = 0; i < data.size(); ++i) {
        uint8_t encrypted_byte = data[i] ^ key.key[i % key.key.size()] ^ key.iv[i % key.iv.size()];
        result.push_back(encrypted_byte);
    }
    
    return result;
#endif
}

// AES-256-CBC解密
vector<uint8_t> Crypto::decryptAES(const vector<uint8_t>& encrypted_data, const SymmetricKey& key) {
#ifdef HAVE_OPENSSL
    if (encrypted_data.size() < 16) {
        return vector<uint8_t>(); // 数据太短
    }
    
    // 提取IV
    vector<uint8_t> iv(encrypted_data.begin(), encrypted_data.begin() + 16);
    
    // 获取密文数据
    vector<uint8_t> ciphertext(encrypted_data.begin() + 16, encrypted_data.end());
    
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return vector<uint8_t>();
    
    // 初始化解密操作
    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key.key.data(), iv.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return vector<uint8_t>();
    }
    
    // 解密数据
    int len;
    int plaintext_len;
    vector<uint8_t> plaintext(ciphertext.size() + AES_BLOCK_SIZE);
    
    if (EVP_DecryptUpdate(ctx, plaintext.data(), &len, ciphertext.data(), ciphertext.size()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return vector<uint8_t>();
    }
    plaintext_len = len;
    
    // 完成解密（处理填充）
    if (EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return vector<uint8_t>();
    }
    plaintext_len += len;
    
    // 调整结果大小
    plaintext.resize(plaintext_len);
    
    EVP_CIPHER_CTX_free(ctx);
    return plaintext;
#else
    // 回退到简化的XOR解密实现
    if (encrypted_data.size() < 16) {
        return vector<uint8_t>(); // 数据太短
    }
    
    // 提取IV
    vector<uint8_t> iv(encrypted_data.begin(), encrypted_data.begin() + 16);
    
    // 解密数据
    vector<uint8_t> result;
    for (size_t i = 16; i < encrypted_data.size(); ++i) {
        size_t data_index = i - 16;
        uint8_t decrypted_byte = encrypted_data[i] ^ key.key[data_index % key.key.size()] ^ iv[data_index % iv.size()];
        result.push_back(decrypted_byte);
    }
    
    return result;
#endif
}

// 从文件加载RSA密钥对（简化实现）
Crypto::RSAKeyPair Crypto::loadRSAKeyPair(const string& cert_path) {
    RSAKeyPair keypair;
    
    // 尝试读取私钥文件
    fs::path private_key_path = fs::path(cert_path) / "private.key";
    fs::path public_key_path = fs::path(cert_path) / "public.key";
    
    if (fs::exists(private_key_path)) {
        ifstream file(private_key_path);
        if (file.is_open()) {
            string line;
            while (getline(file, line)) {
                keypair.private_key += line + "\n";
            }
            file.close();
        }
    }
    
    if (fs::exists(public_key_path)) {
        ifstream file(public_key_path);
        if (file.is_open()) {
            string line;
            while (getline(file, line)) {
                keypair.public_key += line + "\n";
            }
            file.close();
        }
    }
    
    return keypair;
}

// 简化的RSA公钥加密（实际应使用OpenSSL）
vector<uint8_t> Crypto::encryptRSA(const vector<uint8_t>& data, const string& public_key) {
    // 简化实现：使用公钥的哈希作为密钥进行对称加密
    string key_hash = sha256Hash(public_key);
    SymmetricKey sym_key;
    sym_key.key = stringToBytes(key_hash);
    sym_key.iv = generateRandomBytes(16);
    
    return encryptAES(data, sym_key);
}

// 简化的RSA私钥解密
vector<uint8_t> Crypto::decryptRSA(const vector<uint8_t>& encrypted_data, const string& private_key) {
    // 简化实现：使用私钥对应的公钥哈希进行解密
    // 实际实现中应该从私钥推导出公钥
    string key_hash = sha256Hash(private_key);
    SymmetricKey sym_key;
    sym_key.key = stringToBytes(key_hash);
    
    return decryptAES(encrypted_data, sym_key);
}

// 字符串转字节数组
vector<uint8_t> Crypto::stringToBytes(const string& str) {
    vector<uint8_t> bytes;
    bytes.reserve(str.size());
    for (char c : str) {
        bytes.push_back(static_cast<uint8_t>(c));
    }
    return bytes;
}

// 字节数组转字符串
string Crypto::bytesToString(const vector<uint8_t>& bytes) {
    string str;
    str.reserve(bytes.size());
    for (uint8_t b : bytes) {
        str.push_back(static_cast<char>(b));
    }
    return str;
}

// 生成随机字节
vector<uint8_t> Crypto::generateRandomBytes(size_t size) {
    vector<uint8_t> bytes(size);
    
#ifdef HAVE_OPENSSL
    // 使用OpenSSL的安全随机数生成器
    if (RAND_bytes(bytes.data(), size) != 1) {
        // 如果OpenSSL随机数生成失败，回退到标准库
        static random_device rd;
        static mt19937 gen(rd());
        uniform_int_distribution<int> dis(0, 255);
        
        for (size_t i = 0; i < size; ++i) {
            bytes[i] = static_cast<uint8_t>(dis(gen));
        }
    }
#else
    // 使用标准库随机数生成器
    static random_device rd;
    static mt19937 gen(rd());
    uniform_int_distribution<int> dis(0, 255);
    
    for (size_t i = 0; i < size; ++i) {
        bytes[i] = static_cast<uint8_t>(dis(gen));
    }
#endif
    
    return bytes;
}

// SHA256哈希
string Crypto::sha256Hash(const string& data) {
    return sha256_string(data);
}

// PBKDF2密钥派生
vector<uint8_t> Crypto::pbkdf2(const string& password, const vector<uint8_t>& salt, int iterations, size_t key_length) {
#ifdef HAVE_OPENSSL
    // 使用OpenSSL的PBKDF2实现，但避免EVP_sha256冲突，直接使用简化实现
    return pbkdf2Fallback(password, salt, iterations, key_length);
#else
    // 使用简化实现
    return pbkdf2Fallback(password, salt, iterations, key_length);
#endif
}

// PBKDF2简化实现（回退方案）
vector<uint8_t> Crypto::pbkdf2Fallback(const string& password, const vector<uint8_t>& salt, int iterations, size_t key_length) {
    vector<uint8_t> result;
    result.reserve(key_length);
    
    string password_salt = password + bytesToString(salt);
    
    // 简化的PBKDF2实现
    for (size_t i = 0; i < key_length; ++i) {
        string input = password_salt + to_string(i);
        
        // 多轮哈希
        string hash = input;
        for (int j = 0; j < iterations; ++j) {
            hash = sha256Hash(hash);
        }
        
        // 取哈希的第一个字节
        if (!hash.empty()) {
            result.push_back(static_cast<uint8_t>(hash[0]));
        } else {
            result.push_back(0);
        }
    }
    
    return result;
}

// 生成会话密钥
Crypto::SymmetricKey Crypto::generateSessionKey() {
    SymmetricKey key;
    key.key = generateRandomBytes(32);  // AES-256需要32字节密钥
    key.iv = generateRandomBytes(16);   // AES需要16字节IV
    return key;
}

// 验证密钥有效性
bool Crypto::isKeyValid(const SymmetricKey& key) {
    return key.key.size() == 32 && key.iv.size() == 16;
}