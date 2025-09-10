#include "crypto.h"
#include "sha256.h"
#include <random>
#include <cstring>
#include <aes.hpp>

void pkcs7_pad(std::vector<uint8_t>& data, size_t block_size) {
	if (block_size == 0 || block_size > 256) {
		throw std::invalid_argument("Invalid block size. Must be > 0 and <= 256.");
	}

	size_t padding_len = block_size - (data.size() % block_size);
	// 如果数据长度已经是块大小的倍数，则填充一个完整的块
	if (data.size() % block_size == 0) {
		padding_len = block_size;
	}

	uint8_t padding_char = static_cast<char>(padding_len);
	data.insert(data.end(), padding_len, padding_char);
}

void pkcs7_unpad(std::vector<uint8_t>& data) {
	if (data.empty()) {
		return;
	}

	uint8_t last_char = data.back();
	size_t padding_len = static_cast<uint8_t>(last_char);

	// 验证填充长度的有效性，防止恶意输入导致越界
	if (padding_len == 0 || padding_len > data.size()) {
		throw std::invalid_argument("Invalid PKCS#7 padding detected.");
	}

	// 验证填充字节是否正确
	for (size_t i = 0; i < padding_len; ++i) {
		if (static_cast<uint8_t>(data[data.size() - 1 - i]) != padding_len) {
			throw std::runtime_error("Incorrect PKCS#7 padding value.");
		}
	}

	data.resize(data.size() - padding_len);
}


// 从密码生成对称密钥
Crypto::SymmetricKey Crypto::generateKeyFromPassword(const string& password) {
	SymmetricKey key;

	// 生成随机盐
	vector<uint8_t> salt = generateRandomBytes(16);

	// 使用PBKDF2生成密钥和IV
	key.key = pbkdf2(password, salt, 10000, 32);  // AES-256需要32字节密钥
	key.iv = pbkdf2(password + "iv", salt, 1000, 16);  // AES需要16字节IV

	return key;
}

// 简化的AES-256-CBC加密（注意：这是一个简化实现，生产环境应使用专业加密库）
vector<uint8_t> Crypto::encryptAES(const vector<uint8_t>& data) {

	string key_hash = sha256Hash(Config::getInstance().password);
	vector<uint8_t> key = stringToBytes(key_hash);
	vector<uint8_t> iv = generateRandomBytes(16);

	// 1. 直接对输入数据进行操作，避免额外的内存分配
	vector<uint8_t> padded_data = data;
	pkcs7_pad(padded_data, 16);

	AES_ctx ctx;
	AES_init_ctx_iv(&ctx, key.data(), iv.data());
	AES_CBC_encrypt_buffer(&ctx, padded_data.data(), padded_data.size());

	// 2. 使用一个vector作为最终结果
	// 使用vector::insert()一次性插入IV和加密数据，减少内存分配和拷贝
	vector<uint8_t> result;
	result.reserve(iv.size() + padded_data.size());
	result.insert(result.end(), iv.begin(), iv.end());
	result.insert(result.end(), padded_data.begin(), padded_data.end());

	return result;
}

// 简化的AES-256-CBC解密
vector<uint8_t> Crypto::decryptAES(const vector<uint8_t>& encrypted_data) {
	// 1. Validate the input size
	// The encrypted data must be at least 16 bytes (for the IV) and a multiple of 16 (for the block cipher)
	if (encrypted_data.size() < 16 || (encrypted_data.size() - 16) % 16 != 0) {
		// Return an empty vector for invalid data
		return vector<uint8_t>();
	}
	string key_hash = sha256Hash(Config::getInstance().password);
	vector<uint8_t> key = stringToBytes(key_hash);
	// 2. Extract the Initialization Vector (IV)
	// The IV is the first 16 bytes of the encrypted data
	vector<uint8_t> iv(encrypted_data.begin(), encrypted_data.begin() + 16);

	// 3. Create a modifiable copy of the encrypted data, excluding the IV
	// This copy will be decrypted in place
	vector<uint8_t> decrypted_data(encrypted_data.begin() + 16, encrypted_data.end());

	// 4. Initialize the AES context
	AES_ctx ctx;
	AES_init_ctx_iv(&ctx, key.data(), iv.data());

	// 5. Decrypt the buffer
	// Pass the modifiable data buffer and its size to the decryption function
	AES_CBC_decrypt_buffer(&ctx, decrypted_data.data(), decrypted_data.size());

	// 6. Unpad the data
	// This operation modifies the vector in place, shrinking its size
	pkcs7_unpad(decrypted_data);

	return decrypted_data;
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
vector<uint8_t> Crypto::encryptRSA(const vector<uint8_t>& data) {
	// 简化实现：使用公钥的哈希作为密钥进行对称加密
	string key_hash = sha256Hash(Config::getInstance().public_key);
	SymmetricKey sym_key;
	sym_key.key = stringToBytes(key_hash);
	sym_key.iv = generateRandomBytes(16);

	return encryptAES(data);
}

// 简化的RSA私钥解密
vector<uint8_t> Crypto::decryptRSA(const vector<uint8_t>& encrypted_data) {
	// 简化实现：使用私钥对应的公钥哈希进行解密
	// 实际实现中应该从私钥推导出公钥
	string key_hash = sha256Hash(Config::getInstance().private_key);
	SymmetricKey sym_key;
	sym_key.key = stringToBytes(key_hash);

	return decryptAES(encrypted_data);
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

	static random_device rd;
	static mt19937 gen(rd());
	uniform_int_distribution<int> dis(0, 255);

	for (size_t i = 0; i < size; ++i) {
		bytes[i] = static_cast<uint8_t>(dis(gen));
	}

	return bytes;
}

// SHA256哈希
string Crypto::sha256Hash(const string& data) {
	return sha256_string(data);
}

// PBKDF2密钥派生（简化实现）
vector<uint8_t> Crypto::pbkdf2(const string& password, const vector<uint8_t>& salt, int iterations, size_t key_length) {
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
		}
		else {
			result.push_back(0);
		}
	}

	return result;
}