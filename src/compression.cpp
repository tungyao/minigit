#include "compression.h"
#include "filesystem_utils.h"
#include <zlib.h>
#include <cstring>
#include <iomanip>
#include <sstream>

bool CompressionUtils::compressFile(const fs::path& file_path, 
                                   vector<uint8_t>& compressed_data,
                                   ProgressCallback progress_callback) {
    if (!fs::exists(file_path)) {
        return false;
    }

    if (progress_callback) {
        progress_callback(0, "读取文件: " + file_path.filename().string());
    }

    // 读取文件数据
    vector<uint8_t> file_data;
    try {
        file_data = FileSystemUtils::readBinary(file_path);
    } catch (const exception& e) {
        return false;
    }

    if (progress_callback) {
        progress_callback(30, "压缩文件数据...");
    }

    // 压缩数据
    bool result = compressData(file_data, compressed_data, 
        [&](int progress, const string& desc) {
            if (progress_callback) {
                // 将内部进度映射到30-100范围
                int adjusted_progress = 30 + (progress * 70 / 100);
                progress_callback(adjusted_progress, desc);
            }
        });

    if (progress_callback && result) {
        progress_callback(100, "文件压缩完成");
    }

    return result;
}

bool CompressionUtils::decompressToFile(const vector<uint8_t>& compressed_data,
                                       const fs::path& file_path,
                                       ProgressCallback progress_callback) {
    if (progress_callback) {
        progress_callback(0, "开始解压缩到: " + file_path.filename().string());
    }

    vector<uint8_t> decompressed_data;
    bool result = decompressData(compressed_data, decompressed_data,
        [&](int progress, const string& desc) {
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
        progress_callback(80, "写入文件...");
    }

    // 确保目录存在
    fs::create_directories(file_path.parent_path());

    // 写入文件
    try {
        FileSystemUtils::writeBinary(file_path, decompressed_data);
    } catch (const exception& e) {
        return false;
    }

    if (progress_callback) {
        progress_callback(100, "解压缩完成");
    }

    return true;
}

bool CompressionUtils::compressData(const vector<uint8_t>& input_data,
                                   vector<uint8_t>& compressed_data,
                                   ProgressCallback progress_callback) {
    if (input_data.empty()) {
        compressed_data.clear();
        return true;
    }

    // 估算压缩后大小（通常比原始数据小，但为安全起见预留更多空间）
    uLong compressed_size = compressBound(input_data.size());
    compressed_data.resize(compressed_size);

    if (progress_callback) {
        progress_callback(10, "初始化压缩...");
    }

    // 使用zlib压缩
    int result = compress2(compressed_data.data(), &compressed_size,
                          input_data.data(), input_data.size(),
                          COMPRESSION_LEVEL);

    if (result != Z_OK) {
        return false;
    }

    // 调整到实际压缩大小
    compressed_data.resize(compressed_size);

    if (progress_callback) {
        string ratio = getCompressionRatio(input_data.size(), compressed_size);
        progress_callback(100, "压缩完成 " + ratio);
    }

    return true;
}

bool CompressionUtils::decompressData(const vector<uint8_t>& compressed_data,
                                     vector<uint8_t>& output_data,
                                     ProgressCallback progress_callback) {
    if (compressed_data.empty()) {
        output_data.clear();
        return true;
    }

    if (progress_callback) {
        progress_callback(10, "准备解压缩...");
    }

    // 对于解压缩，我们需要估算原始大小
    // 先尝试用压缩数据的4倍大小
    uLong estimated_size = compressed_data.size() * 4;
    output_data.resize(estimated_size);

    int result = Z_BUF_ERROR;
    int retry_count = 0;
    const int max_retries = 3;

    while (result == Z_BUF_ERROR && retry_count < max_retries) {
        uLong decompressed_size = estimated_size;
        result = uncompress(output_data.data(), &decompressed_size,
                           compressed_data.data(), compressed_data.size());

        if (result == Z_BUF_ERROR) {
            // 缓冲区太小，增加大小重试
            estimated_size *= 2;
            output_data.resize(estimated_size);
            retry_count++;
            
            if (progress_callback) {
                progress_callback(30 + retry_count * 20, "调整缓冲区大小...");
            }
        } else if (result == Z_OK) {
            // 成功解压，调整到实际大小
            output_data.resize(decompressed_size);
        }
    }

    if (result != Z_OK) {
        return false;
    }

    if (progress_callback) {
        progress_callback(100, "解压缩完成");
    }

    return true;
}

bool CompressionUtils::createCompressedArchive(const vector<fs::path>& file_paths,
                                              const fs::path& base_path,
                                              vector<uint8_t>& archive_data,
                                              ProgressCallback progress_callback) {
    if (file_paths.empty()) {
        return false;
    }

    if (progress_callback) {
        progress_callback(0, "创建归档...");
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
    for (const auto& file_path : file_paths) {
        fs::path full_path = base_path / file_path;
        if (fs::exists(full_path) && fs::is_regular_file(full_path)) {
            header.total_size += fs::file_size(full_path);
        }
    }

    // 写入头部
    const uint8_t* header_bytes = reinterpret_cast<const uint8_t*>(&header);
    raw_archive.insert(raw_archive.end(), header_bytes, header_bytes + sizeof(header));

    // 写入文件条目
    for (size_t i = 0; i < file_paths.size(); ++i) {
        const auto& file_path = file_paths[i];
        fs::path full_path = base_path / file_path;

        if (progress_callback) {
            int progress = 10 + (i * 40 / file_paths.size());
            progress_callback(progress, "添加文件: " + file_path.filename().string());
        }

        if (!fs::exists(full_path) || !fs::is_regular_file(full_path)) {
            continue;
        }

        // 读取文件数据
        vector<uint8_t> file_data;
        try {
            file_data = FileSystemUtils::readBinary(full_path);
        } catch (const exception& e) {
            continue;
        }

        // 创建文件条目
        ArchiveEntry entry;
        string path_str = file_path.generic_string();
        entry.path_length = static_cast<uint32_t>(path_str.length());
        entry.file_size = file_data.size();
        entry.checksum = crc32(0, file_data.data(), file_data.size());

        // 写入条目头
        const uint8_t* entry_bytes = reinterpret_cast<const uint8_t*>(&entry);
        raw_archive.insert(raw_archive.end(), entry_bytes, entry_bytes + sizeof(entry));

        // 写入路径
        raw_archive.insert(raw_archive.end(), path_str.begin(), path_str.end());

        // 写入文件数据
        raw_archive.insert(raw_archive.end(), file_data.begin(), file_data.end());
    }

    if (progress_callback) {
        progress_callback(50, "压缩归档数据...");
    }

    // 压缩整个归档
    bool result = compressData(raw_archive, archive_data, 
        [&](int progress, const string& desc) {
            if (progress_callback) {
                int adjusted_progress = 50 + (progress * 50 / 100);
                progress_callback(adjusted_progress, desc);
            }
        });

    if (progress_callback && result) {
        string ratio = getCompressionRatio(raw_archive.size(), archive_data.size());
        progress_callback(100, "归档创建完成 " + ratio);
    }

    return result;
}

bool CompressionUtils::extractCompressedArchive(const vector<uint8_t>& archive_data,
                                               const fs::path& output_path,
                                               ProgressCallback progress_callback) {
    if (archive_data.empty()) {
        return false;
    }

    if (progress_callback) {
        progress_callback(0, "解压缩归档...");
    }

    // 解压缩归档数据
    vector<uint8_t> raw_archive;
    bool result = decompressData(archive_data, raw_archive,
        [&](int progress, const string& desc) {
            if (progress_callback) {
                int adjusted_progress = progress * 30 / 100;
                progress_callback(adjusted_progress, desc);
            }
        });

    if (!result) {
        return false;
    }

    if (progress_callback) {
        progress_callback(30, "解析归档结构...");
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
        if (progress_callback) {
            int progress = 30 + (i * 70 / header.file_count);
            progress_callback(progress, "提取文件 " + to_string(i + 1) + "/" + to_string(header.file_count));
        }

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

        string file_path_str(reinterpret_cast<const char*>(raw_archive.data() + offset), entry.path_length);
        offset += entry.path_length;

        // 读取文件数据
        if (offset + entry.file_size > raw_archive.size()) {
            return false;
        }

        vector<uint8_t> file_data(raw_archive.data() + offset, raw_archive.data() + offset + entry.file_size);
        offset += entry.file_size;

        // 验证校验和
        uint32_t calculated_checksum = crc32(0, file_data.data(), file_data.size());
        if (calculated_checksum != entry.checksum) {
            return false;
        }

        // 写入文件
        fs::path full_output_path = output_path / file_path_str;
        fs::create_directories(full_output_path.parent_path());

        try {
            FileSystemUtils::writeBinary(full_output_path, file_data);
        } catch (const exception& e) {
            return false;
        }
    }

    if (progress_callback) {
        progress_callback(100, "归档提取完成");
    }

    return true;
}

string CompressionUtils::getCompressionRatio(size_t original_size, size_t compressed_size) {
    if (original_size == 0) {
        return "(0%)";
    }
    
    double ratio = (1.0 - static_cast<double>(compressed_size) / original_size) * 100;
    
    ostringstream oss;
    oss << "(" << fixed << setprecision(1) << ratio << "% 压缩)";
    return oss.str();
}