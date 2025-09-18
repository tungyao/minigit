#pragma once

#include "common.h"
#include "filesystem_utils.h"
#include "index.h"
#include "objects.h"
#include "commit.h"

/**
 * MiniGit历史命令实现类
 * 实现历史相关的Git命令功能：reset, log, diff
 */
class CommandsHistory
{
public:
    // 历史命令
    static int reset(vector<string> args);
    static int log(vector<string> args);
    static int diff(vector<string> args);

    // 文件状态检测辅助方法
    struct FileStatus
    {
        string path;
        string status; // "M"修改, "A"新增, "D"删除, "??"未跟踪, "R"重命名
        string staged_hash; // 暂存区中的哈希
        string working_hash; // 工作目录中的哈希
        string old_path; // 重命名前的路径（仅在status="R"时有效）
    };

    static vector<FileStatus> getWorkingDirectoryStatus();
    static string calculateWorkingFileHash(const fs::path& file_path);
    static void scanWorkingDirectory(const fs::path& dir, vector<string>& files);

    // 历史文件追踪辅助方法
    static Index::IndexMap getHistoricalFiles(const string& head_commit_id);

    // 提交变更显示辅助方法
    static void showCommitChanges(const Commit& commit);
};
