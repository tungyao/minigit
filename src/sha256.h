#pragma once

#include "common.h"

/**
 * SHA-1 哈希计算类
 * 高性能实现，针对大文件优化
 */
class SHA1 {
public:
	void init();
	void update(const unsigned char *data, size_t len);
	void finalize(unsigned char out[20]);

private:
	uint32_t h[5];
	unsigned char buf[64];
	size_t buf_len = 0;
	uint64_t total_len = 0;

	// 辅助函数
	static inline uint32_t rotl(uint32_t x, int n);
	static inline uint32_t f(int t, uint32_t b, uint32_t c, uint32_t d);
	static inline uint32_t k(int t);

	void process_block(const unsigned char *block);
};

/**
 * 计算字符串的SHA-1哈希
 */
string sha1_string(const string &str);

/**
 * 计算字节数组的SHA-1哈希
 */
string sha1_bytes(const vector<unsigned char> &data);

/**
 * 计算文件的SHA-1哈希（流式处理，内存高效）
 */
string sha1_file(const fs::path &p);

// 兼容性别名（保持向后兼容）
using SHA256 = SHA1;

inline string sha256_string(const string &str) {
	return sha1_string(str);
}

inline string sha256_bytes(const vector<unsigned char> &data) {
	return sha1_bytes(data);
}

inline string sha256_file(const fs::path &p) {
	return sha1_file(p);
}