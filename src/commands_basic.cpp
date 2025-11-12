#include "commands_basic.h"
#include "client.h"
#include "mignore.h"
#include "sha256.h"

int CommandsBasic::init() {
	if (fs::exists(FileSystemUtils::getInstance().mgDir())) {
		cerr << "Already initialized.\n";
		return 1;
	}
	fs::create_directories(FileSystemUtils::getInstance().objectsDir());
	FileSystemUtils::getInstance().writeText(FileSystemUtils::getInstance().indexPath(), "");
	FileSystemUtils::getInstance().writeText(FileSystemUtils::getInstance().headPath(), "");
	FileSystemUtils::getInstance().writeText(FileSystemUtils::getInstance().configPath(),
											 "remote=\n");
	cout << "Initialized empty MiniGit repository in " << FileSystemUtils::getInstance().mgDir()
		 << "\n";
	return 0;
}

int CommandsBasic::add(vector<string> args) {
	FileSystemUtils::getInstance().ensureRepo();
	if (args.empty()) {
		cerr << "Usage: minigit add <path> [...].\n";
		return 1;
	}

	auto idx = Index::read();

	// 获取工作目录状态，用于筛选修改过的文件
	auto working_status = getWorkingDirectoryStatus();
	set<string> modified_files;
	set<string> untracked_files;
	set<string> deleted_files;
	map<string, string> renamed_files; // new_path -> old_path

	// 分类文件状态
	for (const auto &file_status : working_status) {
		if (file_status.status == "M") {
			modified_files.insert(file_status.path);
		} else if (file_status.status == "??") {
			untracked_files.insert(file_status.path);
		} else if (file_status.status == "D") {
			deleted_files.insert(file_status.path);
		} else if (file_status.status == "R") {
			renamed_files[file_status.path] = file_status.old_path;
		}
	}

	int added_count = 0;
	auto ignore_rules = load_ignore();
	for (auto &a : args) {
		fs::path p = a;

		// 将相对路径转换为绝对路径，然后再计算相对于仓库根目录的路径
		fs::path abs_p = fs::absolute(p);
		string rel_path =
			fs::relative(abs_p, FileSystemUtils::getInstance().repoRoot()).generic_string();

		// 调试输出
		cout << "Debug: arg=" << a << ", abs_path=" << abs_p.string() << ", rel_path=" << rel_path
			 << "\n";

		// 检查文件是否在删除文件列表中（即使物理文件不存在）
		// bool is_deleted_file = deleted_files.count(rel_path) > 0;
		//
		// if (!fs::exists(abs_p) && !is_deleted_file) {
		// 	cerr << "skip missing: " << a << "\n";
		// 	continue;
		// }
		// 判断是否是忽略中的文件
		bool isIgnore = should_ignore(ignore_rules, a);
		bool isIgnore2 = should_ignore(ignore_rules, rel_path);
		if (isIgnore && isIgnore2) {
			continue;
		}
		continue;
		// 如果是目录，遍历目录下的所有文件
		if (fs::is_directory(abs_p)) {
			vector<string> dir_files;
			scanWorkingDirectory(abs_p, dir_files);

			// 添加工作目录中扫描到的文件
			set<string> files_to_process(dir_files.begin(), dir_files.end());

			// 重要：还需要包含已删除的文件
			// 检查删除的文件是否在当前处理的目录下
			string dir_relative =
				fs::relative(abs_p, FileSystemUtils::getInstance().repoRoot()).generic_string();
			for (const string &deleted_file : deleted_files) {
				// 如果删除的文件在当前目录下（或者处理根目录）
				if (dir_relative == "." || deleted_file.find(dir_relative + "/") == 0) {
					files_to_process.insert(deleted_file);
				}
			}

			for (const auto &file_path : files_to_process) {
				// 添加修改、新增、删除和重命名的文件
				if (modified_files.count(file_path) || untracked_files.count(file_path) ||
					deleted_files.count(file_path) || renamed_files.count(file_path)) {
					if (deleted_files.count(file_path)) {
						// 处理删除的文件：从暂存区移除
						auto it = idx.find(file_path);
						if (it != idx.end()) {
							idx.erase(it);
							added_count++;
							cout << "remove " << file_path << " (deleted)\n";
						}
					} else if (renamed_files.count(file_path)) {
						// 处理重命名的文件：移除旧文件，添加新文件
						string old_path = renamed_files[file_path];
						auto it = idx.find(old_path);
						if (it != idx.end()) {
							idx.erase(it);
						}
						fs::path full_path = FileSystemUtils::getInstance().repoRoot() / file_path;
						Index::stagePath(full_path, idx);
						added_count++;
						cout << "rename " << old_path << " -> " << file_path << "\n";
					} else {
						// 处理修改和新增的文件
						fs::path full_path = FileSystemUtils::getInstance().repoRoot() / file_path;
						Index::stagePath(full_path, idx);
						added_count++;
						cout << "add " << file_path << " ("
							 << (modified_files.count(file_path) ? "modified" : "new") << ")\n";
					}
				}
			}
		} else {
			// 单个文件处理
			// rel_path 已经在上面计算过了

			// 检查文件状态
			bool has_changes = modified_files.count(rel_path) || untracked_files.count(rel_path) ||
							   deleted_files.count(rel_path) || renamed_files.count(rel_path);

			if (has_changes) {
				if (deleted_files.count(rel_path)) {
					// 处理删除的文件：从暂存区移除
					auto it = idx.find(rel_path);
					if (it != idx.end()) {
						idx.erase(it);
						added_count++;
						cout << "remove " << rel_path << " (deleted)\n";
					} else {
						cout << "skip " << rel_path << " (already removed from index)\n";
					}
				} else if (renamed_files.count(rel_path)) {
					// 处理重命名的文件：移除旧文件，添加新文件
					string old_path = renamed_files[rel_path];
					auto it = idx.find(old_path);
					if (it != idx.end()) {
						idx.erase(it);
					}
					Index::stagePath(p, idx);
					added_count++;
					cout << "rename " << old_path << " -> " << rel_path << "\n";
				} else {
					// 处理修改和新增的文件
					Index::stagePath(abs_p, idx);
					added_count++;
					cout << "add " << rel_path << " ("
						 << (modified_files.count(rel_path) ? "modified" : "new") << ")\n";
				}
			} else {
				cout << "skip " << rel_path << " (unchanged)\n";
			}
		}
	}

	if (added_count == 0) {
		cout << "No modified, new, deleted, or renamed files to add.\n";
	} else {
		cout << "Processed " << added_count << " file change(s) in staging area.\n";
	}

	Index::write(idx);
	return 0;
}

