#include "compression.h"
#include "filesystem_utils.h"
#include <cstring>
#include <iomanip>
#include <lz4.h>
#include <sstream>

#include "utils.h"

uint8_t CompressionUtils::DECOMPRESS_FLAG_CLONE = 0x0;
uint8_t CompressionUtils::DECOMPRESS_FLAG_PUSH = CompressionUtils::DECOMPRESS_FLAG_CLONE + 1;
uint8_t CompressionUtils::DECOMPRESS_FLAG_PULL = CompressionUtils::DECOMPRESS_FLAG_CLONE + 2;

bool CompressionUtils::compressFile(const fs::path &file_path, vector<uint8_t> &compressed_data,
									ProgressCallback progress_callback) {
	if (!fs::exists(file_path)) {
		return false;
	}

	if (progress_callback) {
		progress_callback(0, "read file: " + file_path.filename().string());
	}

	// 读取文件数据
	vector<uint8_t> file_data;
	try {
		file_data = FileSystemUtils::getInstance().readBinary(file_path);
	} catch (const exception &e) {
		return false;
	}

	if (progress_callback) {
		progress_callback(30, "compress file...");
	}

	// 压缩数据
	bool result = compressData(file_data, compressed_data, [&](int progress, const string &desc) {
		if (progress_callback) {
			// 将内部进度映射到30-100范围
			int adjusted_progress = 30 + (progress * 70 / 100);
			progress_callback(adjusted_progress, desc);
		}
	});

	if (progress_callback && result) {
		progress_callback(100, "compress data complate");
	}

	return result;
}

bool CompressionUtils::decompressToFile(const vector<uint8_t> &compressed_data,
										const fs::path &file_path,
										ProgressCallback progress_callback) {
	if (progress_callback) {
		progress_callback(0, "start decompression to: " + file_path.filename().string());
	}

	vector<uint8_t> decompressed_data;
	bool result =
		decompressData(compressed_data, decompressed_data, [&](int progress, const string &desc) {
			if (progress_callback) {
				// 将内部进度映射到0-80范围
				int adjusted_progress = progress * 80 / 100;
				progress_callback(adjusted_progress, desc);
			}
		});

	if (!result) {
		return false;
	}

	if (progress_callback) {
		progress_callback(80, "write file...");
	}

	// 确保目录存在
	fs::create_directories(file_path.parent_path());

	// 写入文件
	try {
		FileSystemUtils::getInstance().writeBinary(file_path, decompressed_data);
	} catch (const exception &e) {
		return false;
	}

	if (progress_callback) {
		progress_callback(100, "decompress complated");
	}

	return true;
}

bool CompressionUtils::compressData(const vector<uint8_t> &input_data, vector<uint8_t> &packed,
									ProgressCallback progress_callback) {
	if (input_data.empty()) {
		packed.clear();
		return true;
	}

	int input_size = static_cast<int>(input_data.size());
	int max_compressed_size = LZ4_compressBound(input_size);

	// 分配真实空间（包含前4字节存储原始长度）
	packed.resize(4 + max_compressed_size);

	// 写入原始数据长度（前4字节）
	std::memcpy(packed.data(), &input_size, sizeof(int));

	if (progress_callback) {
		progress_callback(10, "init compress...");
	}

	// 压缩
	int compressed_size = LZ4_compress_default(reinterpret_cast<const char *>(input_data.data()),
											   reinterpret_cast<char *>(packed.data() + 4),
											   input_size, max_compressed_size);

	if (compressed_size <= 0) {
		// 压缩失败
		packed.clear();
		return false;
	}

	// 调整 packed 大小到实际压缩结果
	packed.resize(4 + compressed_size);

	if (progress_callback) {
		string ratio = getCompressionRatio(input_size, compressed_size);
		progress_callback(100, "compressed " + ratio);
	}

	return true;
}

bool CompressionUtils::decompressData(const vector<uint8_t> &packed_data, vector<uint8_t> &output,
									  ProgressCallback progress_callback) {
	if (packed_data.empty()) {
		output.clear();
		return true;
	}

	if (packed_data.size() < 4) {
		return false; // 数据太小，连长度都没有
	}

	int original_size = 0;
	std::memcpy(&original_size, packed_data.data(), sizeof(int));

	if (original_size <= 0) {
		return false; // 长度异常
	}

	// 分配解压缓冲区
	output.resize(original_size);

	if (progress_callback) {
		progress_callback(10, "preparing to decompress...");
	}

	// 解压
	int decompressed_size =
		LZ4_decompress_safe(reinterpret_cast<const char *>(packed_data.data() + 4),
							reinterpret_cast<char *>(output.data()),
							static_cast<int>(packed_data.size() - 4), original_size);

	if (decompressed_size < 0) {
		// 解压失败
		output.clear();
		return false;
	}

	// 正常情况下 decompressed_size == original_size
	output.resize(decompressed_size);

	if (progress_callback) {
		progress_callback(100, "decompression completed");
	}

	return true;
}

