#pragma once

#include "common.h"
#define MARKNAME ".minigit"
/**
 * 文件系统工具类
 * 提供MiniGit所需的文件和目录操作
 */
class FileSystemUtils {
public:
	// 仓库路径相关
	static fs::path repoRoot();
	static fs::path mgDir();
	static fs::path objectsDir();
	static fs::path indexPath();
	static fs::path headPath();
	static fs::path configPath();

	// 文件操作
	static void writeText(const fs::path& p, const string& s);
	static string readText(const fs::path& p);

	// 仓库检查
	static void ensureRepo();
	static bool isIgnored(const fs::path& p);
};