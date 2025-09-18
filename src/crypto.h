#pragma once

#include "common.h"
#include <protocol.h>

void pkcs7_pad(std::vector<uint8_t> &data, size_t block_size);

void pkcs7_unpad(std::vector<uint8_t> &data);


/**
 * 加密工具类
 * 支持对称加密和RSA证书加密
 */
class Crypto {
public:
	// 对称加密相关
	struct SymmetricKey {
		vector<uint8_t> key;
		vector<uint8_t> iv;
	};

	// 从密码生成对称密钥
	static SymmetricKey generateKeyFromPassword(const string &password);

	// AES-256-CBC加密
	static vector<uint8_t> encryptAES(const vector<uint8_t> &data);

	// AES-256-CBC解密
	static vector<uint8_t> decryptAES(const vector<uint8_t> &encrypted_data);

	// RSA证书相关
	struct RSAKeyPair {
		string public_key;
		string private_key;
	};

	// 从文件加载RSA密钥对
	static RSAKeyPair loadRSAKeyPair(const string &cert_path);

	// RSA公钥加密
	static vector<uint8_t> encryptRSA(const vector<uint8_t> &data);

	// RSA私钥解密
	static vector<uint8_t> decryptRSA(const vector<uint8_t> &encrypted_data);

	// 工具方法
	static vector<uint8_t> stringToBytes(const string &str);
	static string bytesToString(const vector<uint8_t> &bytes);

	// 随机数生成
	static vector<uint8_t> generateRandomBytes(size_t size);

	// 哈希函数
	static string sha256Hash(const string &data);

private:
	// PBKDF2密钥派生
	static vector<uint8_t> pbkdf2(const string &password, const vector<uint8_t> &salt,
	                              int iterations, size_t key_length);
};