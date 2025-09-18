#include "commands_history.h"
#include "sha256.h"
#include <commands_remote.h>

int CommandsHistory::reset(vector<string> args) {
	FileSystemUtils::getInstance().ensureRepo();

	// Parse arguments
	string mode = "mixed"; // default mode
	string target_commit;

	for (size_t i = 0; i < args.size(); ++i) {
		if (args[i] == "--soft") {
			mode = "soft";
		} else if (args[i] == "--mixed") {
			mode = "mixed";
		} else if (args[i] == "--hard") {
			mode = "hard";
		} else {
			// This should be the commit hash
			target_commit = args[i];
		}
	}

	// If no commit specified, use HEAD
	if (target_commit.empty()) {
		target_commit =
			FileSystemUtils::getInstance().readText(FileSystemUtils::getInstance().headPath());
		if (target_commit.empty()) {
			cerr << "No commits yet.\n";
			return 1;
		}
	}

	// Validate that the target commit exists
	auto target = CommitManager::loadCommit(target_commit);
	if (!target) {
		cerr << "Invalid commit: " << target_commit << "\n";
		return 1;
	}

	cout << "Resetting to commit " << target_commit.substr(0, 12) << " (" << mode << " mode)\n";

	// Step 1: Always update HEAD to target commit
	FileSystemUtils::getInstance().writeText(FileSystemUtils::getInstance().headPath(),
											 target_commit);

	// Step 2: Save current index before updating (needed for hard mode)
	auto current_index = Index::read();

	// Update index if mixed or hard mode
	if (mode == "mixed" || mode == "hard") {
		// Clear current index and set it to target commit's tree
		Index::write(target->tree);
		cout << "Updated index to match commit " << target_commit.substr(0, 12) << "\n";
	}

	// Step 3: Update working directory if hard mode
	if (mode == "hard") {
		// 收集当前工作目录中所有文件（在更新暂存区之前）
		vector<string> current_working_files;
		scanWorkingDirectory(FileSystemUtils::getInstance().repoRoot(), current_working_files);

		// 收集当前暂存区和HEAD中跟踪的文件（在更新之前）
		set<string> tracked_files;
		for (const auto &kv : current_index) {
			tracked_files.insert(kv.first);
		}
		// 如果有HEAD提交，也包含HEAD中的文件
		string current_head =
			FileSystemUtils::getInstance().readText(FileSystemUtils::getInstance().headPath());
		if (!current_head.empty()) {
			auto current_head_commit = CommitManager::loadCommit(current_head);
			if (current_head_commit) {
				for (const auto &kv : current_head_commit->tree) {
					tracked_files.insert(kv.first);
				}
			}
		}

		// 删除工作目录中存在但在目标提交中不存在的跟踪文件
		for (const string &file_path : current_working_files) {
			// 只处理跟踪的文件，忽略未跟踪的文件
			if (tracked_files.find(file_path) != tracked_files.end() &&
				target->tree.find(file_path) == target->tree.end()) {
				fs::path full_path = FileSystemUtils::getInstance().repoRoot() / file_path;
				try {
					fs::remove(full_path);
					cout << "Removed: " << file_path << "\n";
				} catch (const exception &e) {
					cerr << "Warning: Could not remove " << file_path << ": " << e.what() << "\n";
				}
			}
		}

		// 恢复目标提交中的所有文件到工作目录
		for (auto &kv : target->tree) {
			fs::path out = FileSystemUtils::getInstance().repoRoot() / kv.first;
			fs::create_directories(out.parent_path());
			try {
				fs::copy_file(FileSystemUtils::getInstance().objectsDir() / kv.second, out,
							  fs::copy_options::overwrite_existing);
				cout << "Restored: " << kv.first << "\n";
			} catch (const exception &e) {
				cerr << "Warning: Could not restore " << kv.first << ": " << e.what() << "\n";
			}
		}
		cout << "Updated working directory to match commit " << target_commit.substr(0, 12) << "\n";
	}

	cout << "Reset complete.\n";
	return 0;
}