int CommandsBasic::commit(vector<string> args) {
	FileSystemUtils::getInstance().ensureRepo();
	string msg;
	for (size_t i = 0; i < args.size(); ++i) {
		if (args[i] == "-m" && i + 1 < args.size()) {
			msg = args[i + 1];
			i++;
		}
	}
	if (msg.empty()) {
		cerr << "Use -m \"message\".\n";
		return 1;
	}
	auto idx = Index::read();
	if (idx.empty()) {
		cerr << "Nothing to commit.\n";
		return 1;
	}
	Commit c;
	c.parent = FileSystemUtils::getInstance().readText(FileSystemUtils::getInstance().headPath());
	c.message = msg;
	c.timestamp = CommitManager::nowISO8601();
	c.tree = idx;
	string id = CommitManager::storeCommit(c);

	// 正确的做法：提交后暂存区应该与新的HEAD提交内容一致
	// 而不是清空暂存区
	// 暂存区已经包含了正确的文件，不需要清空

	cout << "Committed " << id.substr(0, 12) << " - " << msg << "\n";
	return 0;
}

int CommandsBasic::status() {
	FileSystemUtils::getInstance().ensureRepo();
	string head =
		FileSystemUtils::getInstance().readText(FileSystemUtils::getInstance().headPath());
	cout << "On commit: " << (head.empty() ? string("(none)") : head.substr(0, 12)) << "\n";

	auto idx = Index::read();
	cout << "Staged files: " << idx.size() << "\n";
	for (auto &kv : idx)
		cout << "  " << kv.first << " -> " << kv.second.substr(0, 12) << "\n";

	// 显示工作目录状态
	auto working_status = getWorkingDirectoryStatus();
	if (!working_status.empty()) {
		cout << "\nWorking directory changes:\n";
		for (const auto &file_status : working_status) {
			cout << "  " << file_status.status << " " << file_status.path;
			if (file_status.status == "M") {
				cout << " (modified)";
			} else if (file_status.status == "D") {
				cout << " (deleted)";
			} else if (file_status.status == "??") {
				cout << " (untracked)";
			}
			cout << "\n";
		}
	} else {
		cout << "\nWorking directory clean.\n";
	}

	return 0;
}

