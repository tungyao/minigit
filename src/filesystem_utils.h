#pragma once

#include "common.h"
#define MARKNAME ".minigit"
/**
 * 文件系统工具类
 * 提供MiniGit所需的文件和目录操作
 */
class FileSystemUtils {
private:
	FileSystemUtils(const FileSystemUtils &) = delete;
	FileSystemUtils &operator=(const FileSystemUtils &) = delete;

	FileSystemUtils() = default;
	~FileSystemUtils() = default;

	string repo;

public:
	static FileSystemUtils& getInstance() {
		static FileSystemUtils instance;
		return instance;
	}

	bool endsWith(const std::string &str, const std::string &suffix);
	// 仓库路径相关
	fs::path repoRoot();
	fs::path mgDir();
	fs::path objectsDir();
	fs::path indexPath();
	fs::path headPath();
	fs::path configPath();

	void useRepo(const string & repo_name);

	// 文件操作
	void writeText(const fs::path &p, const string &s);
	string readText(const fs::path &p);
	void writeBinary(const fs::path &p, const vector<uint8_t> &data);
	vector<uint8_t> readBinary(const fs::path &p);

	// 仓库检查
	void ensureRepo();
	bool isIgnored(const fs::path &p);
};