int CommandsHistory::log(vector<string> args) {
	FileSystemUtils::getInstance().ensureRepo();

	// 解析参数
	int max_count = -1; // -1表示显示所有提交
	bool line = false;
	bool is_remote = false;
	string password;
	for (size_t i = 0; i < args.size(); ++i) {
		if (args[i] == "--line") {
			line = true;
		} else if (args[i] == "--remote") {
			is_remote = true;
		} else if (args[i] == "--password" && i + 1 < args.size()) {
			password = args[i + 1];
			i++;
		} else if (args[i].substr(0, 2) == "-n" && i + 1 < args.size()) {
			try {
				max_count = stoi(args[i + 1]);
				i++; // 跳过下一个参数
			} catch (const exception &) {
				cerr << "Invalid number: " << args[i + 1] << "\n";
				return 1;
			}
		} else if (args[i].substr(0, 8) == "--max-count=" && args[i].length() > 8) {
			try {
				max_count = stoi(args[i].substr(12));
			} catch (const exception &) {
				cerr << "Invalid number in --max-count: " << args[i] << "\n";
				return 1;
			}
		}
	}

	string remote = CommandsRemote::getRemote();
	if (remote.empty()) {
		cerr << "No remote configured. Use: minigit set-remote <folder>\n";
		return 1;
	}
	if (is_remote && password.empty()) {
		cerr << "Remote log requires --password option\n";
		cerr << "Usage: minigit log --remote --password <password>\n";
		return 1;
	}
	CommandsRemote::NetworkRemote network_remote = CommandsRemote::parseNetworkRemote(remote);
	if (network_remote.host.empty()) {
		cerr << "Invalid network remote format: " << remote << "\n";
		return 1;
	}
	if (is_remote) {
		return CommandsRemote::networkLog(network_remote, password, max_count, line);
	}
	// 获取当前HEAD
	string current_id =
		FileSystemUtils::getInstance().readText(FileSystemUtils::getInstance().headPath());
	if (current_id.empty()) {
		cout << "No commits yet.\n";
		return 0;
	}

	// 遍历提交历史
	vector<Commit> commit_history;
	string commit_id = current_id;
	int count = 0;

	while (!commit_id.empty() && (max_count == -1 || count < max_count)) {
		auto commit_opt = CommitManager::loadCommit(commit_id);
		if (!commit_opt) {
			cerr << "Warning: Cannot load commit " << commit_id << "\n";
			break;
		}

		commit_history.push_back(*commit_opt);
		commit_id = commit_opt->parent;
		count++;
	}

	// 显示提交历史
	if (commit_history.empty()) {
		cout << "No commits to show.\n";
		return 0;
	}

	for (const auto &commit : commit_history) {
		if (line) {
			// 单行格式：commit_hash message
			cout << commit.id.substr(0, 12) << " " << commit.message << "\n";
		} else {
			// 详细格式
			cout << "commit " << commit.id << "\n";
			if (!commit.parent.empty()) {
				cout << "Parent: " << commit.parent.substr(0, 12) << "\n";
			}
			cout << "Date: " << commit.timestamp << "\n";
			cout << "\n    " << commit.message << "\n";

			// 计算并显示文件变更（包括删除的文件）
			showCommitChanges(commit);
			cout << "\n";
		}
	}

	return 0;
}

int CommandsHistory::diff(vector<string> args) {
	FileSystemUtils::getInstance().ensureRepo();

	// 解析参数
	bool name_only = false;
	bool cached = false; // diff --cached 显示暂存区与HEAD的差异

	for (size_t i = 0; i < args.size(); ++i) {
		if (args[i] == "--name-only") {
			name_only = true;
		} else if (args[i] == "--cached" || args[i] == "--staged") {
			cached = true;
		}
	}

	if (cached) {
		// 显示暂存区与HEAD的差异
		cout << "Changes staged for commit:\n";
		auto index = Index::read();
		if (index.empty()) {
			cout << "No changes staged.\n";
			return 0;
		}

		for (const auto &kv : index) {
			if (name_only) {
				cout << kv.first << "\n";
			} else {
				cout << "A  " << kv.first << " (" << kv.second.substr(0, 12) << ")\n";
			}
		}
	} else {
		// 显示工作目录与暂存区的差异
		auto working_status = getWorkingDirectoryStatus();
		if (working_status.empty()) {
			cout << "No changes in working directory.\n";
			return 0;
		}

		printf("Changes in working directory:\n");
		for (const auto &file_status : working_status) {
			if (name_only) {
				cout << file_status.path << "\n";
			} else {
				cout << file_status.status << "  " << file_status.path;
				if (file_status.status == "M") {
					cout << " (staged: " << file_status.staged_hash.substr(0, 12)
						 << ", working: " << file_status.working_hash.substr(0, 12) << ")";
				} else if (file_status.status == "D") {
					cout << " (deleted from working directory)";
				} else if (file_status.status == "??") {
					cout << " (untracked, hash: " << file_status.working_hash.substr(0, 12) << ")";
				} else if (file_status.status == "A") {
					cout << " (new file staged)";
				} else if (file_status.status == "AM") {
					cout << " (new file staged, then modified)";
				} else if (file_status.status == "AD") {
					cout << " (new file staged, then deleted - staged hash: "
						 << file_status.staged_hash.substr(0, 12) << ")";
				} else if (file_status.status == "MD") {
					cout << " (modified in stage, then deleted)";
				} else if (file_status.status == "MM") {
					cout << " (modified in stage and working directory)";
				}
				cout << "\n";
			}
		}
	}

	return 0;
}