int CommandsBasic::checkout() {
	FileSystemUtils::getInstance().ensureRepo();
	string head =
		FileSystemUtils::getInstance().readText(FileSystemUtils::getInstance().headPath());
	if (head.empty()) {
		cerr << "No commit to checkout.\n";
		return 1;
	}
	auto oc = CommitManager::loadCommit(head);
	if (!oc) {
		cerr << "HEAD object missing."
			 << "\n";
		return 1;
	}
	// write files to working dir
	for (auto &kv : oc->tree) {
		fs::path out = FileSystemUtils::getInstance().repoRoot() / kv.first;
		fs::create_directories(out.parent_path());
		fs::copy_file(FileSystemUtils::getInstance().objectsDir() / kv.second, out,
					  fs::copy_options::overwrite_existing);
	}
	cout << "Checked out commit " << head.substr(0, 12) << "\n";
	return 0;
}

// Helper functions implementation
string CommandsBasic::calculateWorkingFileHash(const fs::path &file_path) {
	if (!fs::exists(file_path) || !fs::is_regular_file(file_path)) {
		return ""; // 文件不存在或不是常规文件
	}
	return sha256_file(file_path);
}

void CommandsBasic::scanWorkingDirectory(const fs::path &dir, vector<string> &files) {
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
Index::IndexMap CommandsBasic::getHistoricalFiles(const string &head_commit_id) {
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

vector<CommandsBasic::FileStatus> CommandsBasic::getWorkingDirectoryStatus() {
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

	// remove ignore file

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

	// 检测重命名
	detectRenames(statuses);

	return statuses;
}

// 重命名检测实现
void CommandsBasic::detectRenames(vector<FileStatus> &statuses) {
	// 收集删除和新增的文件
	vector<size_t> deleted_indices;
	vector<size_t> new_indices;

	for (size_t i = 0; i < statuses.size(); ++i) {
		if (statuses[i].status == "D") {
			deleted_indices.push_back(i);
		} else if (statuses[i].status == "??") {
			new_indices.push_back(i);
		}
	}

	// 检测重命名：删除的文件哈希与新文件哈希相同
	for (size_t del_idx : deleted_indices) {
		const auto &deleted_file = statuses[del_idx];
		if (deleted_file.staged_hash.empty())
			continue;

		for (size_t new_idx : new_indices) {
			auto &new_file = statuses[new_idx];
			if (new_file.working_hash.empty())
				continue;

			// 如果哈希相同，认为是重命名
			if (deleted_file.staged_hash == new_file.working_hash) {
				// 将新文件标记为重命名
				new_file.status = "R";
				new_file.old_path = deleted_file.path;
				new_file.staged_hash = deleted_file.staged_hash;

				// 从列表中移除已删除的文件（标记为已处理）
				statuses[del_idx].status = "";
				break;
			}
		}
	}

	// 移除已处理的删除文件（status为空的）
	statuses.erase(remove_if(statuses.begin(), statuses.end(),
							 [](const FileStatus &fs) { return fs.status.empty(); }),
				   statuses.end());
}