#include "sha256.h"
#include <cstring> // for memcpy

// SHA-1常量和辅助函数
inline uint32_t SHA1::rotl(uint32_t x, int n) {
	return (x << n) | (x >> (32 - n));
}

inline uint32_t SHA1::f(int t, uint32_t b, uint32_t c, uint32_t d) {
	if (t < 20)
		return (b & c) | ((~b) & d);
	if (t < 40)
		return b ^ c ^ d;
	if (t < 60)
		return (b & c) | (b & d) | (c & d);
	return b ^ c ^ d;
}

inline uint32_t SHA1::k(int t) {
	if (t < 20)
		return 0x5A827999;
	if (t < 40)
		return 0x6ED9EBA1;
	if (t < 60)
		return 0x8F1BBCDC;
	return 0xCA62C1D6;
}

void SHA1::init() {
	h[0] = 0x67452301;
	h[1] = 0xEFCDAB89;
	h[2] = 0x98BADCFE;
	h[3] = 0x10325476;
	h[4] = 0xC3D2E1F0;
	buf_len = 0;
	total_len = 0;
}

void SHA1::process_block(const unsigned char *block) {
	uint32_t w[80];

	// 将64字节的输入分解为16个32位字
	for (int i = 0; i < 16; i++) {
		w[i] = (block[i * 4] << 24) | (block[i * 4 + 1] << 16) | (block[i * 4 + 2] << 8) |
			   block[i * 4 + 3];
	}

	// 扩展到80个字
	for (int i = 16; i < 80; i++) {
		w[i] = rotl(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
	}

	uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];

	// 80轮主循环
	for (int i = 0; i < 80; i++) {
		uint32_t temp = rotl(a, 5) + f(i, b, c, d) + e + w[i] + k(i);
		e = d;
		d = c;
		c = rotl(b, 30);
		b = a;
		a = temp;
	}

	h[0] += a;
	h[1] += b;
	h[2] += c;
	h[3] += d;
	h[4] += e;
}

void SHA1::update(const unsigned char *data, size_t len) {
	total_len += len;

	// 处理缓冲区中的剩余数据
	while (len > 0) {
		size_t copy_len = min(len, 64 - buf_len);
		memcpy(buf + buf_len, data, copy_len);
		buf_len += copy_len;
		data += copy_len;
		len -= copy_len;

		if (buf_len == 64) {
			process_block(buf);
			buf_len = 0;
		}
	}
}

void SHA1::finalize(unsigned char out[20]) {
	// 添加填充
	buf[buf_len++] = 0x80;

	// 如果剩余空间不足8字节存放长度，则填充到下一个块
	if (buf_len > 56) {
		while (buf_len < 64) {
			buf[buf_len++] = 0;
		}
		process_block(buf);
		buf_len = 0;
	}

	// 填充0直到56字节
	while (buf_len < 56) {
		buf[buf_len++] = 0;
	}

	// 添加长度（以比特为单位）
	uint64_t total_bits = total_len * 8;
	for (int i = 7; i >= 0; i--) {
		buf[56 + i] = (unsigned char)(total_bits & 0xFF);
		total_bits >>= 8;
	}

	process_block(buf);

	// 输出结果
	for (int i = 0; i < 5; i++) {
		out[i * 4] = (h[i] >> 24) & 0xFF;
		out[i * 4 + 1] = (h[i] >> 16) & 0xFF;
		out[i * 4 + 2] = (h[i] >> 8) & 0xFF;
		out[i * 4 + 3] = h[i] & 0xFF;
	}
}

string sha1_bytes(const vector<unsigned char> &data) {
	SHA1 s;
	s.init();
	s.update(data.data(), data.size());
	unsigned char out[20];
	s.finalize(out);

	static const char *hex = "0123456789abcdef";
	string r;
	r.reserve(40);
	for (int i = 0; i < 20; ++i) {
		r.push_back(hex[out[i] >> 4]);
		r.push_back(hex[out[i] & 0xF]);
	}
	return r;
}

string sha1_string(const string &str) {
	vector<unsigned char> data(str.begin(), str.end());
	return sha1_bytes(data);
}

string sha1_file(const fs::path &p) {
	ifstream f(p, ios::binary);
	if (!f.is_open()) {
		throw runtime_error("Cannot open file: " + p.string());
	}

	SHA1 s;
	s.init();

	// 使用固定大小缓冲区流式处理，避免一次性加载整个文件
	constexpr size_t BUFFER_SIZE = 65536; // 64KB缓冲区
	vector<unsigned char> buffer(BUFFER_SIZE);

	while (f.good()) {
		f.read(reinterpret_cast<char *>(buffer.data()), BUFFER_SIZE);
		size_t bytes_read = f.gcount();
		if (bytes_read > 0) {
			s.update(buffer.data(), bytes_read);
		}
	}

	unsigned char out[20];
	s.finalize(out);

	static const char *hex = "0123456789abcdef";
	string r;
	r.reserve(40);
	for (int i = 0; i < 20; ++i) {
		r.push_back(hex[out[i] >> 4]);
		r.push_back(hex[out[i] & 0xF]);
	}
	return r;
}