// Helper functions implementation
string CommandsHistory::calculateWorkingFileHash(const fs::path &file_path) {
	if (!fs::exists(file_path) || !fs::is_regular_file(file_path)) {
		return ""; // 文件不存在或不是常规文件
	}
	return sha256_file(file_path);
}

void CommandsHistory::scanWorkingDirectory(const fs::path &dir, vector<string> &files) {
	try {
		for (auto &entry :
			 fs::recursive_directory_iterator(dir, fs::directory_options::skip_permission_denied)) {
			try {
				bool irf = entry.is_regular_file();
				if (irf && !FileSystemUtils::getInstance().isIgnored(entry.path())) {
					string relative_path =
						fs::relative(entry.path(), FileSystemUtils::getInstance().repoRoot())
							.generic_string();
					files.push_back(relative_path);
				}
			} catch (const fs::filesystem_error &) {
				// 静默忽略单个文件的错误（比如权限问题）
				continue;
			}
		}
	} catch (const fs::filesystem_error &) {
		// 静默忽略文件系统错误（比如权限问题）
	}
}

// 获取历史提交中所有曾经存在的文件（用于检测意外删除）
Index::IndexMap CommandsHistory::getHistoricalFiles(const string &head_commit_id) {
	Index::IndexMap historical_files;

	if (head_commit_id.empty()) {
		return historical_files; // 空仓库，没有历史文件
	}

	// 遍历所有历史提交，收集所有曾经存在的文件
	string current_commit_id = head_commit_id;
	set<string> visited_commits; // 防止无限循环

	while (!current_commit_id.empty() &&
		   visited_commits.find(current_commit_id) == visited_commits.end()) {
		visited_commits.insert(current_commit_id);

		auto commit_opt = CommitManager::loadCommit(current_commit_id);
		if (!commit_opt) {
			break; // 无法加载提交，停止遍历
		}

		// 将当前提交中的所有文件添加到历史文件集合中
		for (const auto &file_entry : commit_opt->tree) {
			const string &file_path = file_entry.first;
			const string &file_hash = file_entry.second;

			// 如果该文件尚未被记录，或者当前提交中的版本更新，则保存
			if (historical_files.find(file_path) == historical_files.end()) {
				historical_files[file_path] = file_hash;
			}
		}

		// 移动到父提交
		current_commit_id = commit_opt->parent;
	}

	return historical_files;
}

// 显示提交的文件变更（包括新增、修改和删除）
void CommandsHistory::showCommitChanges(const Commit &commit) {
	Index::IndexMap parent_files;

	// 获取父提交的文件列表
	if (!commit.parent.empty()) {
		auto parent_commit = CommitManager::loadCommit(commit.parent);
		if (parent_commit) {
			parent_files = parent_commit->tree;
		}
	}

	vector<pair<string, string>> added_files;	 // 新增的文件
	vector<pair<string, string>> modified_files; // 修改的文件
	vector<string> deleted_files;				 // 删除的文件

	// 检查当前提交中的文件（新增或修改）
	for (const auto &file : commit.tree) {
		const string &file_path = file.first;
		const string &file_hash = file.second;

		auto parent_it = parent_files.find(file_path);
		if (parent_it == parent_files.end()) {
			// 文件在父提交中不存在，是新增的
			added_files.push_back({file_path, file_hash});
		} else if (parent_it->second != file_hash) {
			// 文件在父提交中存在但哈希不同，是修改的
			modified_files.push_back({file_path, file_hash});
		}
		// 如果哈希相同，则文件未变更，不显示
	}

	// 检查父提交中的文件是否被删除
	for (const auto &parent_file : parent_files) {
		const string &file_path = parent_file.first;

		if (commit.tree.find(file_path) == commit.tree.end()) {
			// 文件在当前提交中不存在，被删除了
			deleted_files.push_back(file_path);
		}
	}

	// 计算总变更数
	size_t total_changes = added_files.size() + modified_files.size() + deleted_files.size();

	cout << "\n    Files changed: " << total_changes;
	if (added_files.size() > 0)
		cout << " (+" << added_files.size() << " added)";
	if (modified_files.size() > 0)
		cout << " (~" << modified_files.size() << " modified)";
	if (deleted_files.size() > 0)
		cout << " (-" << deleted_files.size() << " deleted)";
	cout << "\n";

	// 显示新增的文件
	for (const auto &file : added_files) {
		cout << "        + " << file.first << " (" << file.second.substr(0, 12) << ")\n";
	}

	// 显示修改的文件
	for (const auto &file : modified_files) {
		cout << "        ~ " << file.first << " (" << file.second.substr(0, 12) << ")\n";
	}

	// 显示删除的文件
	for (const string &file_path : deleted_files) {
		cout << "        - " << file_path << " (deleted)\n";
	}
}

