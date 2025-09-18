#pragma once

#include "common.h"
#include <functional>

/**
 * 压缩工具类
 * 提供文件和数据的压缩/解压功能，带进度回调
 */
class CompressionUtils {
public:
	static uint8_t DECOMPRESS_FLAG_CLONE;
	static uint8_t DECOMPRESS_FLAG_PUSH;
	static uint8_t DECOMPRESS_FLAG_PULL;


	// 进度回调函数类型
	// 参数：当前进度（0-100），操作描述
	using ProgressCallback = std::function<void(int progress, const string &description)>;

	/**
	 * 压缩文件到字节数组
	 * @param file_path 文件路径
	 * @param compressed_data 输出压缩数据
	 * @param progress_callback 进度回调
	 * @return 是否成功
	 */
	static bool compressFile(const fs::path &file_path,
	                         vector<uint8_t> &compressed_data,
	                         ProgressCallback progress_callback = nullptr);

	/**
	 * 解压字节数组到文件
	 * @param compressed_data 压缩数据
	 * @param file_path 输出文件路径
	 * @param progress_callback 进度回调
	 * @return 是否成功
	 */
	static bool decompressToFile(const vector<uint8_t> &compressed_data,
	                             const fs::path &file_path,
	                             ProgressCallback progress_callback = nullptr);

	/**
	 * 压缩字节数据
	 * @param input_data 输入数据
	 * @param compressed_data 输出压缩数据
	 * @param progress_callback 进度回调
	 * @return 是否成功
	 */
	static bool compressData(const vector<uint8_t> &input_data,
	                         vector<uint8_t> &compressed_data,
	                         ProgressCallback progress_callback = nullptr);

	/**
	 * 解压字节数据
	 * @param compressed_data 压缩数据
	 * @param output_data 输出数据
	 * @param progress_callback 进度回调
	 * @return 是否成功
	 */
	static bool decompressData(const vector<uint8_t> &compressed_data,
	                           vector<uint8_t> &output_data,
	                           ProgressCallback progress_callback = nullptr);

	/**
	 * 创建压缩归档包含多个文件
	 * @param file_paths 文件路径列表（相对路径）
	 * @param base_path 基础路径
	 * @param archive_data 输出归档数据
	 * @param progress_callback 进度回调
	 * @return 是否成功
	 */
	static bool createCompressedArchive(const vector<fs::path> &file_paths,
	                                    const fs::path &base_path,
	                                    vector<uint8_t> &archive_data, uint32_t &raw_size,
	                                    ProgressCallback progress_callback = nullptr);

	/**
	 * 从压缩归档提取文件
	 * @param archive_data 归档数据
	 * @param output_path 输出路径
	 * @param progress_callback 进度回调
	 * @return 是否成功
	 */
	static bool extractCompressedArchive(const vector<uint8_t> &archive_data,
	                                     const fs::path &output_path,
	                                     ProgressCallback progress_callback = nullptr);

	/**
	 * 获取压缩比率字符串
	 * @param original_size 原始大小
	 * @param compressed_size 压缩大小
	 * @return 压缩比率描述
	 */
	static string getCompressionRatio(size_t original_size, size_t compressed_size);

private:
	// 简单的归档格式头部
	struct ArchiveHeader {
		uint32_t magic; // 魔数 'MGIT'
		uint32_t version; // 版本
		uint32_t file_count; // 文件数量
		uint64_t total_size; // 原始总大小
	};

	// 归档文件条目
	struct ArchiveEntry {
		uint32_t path_length; // 路径长度
		uint64_t file_size; // 文件大小
		uint32_t checksum; // 校验和
		// 接下来是路径字符串和文件数据
	};

	static const uint32_t ARCHIVE_MAGIC = 0x4D474954; // 'MGIT'
	static const uint32_t ARCHIVE_VERSION = 1;
	static const int COMPRESSION_LEVEL = 6;
};