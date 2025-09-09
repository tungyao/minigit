#include "commands.h"
#include "client.h"

int Commands::init() {
	if (fs::exists(FileSystemUtils::mgDir())) {
		cerr << "Already initialized.\n";
		return 1;
	}
	fs::create_directories(FileSystemUtils::objectsDir());
	FileSystemUtils::writeText(FileSystemUtils::indexPath(), "");
	FileSystemUtils::writeText(FileSystemUtils::headPath(), "");
	FileSystemUtils::writeText(FileSystemUtils::configPath(), "remote=\n");
	cout << "Initialized empty MiniGit repository in " << FileSystemUtils::mgDir() << "\n";
	return 0;
}

int Commands::add(vector<string> args) {
	FileSystemUtils::ensureRepo();
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
	for (const auto& file_status : working_status) {
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
	for (auto& a : args) {
		fs::path p = a;
		if (!fs::exists(p)) {
			cerr << "skip missing: " << a << "\n";
			continue;
		}
		
		// 如果是目录，遍历目录下的所有文件
		if (fs::is_directory(p)) {
			vector<string> dir_files;
			scanWorkingDirectory(p, dir_files);
			
			for (const auto& file_path : dir_files) {
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
						fs::path full_path = FileSystemUtils::repoRoot() / file_path;
						Index::stagePath(full_path, idx);
						added_count++;
						cout << "rename " << old_path << " -> " << file_path << "\n";
					} else {
						// 处理修改和新增的文件
						fs::path full_path = FileSystemUtils::repoRoot() / file_path;
						Index::stagePath(full_path, idx);
						added_count++;
						cout << "add " << file_path << " (" << 
							(modified_files.count(file_path) ? "modified" : "new") << ")\n";
					}
				}
			}
		} else {
			// 单个文件处理
			string rel_path = fs::relative(p, FileSystemUtils::repoRoot()).string();
			
			// 添加修改、新增、删除和重命名的文件
			if (modified_files.count(rel_path) || untracked_files.count(rel_path) || 
			    deleted_files.count(rel_path) || renamed_files.count(rel_path)) {
				
				if (deleted_files.count(rel_path)) {
					// 处理删除的文件：从暂存区移除
					auto it = idx.find(rel_path);
					if (it != idx.end()) {
						idx.erase(it);
						added_count++;
						cout << "remove " << rel_path << " (deleted)\n";
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
					Index::stagePath(p, idx);
					added_count++;
					cout << "add " << rel_path << " (" << 
						(modified_files.count(rel_path) ? "modified" : "new") << ")\n";
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

int Commands::commit(vector<string> args) {
	FileSystemUtils::ensureRepo();
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
	c.parent = FileSystemUtils::readText(FileSystemUtils::headPath());
	c.message = msg;
	c.timestamp = CommitManager::nowISO8601();
	c.tree = idx;
	string id = CommitManager::storeCommit(c);

	// 清空暂存区 - 提交后暂存区应该为空
	Index::IndexMap empty_index;
	Index::write(empty_index);

	cout << "Committed " << id.substr(0, 12) << " - " << msg << "\n";
	return 0;
}

string Commands::getRemote() {
	FileSystemUtils::ensureRepo();
	string cfg = FileSystemUtils::readText(FileSystemUtils::configPath());
	string key = "remote=";
	auto p = cfg.find(key);
	if (p == string::npos)
		return string("");
	string v = cfg.substr(p + key.size());
	while (!v.empty() && (v.back() == '\n' || v.back() == '\r'))
		v.pop_back();
	return v;
}

void Commands::setRemoteConfig(const string& path) {
	string s = "remote=" + path + "\n";
	FileSystemUtils::writeText(FileSystemUtils::configPath(), s);
}

int Commands::setRemote(const string& path) {
	FileSystemUtils::ensureRepo();
	if (path.empty()) {
		cerr << "Usage: minigit set-remote <absolute-folder>\n";
		return 1;
	}
	fs::create_directories(fs::path(path) / "objects");
	setRemoteConfig(fs::absolute(path).string());
	cout << "Remote set to " << getRemote() << "\n";
	return 0;
}

int Commands::push(vector<string> args) {
	FileSystemUtils::ensureRepo();
	string remote = getRemote();
	if (remote.empty()) {
		cerr << "No remote configured. Use: minigit set-remote <folder>\n";
		return 1;
	}

	printf("remote: %s\n", remote.c_str());

	// 检查是否为网络远程仓库 (server://格式)
	if (remote.find("server://") == 0) {
		// 解析命令行参数
		string password;
		for (size_t i = 0; i < args.size(); ++i) {
			if (args[i] == "--password" && i + 1 < args.size()) {
				password = args[i + 1];
				i++;
			}
		}

		if (password.empty()) {
			cerr << "Network push requires --password option\n";
			cerr << "Usage: minigit push --password <password>\n";
			return 1;
		}

		// 解析网络远程地址
		NetworkRemote network_remote = parseNetworkRemote(remote);
		if (network_remote.host.empty()) {
			cerr << "Invalid network remote format: " << remote << "\n";
			return 1;
		}

		// 执行网络 push
		return networkPush(network_remote, password);
	}

	// 原有的本地文件系统push逻辑
	fs::path r = remote;
	fs::create_directories(r / "objects");
	// copy all local objects not present remotely
	for (auto& e : fs::directory_iterator(FileSystemUtils::objectsDir())) {
		auto id = e.path().filename().string();
		if (!fs::exists(r / "objects" / id))
			fs::copy_file(e.path(), r / "objects" / id,
				fs::copy_options::overwrite_existing);
	}
	// update remote HEAD
	string head = FileSystemUtils::readText(FileSystemUtils::headPath());
	FileSystemUtils::writeText(r / "HEAD", head);
	cout << "Pushed to " << remote << " (HEAD=" << head.substr(0, 12) << ")\n";
	return 0;
}

int Commands::pull(vector<string> args) {
	FileSystemUtils::ensureRepo();
	string remote = getRemote();
	if (remote.empty()) {
		cerr << "No remote configured. Use: minigit set-remote <folder>\n";
		return 1;
	}

	// 检查是否为网络远程仓库 (server://格式)
	if (remote.find("server://") == 0) {
		// 解析命令行参数
		string password;
		for (size_t i = 0; i < args.size(); ++i) {
			if (args[i] == "--password" && i + 1 < args.size()) {
				password = args[i + 1];
				i++;
			}
		}

		if (password.empty()) {
			cerr << "Network pull requires --password option\n";
			cerr << "Usage: minigit pull --password <password>\n";
			return 1;
		}

		// 解析网络远程地址
		NetworkRemote network_remote = parseNetworkRemote(remote);
		if (network_remote.host.empty()) {
			cerr << "Invalid network remote format: " << remote << "\n";
			return 1;
		}

		// 执行网络 pull
		return networkPull(network_remote, password);
	}

	// 原有的本地文件系统pull逻辑
	fs::path r = remote;
	if (!fs::exists(r)) {
		cerr << "Remote path missing." << "\n";
		return 1;
	}
	// copy objects from remote
	for (auto& e : fs::directory_iterator(r / "objects")) {
		auto id = e.path().filename().string();
		if (!fs::exists(FileSystemUtils::objectsDir() / id))
			fs::copy_file(e.path(), FileSystemUtils::objectsDir() / id,
				fs::copy_options::overwrite_existing);
	}
	// update local HEAD to remote's HEAD
	string rhead = FileSystemUtils::readText(r / "HEAD");
	if (!rhead.empty())
		FileSystemUtils::writeText(FileSystemUtils::headPath(), rhead);
	cout << "Pulled from " << remote << " (HEAD=" << rhead.substr(0, 12) << ")\n";
	return 0;
}

int Commands::status() {
	FileSystemUtils::ensureRepo();
	string head = FileSystemUtils::readText(FileSystemUtils::headPath());
	cout << "On commit: "
		<< (head.empty() ? string("(none)") : head.substr(0, 12)) << "\n";

	auto idx = Index::read();
	cout << "Staged files: " << idx.size() << "\n";
	for (auto& kv : idx)
		cout << "  " << kv.first << " -> " << kv.second.substr(0, 12) << "\n";

	// 显示工作目录状态
	auto working_status = getWorkingDirectoryStatus();
	if (!working_status.empty()) {
		cout << "\nWorking directory changes:\n";
		for (const auto& file_status : working_status) {
			cout << "  " << file_status.status << " " << file_status.path;
			if (file_status.status == "M") {
				cout << " (modified)";
			}
			else if (file_status.status == "D") {
				cout << " (deleted)";
			}
			else if (file_status.status == "??") {
				cout << " (untracked)";
			}
			cout << "\n";
		}
	}
	else {
		cout << "\nWorking directory clean.\n";
	}

	return 0;
}

int Commands::checkout() {
	FileSystemUtils::ensureRepo();
	string head = FileSystemUtils::readText(FileSystemUtils::headPath());
	if (head.empty()) {
		cerr << "No commit to checkout.\n";
		return 1;
	}
	auto oc = CommitManager::loadCommit(head);
	if (!oc) {
		cerr << "HEAD object missing." << "\n";
		return 1;
	}
	// write files to working dir
	for (auto& kv : oc->tree) {
		fs::path out = FileSystemUtils::repoRoot() / kv.first;
		fs::create_directories(out.parent_path());
		fs::copy_file(FileSystemUtils::objectsDir() / kv.second, out,
			fs::copy_options::overwrite_existing);
	}
	cout << "Checked out commit " << head.substr(0, 12) << "\n";
	return 0;
}

int Commands::reset(vector<string> args) {
	FileSystemUtils::ensureRepo();

	// Parse arguments
	string mode = "mixed"; // default mode
	string target_commit;

	for (size_t i = 0; i < args.size(); ++i) {
		if (args[i] == "--soft") {
			mode = "soft";
		}
		else if (args[i] == "--mixed") {
			mode = "mixed";
		}
		else if (args[i] == "--hard") {
			mode = "hard";
		}
		else {
			// This should be the commit hash
			target_commit = args[i];
		}
	}

	// If no commit specified, use HEAD
	if (target_commit.empty()) {
		target_commit = FileSystemUtils::readText(FileSystemUtils::headPath());
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
	FileSystemUtils::writeText(FileSystemUtils::headPath(), target_commit);

	// Step 2: Update index if mixed or hard mode
	if (mode == "mixed" || mode == "hard") {
		// Clear current index and set it to target commit's tree
		Index::write(target->tree);
		cout << "Updated index to match commit " << target_commit.substr(0, 12) << "\n";
	}

	// Step 3: Update working directory if hard mode
	if (mode == "hard") {
		// Remove all tracked files from working directory
		auto current_index = Index::read();
		for (auto& kv : current_index) {
			fs::path file_path = FileSystemUtils::repoRoot() / kv.first;
			if (fs::exists(file_path)) {
				try {
					fs::remove(file_path);
				}
				catch (const exception& e) {
					cerr << "Warning: Could not remove " << kv.first << ": " << e.what() << "\n";
				}
			}
		}

		// Write files from target commit to working directory
		for (auto& kv : target->tree) {
			fs::path out = FileSystemUtils::repoRoot() / kv.first;
			fs::create_directories(out.parent_path());
			try {
				fs::copy_file(FileSystemUtils::objectsDir() / kv.second, out,
					fs::copy_options::overwrite_existing);
			}
			catch (const exception& e) {
				cerr << "Warning: Could not restore " << kv.first << ": " << e.what() << "\n";
			}
		}
		cout << "Updated working directory to match commit " << target_commit.substr(0, 12) << "\n";
	}

	cout << "Reset complete.\n";
	return 0;
}

int Commands::log(vector<string> args) {
	FileSystemUtils::ensureRepo();

	// 解析参数
	int max_count = -1; // -1表示显示所有提交
	bool oneline = false;

	for (size_t i = 0; i < args.size(); ++i) {
		if (args[i] == "--oneline") {
			oneline = true;
		}
		else if (args[i].substr(0, 2) == "-n" && i + 1 < args.size()) {
			try {
				max_count = stoi(args[i + 1]);
				i++; // 跳过下一个参数
			}
			catch (const exception&) {
				cerr << "Invalid number: " << args[i + 1] << "\n";
				return 1;
			}
		}
		else if (args[i].substr(0, 8) == "--max-count=" && args[i].length() > 8) {
			try {
				max_count = stoi(args[i].substr(12));
			}
			catch (const exception&) {
				cerr << "Invalid number in --max-count: " << args[i] << "\n";
				return 1;
			}
		}
	}

	// 获取当前HEAD
	string current_id = FileSystemUtils::readText(FileSystemUtils::headPath());
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

	for (const auto& commit : commit_history) {
		if (oneline) {
			// 单行格式：commit_hash message
			cout << commit.id.substr(0, 12) << " " << commit.message << "\n";
		}
		else {
			// 详细格式
			cout << "commit " << commit.id << "\n";
			if (!commit.parent.empty()) {
				cout << "Parent: " << commit.parent.substr(0, 12) << "\n";
			}
			cout << "Date: " << commit.timestamp << "\n";
			cout << "\n    " << commit.message << "\n";

			// 显示文件变更统计
			cout << "\n    Files changed: " << commit.tree.size() << "\n";
			for (const auto& file : commit.tree) {
				cout << "        " << file.first << " (" << file.second.substr(0, 12) << ")\n";
			}
			cout << "\n";
		}
	}

	return 0;
}

// 文件状态检测辅助方法实现

string Commands::calculateWorkingFileHash(const fs::path& file_path) {
	if (!fs::exists(file_path) || !fs::is_regular_file(file_path)) {
		return ""; // 文件不存在或不是常规文件
	}
	return sha256_file(file_path);
}

void Commands::scanWorkingDirectory(const fs::path& dir, vector<string>& files) {
	try {
		for (auto& entry : fs::recursive_directory_iterator(dir, 
			fs::directory_options::skip_permission_denied)) {
			try {
				if (entry.is_regular_file() && !FileSystemUtils::isIgnored(entry.path())) {
					string relative_path = fs::relative(entry.path(), FileSystemUtils::repoRoot()).generic_string();
					files.push_back(relative_path);
				}
			}
			catch (const fs::filesystem_error&) {
				// 静默忽略单个文件的错误（比如权限问题）
				continue;
			}
		}
	}
	catch (const fs::filesystem_error&) {
		// 静默忽略文件系统错误（比如权限问题）
	}
}

vector<Commands::FileStatus> Commands::getWorkingDirectoryStatus() {
	vector<FileStatus> statuses;
	auto index = Index::read();
	
	// 获取当前HEAD提交中的文件
	Index::IndexMap head_files;
	string head_commit_id = FileSystemUtils::readText(FileSystemUtils::headPath());
	if (!head_commit_id.empty()) {
		auto head_commit = CommitManager::loadCommit(head_commit_id);
		if (head_commit) {
			head_files = head_commit->tree;
		}
	}

	// 获取工作目录中的所有文件
	vector<string> working_files;
	scanWorkingDirectory(FileSystemUtils::repoRoot(), working_files);

	// 创建所有文件的集合（HEAD提交 + 暂存区 + 工作目录）
	set<string> all_files;
	for (const auto& kv : head_files) {
		all_files.insert(kv.first);
	}
	for (const auto& kv : index) {
		all_files.insert(kv.first);
	}
	for (const auto& file : working_files) {
		all_files.insert(file);
	}

	// 分析每个文件的状态
	for (const string& file_path : all_files) {
		FileStatus status;
		status.path = file_path;

		fs::path full_path = FileSystemUtils::repoRoot() / file_path;
		bool exists_in_working = fs::exists(full_path);
		bool exists_in_index = index.find(file_path) != index.end();
		bool exists_in_head = head_files.find(file_path) != head_files.end();

		if (exists_in_index) {
			status.staged_hash = index[file_path];
		}
		
		string head_hash;
		if (exists_in_head) {
			head_hash = head_files[file_path];
		}

		if (exists_in_working) {
			status.working_hash = calculateWorkingFileHash(full_path);
		}

		// 决定文件状态
		if (!exists_in_head && !exists_in_index && exists_in_working) {
			// 全新文件（未跟踪）
			status.status = "??";
		}
		else if (exists_in_head && !exists_in_working) {
			// 已删除的文件
			status.status = "D";
			status.staged_hash = head_hash; // 使用HEAD中的哈希作为基准
		}
		else if (exists_in_head && exists_in_working) {
			if (head_hash != status.working_hash) {
				// 已修改的文件
				status.status = "M";
				status.staged_hash = head_hash; // 使用HEAD中的哈希作为基准
			}
			else {
				// 未修改的文件（不显示）
				continue;
			}
		}
		else if (!exists_in_head && exists_in_index && exists_in_working) {
			// 暂存区中的新文件
			if (status.staged_hash != status.working_hash) {
				// 暂存后又修改的文件
				status.status = "M";
			}
			else {
				// 暂存区中的新文件（未修改）
				continue;
			}
		}

		statuses.push_back(status);
	}

	// 检测重命名
	detectRenames(statuses);

	return statuses;
}

int Commands::diff(vector<string> args) {
	FileSystemUtils::ensureRepo();

	// 解析参数
	bool name_only = false;
	bool cached = false; // diff --cached 显示暂存区与HEAD的差异

	for (size_t i = 0; i < args.size(); ++i) {
		if (args[i] == "--name-only") {
			name_only = true;
		}
		else if (args[i] == "--cached" || args[i] == "--staged") {
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

		for (const auto& kv : index) {
			if (name_only) {
				cout << kv.first << "\n";
			}
			else {
				cout << "A  " << kv.first << " (" << kv.second.substr(0, 12) << ")\n";
			}
		}
	}
	else {
		// 显示工作目录与暂存区的差异
		auto working_status = getWorkingDirectoryStatus();
		if (working_status.empty()) {
			cout << "No changes in working directory.\n";
			return 0;
		}

		cout << "Changes in working directory:\n";
		for (const auto& file_status : working_status) {
			if (name_only) {
				cout << file_status.path << "\n";
			}
			else {
				cout << file_status.status << "  " << file_status.path;
				if (file_status.status == "M") {
					cout << " (staged: " << file_status.staged_hash.substr(0, 12)
						<< ", working: " << file_status.working_hash.substr(0, 12) << ")";
				}
				else if (file_status.status == "D") {
					cout << " (deleted from working directory)";
				}
				else if (file_status.status == "??") {
					cout << " (untracked, hash: " << file_status.working_hash.substr(0, 12) << ")";
				}
				cout << "\n";
			}
		}
	}

	return 0;
}

// 网络远程操作辅助方法

// 解析网络远程地址 server://host:port/repo
Commands::NetworkRemote Commands::parseNetworkRemote(const string& remote_url) {
	NetworkRemote remote;

	// 检查是否以 server:// 开头
	if (remote_url.find("server://") != 0) {
		return remote; // 返回空的NetworkRemote
	}

	// 移除 server:// 前缀
	string url_part = remote_url.substr(9); // "server://".length() = 9

	// 查找最后一个 /
	size_t slash_pos = url_part.find_last_of('/');
	if (slash_pos == string::npos) {
		return remote; // 格式错误
	}

	// 提取仓库名
	remote.repo_name = url_part.substr(slash_pos + 1);

	// 提取 host:port 部分
	string host_port = url_part.substr(0, slash_pos);

	// 解析 host:port
	size_t colon_pos = host_port.find(':');
	if (colon_pos != string::npos) {
		remote.host = host_port.substr(0, colon_pos);
		try {
			remote.port = stoi(host_port.substr(colon_pos + 1));
		}
		catch (const exception&) {
			remote.port = 8080; // 默认端口
		}
	}
	else {
		remote.host = host_port;
		remote.port = 8080; // 默认端口
	}

	return remote;
}

// 执行网络 push 操作
int Commands::networkPush(const NetworkRemote& remote, const string& password) {
	cout << "Connecting to " << remote.host << ":" << remote.port << "...\n";

	// 创建客户端配置
	Client::Config config;
	config.server_host = remote.host;
	config.server_port = remote.port;
	config.password = password;

	// 创建客户端并连接
	Client client(config);

	if (!client.connect()) {
		cerr << "Failed to connect to server\n";
		return 1;
	}

	if (!client.authenticate()) {
		cerr << "Authentication failed\n";
		return 1;
	}

	// 使用指定的仓库
	if (!client.useRepository(remote.repo_name)) {
		cerr << "Failed to use repository: " << remote.repo_name << "\n";
		return 1;
	}

	// 执行 push
	if (!client.push()) {
		cerr << "Push failed\n";
		return 1;
	}

	cout << "Push completed successfully!\n";
	return 0;
}

// 执行网络 pull 操作
int Commands::networkPull(const NetworkRemote& remote, const string& password) {
	cout << "Connecting to " << remote.host << ":" << remote.port << "...\n";

	// 创建客户端配置
	Client::Config config;
	config.server_host = remote.host;
	config.server_port = remote.port;
	config.password = password;

	// 创建客户端并连接
	Client client(config);

	if (!client.connect()) {
		cerr << "Failed to connect to server\n";
		return 1;
	}

	if (!client.authenticate()) {
		cerr << "Authentication failed\n";
		return 1;
	}

	// 使用指定的仓库
	if (!client.useRepository(remote.repo_name)) {
		cerr << "Failed to use repository: " << remote.repo_name << "\n";
		return 1;
	}

	// 执行 pull
	if (!client.pull()) {
		cerr << "Pull failed\n";
		return 1;
	}

	cout << "Pull completed successfully!\n";
	return 0;
}

// 重命名检测实现
void Commands::detectRenames(vector<FileStatus>& statuses) {
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
		const auto& deleted_file = statuses[del_idx];
		if (deleted_file.staged_hash.empty()) continue;
		
		for (size_t new_idx : new_indices) {
			auto& new_file = statuses[new_idx];
			if (new_file.working_hash.empty()) continue;
			
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
	statuses.erase(
		remove_if(statuses.begin(), statuses.end(), 
			[](const FileStatus& fs) { return fs.status.empty(); }),
		statuses.end()
	);
}