vector<CommandsHistory::FileStatus> CommandsHistory::getWorkingDirectoryStatus() {
	vector<FileStatus> statuses;
	auto index = Index::read();

	// 获取当前HEAD提交中的文件
	Index::IndexMap head_files;
	string head_commit_id =
		FileSystemUtils::getInstance().readText(FileSystemUtils::getInstance().headPath());
	if (!head_commit_id.empty()) {
		auto head_commit = CommitManager::loadCommit(head_commit_id);
		if (head_commit) {
			head_files = head_commit->tree;
		}
	}

	// 获取工作目录中的所有文件
	vector<string> working_files;
	scanWorkingDirectory(FileSystemUtils::getInstance().repoRoot(), working_files);

	// 创建所有文件的集合（HEAD提交 + 暂存区 + 工作目录）
	set<string> all_files;
	for (const auto &kv : head_files) {
		all_files.insert(kv.first);
	}
	for (const auto &kv : index) {
		all_files.insert(kv.first);
	}
	for (const auto &file : working_files) {
		all_files.insert(file);
	}

	// 分析每个文件的状态
	for (const string &file_path : all_files) {
		FileStatus status;
		status.path = file_path;

		fs::path full_path = FileSystemUtils::getInstance().repoRoot() / file_path;
		bool exists_in_working = fs::exists(full_path);
		bool exists_in_index = index.find(file_path) != index.end();
		bool exists_in_head = head_files.find(file_path) != head_files.end();

		string head_hash;
		if (exists_in_head) {
			head_hash = head_files[file_path];
		}

		string staged_hash;
		if (exists_in_index) {
			staged_hash = index[file_path];
			status.staged_hash = staged_hash;
		}

		string working_hash;
		if (exists_in_working) {
			working_hash = calculateWorkingFileHash(full_path);
			status.working_hash = working_hash;
		}

		// 决定文件状态 - 使用更清晰的条件判断，包括历史文件检测
		if (!exists_in_head && !exists_in_index && exists_in_working) {
			// 情况1: 全新文件（未跟踪）
			status.status = "??";
		} else if (exists_in_head && !exists_in_index && !exists_in_working) {
			// 情况2: 在HEAD中存在，但已从工作目录删除，且未暂存删除操作
			status.status = "D";
			status.staged_hash = head_hash; // 保存HEAD中的哈希用于对比
		} else if (exists_in_head && exists_in_index && !exists_in_working) {
			// 情况3: 在HEAD和暂存区中存在，但已从工作目录删除
			if (head_hash == staged_hash) {
				// 暂存区中是删除操作
				status.status = "D";
			} else {
				// 暂存区中有修改，但工作目录中删除了
				status.status = "MD"; // 修改后删除
			}
		} else if (!exists_in_head && exists_in_index && !exists_in_working) {
			// 情况4: 暂存了新文件，但随后从工作目录删除
			// 这是您提到的关键场景：文件在暂存区中存在，但在HEAD和工作目录中都不存在
			status.status = "AD";			  // 添加后删除
			status.staged_hash = staged_hash; // 保留暂存区中的哈希
		} else if (!exists_in_head && exists_in_index && exists_in_working) {
			// 情况5: 新文件已暂存
			if (staged_hash == working_hash) {
				// 新文件已暂存且未修改
				status.status = "A";
			} else {
				// 新文件已暂存但随后修改
				status.status = "AM";
			}
		} else if (exists_in_head && !exists_in_index && exists_in_working) {
			// 情况6: HEAD中存在，暂存区中没有，工作目录中存在
			if (head_hash == working_hash) {
				// 文件未修改，不显示
				continue;
			} else {
				// 文件已修改但未暂存
				status.status = "M";
				status.staged_hash = head_hash; // 用HEAD作为基准
			}
		} else if (exists_in_head && exists_in_index && exists_in_working) {
			// 情况7: 三个地方都存在
			if (head_hash == staged_hash && staged_hash == working_hash) {
				// 完全相同，不显示
				continue;
			} else if (head_hash != staged_hash && staged_hash == working_hash) {
				// 已暂存修改，工作目录未变
				status.status = "M";
			} else if (head_hash == staged_hash && staged_hash != working_hash) {
				// 暂存区未变，工作目录有修改
				status.status = "M";
				status.staged_hash = head_hash;
			} else {
				// 暂存区和工作目录都有修改
				status.status = "MM"; // 双重修改
			}
		}

		statuses.push_back(status);
	}

	return statuses;
}