bool CompressionUtils::createCompressedArchive(const vector<fs::path> &file_paths,
											   const fs::path &base_path,
											   vector<uint8_t> &archive_data, uint32_t &raw_size,
											   ProgressCallback progress_callback) {
	if (file_paths.empty()) {
		return false;
	}

	if (progress_callback) {
		progress_callback(0, "create archive...");
	}

	// 构建归档数据
	vector<uint8_t> raw_archive;

	// 写入归档头部
	ArchiveHeader header;
	header.magic = ARCHIVE_MAGIC;
	header.version = ARCHIVE_VERSION;
	header.file_count = static_cast<uint32_t>(file_paths.size());
	header.total_size = 0;

	// 计算总大小
	for (const auto &file_path : file_paths) {
		fs::path full_path = base_path / file_path;
		if (fs::exists(full_path) && fs::is_regular_file(full_path)) {
			header.total_size += fs::file_size(full_path);
		}
	}

	// 写入头部
	const uint8_t *header_bytes = reinterpret_cast<const uint8_t *>(&header);
	raw_archive.insert(raw_archive.end(), header_bytes, header_bytes + sizeof(header));

	// 写入文件条目
	for (size_t i = 0; i < file_paths.size(); ++i) {
		const auto &file_path = file_paths[i];
		fs::path full_path = base_path / file_path;

		if (progress_callback) {
			int progress = 10 + (i * 40 / file_paths.size());
			progress_callback(progress, "add file: " + file_path.filename().string());
		}

		if (!fs::exists(full_path) || !fs::is_regular_file(full_path)) {
			continue;
		}

		// 读取文件数据
		vector<uint8_t> file_data;
		try {
			file_data = FileSystemUtils::getInstance().readBinary(full_path);
		} catch (const exception &e) {
			continue;
		}

		// 创建文件条目
		ArchiveEntry entry;
		string path_str = file_path.generic_string();
		entry.path_length = static_cast<uint32_t>(path_str.length());
		entry.file_size = file_data.size();
		entry.checksum = crc32(file_data);

		// 写入条目头
		const uint8_t *entry_bytes = reinterpret_cast<const uint8_t *>(&entry);
		raw_archive.insert(raw_archive.end(), entry_bytes, entry_bytes + sizeof(entry));

		// 写入路径
		raw_archive.insert(raw_archive.end(), path_str.begin(), path_str.end());

		// 写入文件数据
		raw_archive.insert(raw_archive.end(), file_data.begin(), file_data.end());
	}

	if (progress_callback) {
		progress_callback(50, "compress archive data...");
	}
	raw_size = raw_archive.size();
	// 压缩整个归档
	bool result = compressData(raw_archive, archive_data, [&](int progress, const string &desc) {
		if (progress_callback) {
			int adjusted_progress = 50 + (progress * 50 / 100);
			progress_callback(adjusted_progress, desc);
		}
	});

	// if (progress_callback && result) {
	// 	string ratio = getCompressionRatio(raw_archive.size(), archive_data.size());
	// 	progress_callback(100, "archiving has been completed" + ratio);
	// }

	return result;
}

bool CompressionUtils::extractCompressedArchive(const vector<uint8_t> &archive_data,
												const fs::path &output_path,
												ProgressCallback progress_callback) {
	if (archive_data.empty()) {
		return false;
	}

	if (progress_callback) {
		progress_callback(0, "unarchive...");
	}

	// 解压缩归档数据
	vector<uint8_t> raw_archive;
	bool result = decompressData(archive_data, raw_archive, [&](int progress, const string &desc) {
		if (progress_callback) {
			int adjusted_progress = progress * 30 / 100;
			progress_callback(adjusted_progress, desc);
		}
	});

	if (!result) {
		return false;
	}

	if (progress_callback) {
		progress_callback(30, "analysis of the archive...");
	}

	// 解析归档头部
	if (raw_archive.size() < sizeof(ArchiveHeader)) {
		return false;
	}

	ArchiveHeader header;
	memcpy(&header, raw_archive.data(), sizeof(header));

	if (header.magic != ARCHIVE_MAGIC) {
		return false;
	}

	// 确保输出目录存在
	fs::create_directories(output_path);

	size_t offset = sizeof(ArchiveHeader);

	// 提取文件
	for (uint32_t i = 0; i < header.file_count; ++i) {
		if (offset + sizeof(ArchiveEntry) > raw_archive.size()) {
			return false;
		}

		ArchiveEntry entry;
		memcpy(&entry, raw_archive.data() + offset, sizeof(entry));
		offset += sizeof(ArchiveEntry);

		// 读取文件路径
		if (offset + entry.path_length > raw_archive.size()) {
			return false;
		}

		string file_path_str(reinterpret_cast<const char *>(raw_archive.data() + offset),
							 entry.path_length);
		offset += entry.path_length;
		if (progress_callback) {
			int progress = 30 + (i * 70 / header.file_count);
			progress_callback(progress, to_string(i + 1) + "/" + to_string(header.file_count));
		}
		// 读取文件数据
		if (offset + entry.file_size > raw_archive.size()) {
			return false;
		}

		vector<uint8_t> file_data(raw_archive.data() + offset,
								  raw_archive.data() + offset + entry.file_size);
		offset += entry.file_size;

		// 验证校验和
		uint32_t calculated_checksum = crc32(file_data);
		if (calculated_checksum != entry.checksum) {
			return false;
		}
		fs::path full_output_path;
		if (file_path_str.find(MARKNAME) != std::string::npos) {
			full_output_path = output_path / file_path_str;
		} else {
			full_output_path = output_path / MARKNAME / file_path_str;
		}
		cout << "  " << file_path_str;
		fs::create_directories(full_output_path.parent_path());

		try {
			FileSystemUtils::getInstance().writeBinary(full_output_path, file_data);
		} catch (const exception &e) {
			return false;
		}
	}

	if (progress_callback) {
		progress_callback(100, "unarchive complete");
	}

	return true;
}

string CompressionUtils::getCompressionRatio(size_t original_size, size_t compressed_size) {
	if (original_size == 0) {
		return "(0%)";
	}

	double ratio = (1.0 - static_cast<double>(compressed_size) / original_size) * 100;

	ostringstream oss;
	oss << "(" << fixed << setprecision(1) << ratio << "% compression)";
	return oss.str();
}