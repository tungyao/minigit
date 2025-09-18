#pragma once

#include "commit.h"
#include "common.h"
#include "filesystem_utils.h"
#include "index.h"
#include "objects.h"

/**
 * MiniGit基本命令实现类
 * 实现基本的Git命令功能：init, add, commit, status, checkout
 */
class CommandsBasic {
public:
	// 基本命令
	static int init();
	static int add(vector<string> args);
	static int commit(vector<string> args);
	static int status();
	static int checkout();

	// 文件状态检测辅助方法
	struct FileStatus {
		string path;
		string status; // "M"修改, "A"新增, "D"删除, "??"未跟踪, "R"重命名
		string staged_hash; // 暂存区中的哈希
		string working_hash; // 工作目录中的哈希
		string old_path; // 重命名前的路径（仅在status="R"时有效）
	};

	static vector<FileStatus> getWorkingDirectoryStatus();

	static string calculateWorkingFileHash(const fs::path &file_path);

	static void scanWorkingDirectory(const fs::path &dir, vector<string> &files);

	static void detectRenames(vector<FileStatus> &statuses);

	// 历史文件追踪辅助方法
	static Index::IndexMap getHistoricalFiles(const string &head_commit_id);
};