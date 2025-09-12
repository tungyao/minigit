#include "commands_remote.h"
#include "client.h"
#include "compression.h"
#include "progress.h"

string CommandsRemote::getRemote()
{
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

void CommandsRemote::setRemoteConfig(const string& path)
{
	string s = "remote=" + path + "\n";
	FileSystemUtils::writeText(FileSystemUtils::configPath(), s);
}

int CommandsRemote::setRemote(const string& path)
{
	FileSystemUtils::ensureRepo();
	if (path.empty())
	{
		cerr << "Usage: minigit set-remote <absolute-folder>\n";
		return 1;
	}
	fs::create_directories(fs::path(path) / "objects");
	setRemoteConfig(fs::absolute(path).string());
	cout << "Remote set to " << getRemote() << "\n";
	return 0;
}

int CommandsRemote::push(vector<string> args)
{
	FileSystemUtils::ensureRepo();
	string remote = getRemote();
	if (remote.empty())
	{
		cerr << "No remote configured. Use: minigit set-remote <folder>\n";
		return 1;
	}

	printf("remote: %s\n", remote.c_str());

	// 检查是否为网络远程仓库 (server://格式)
	if (remote.find("server://") == 0)
	{
		// 解析命令行参数
		string password;
		for (size_t i = 0; i < args.size(); ++i)
		{
			if (args[i] == "--password" && i + 1 < args.size())
			{
				password = args[i + 1];
				i++;
			}
		}

		if (password.empty())
		{
			cerr << "Network push requires --password option\n";
			cerr << "Usage: minigit push --password <password>\n";
			return 1;
		}

		// 解析网络远程地址
		NetworkRemote network_remote = parseNetworkRemote(remote);
		if (network_remote.host.empty())
		{
			cerr << "Invalid network remote format: " << remote << "\n";
			return 1;
		}

		// 执行网络 push
		return networkPush(network_remote, password);
	}

	// 原有的本地文件系统push逻辑
	fs::path r = remote;
	fs::create_directories(r / "objects");
	
	cout << "正在扫描需要推送的文件...\n";
	
	// 收集需要推送的文件
	vector<fs::path> files_to_push;
	vector<fs::path> relative_paths;
	
	// 添加HEAD文件
	if (fs::exists(FileSystemUtils::headPath())) {
		files_to_push.push_back(FileSystemUtils::headPath());
		relative_paths.push_back("HEAD");
	}
	
	// 添加对象文件
	if (fs::exists(FileSystemUtils::objectsDir())) {
		for (auto& e : fs::directory_iterator(FileSystemUtils::objectsDir())) {
			auto id = e.path().filename().string();
			if (!fs::exists(r / "objects" / id)) {
				files_to_push.push_back(e.path());
				relative_paths.push_back(fs::path("objects") / id);
			}
		}
	}
	
	if (files_to_push.empty()) {
		cout << "没有文件需要推送\n";
		return 0;
	}
	
	cout << "发现 " << files_to_push.size() << " 个文件需要推送\n";
	
	// 创建压缩归档
	vector<uint8_t> compressed_archive;
	bool compression_success = CompressionUtils::createCompressedArchive(
		relative_paths,
		FileSystemUtils::repoRoot() / ".minigit",
		compressed_archive,
		[](int progress, const string& description) {
			ProgressDisplay::showCompressionProgress(progress, "压缩", description);
		}
	);
	
	ProgressDisplay::finish();
	
	if (!compression_success) {
		cerr << "压缩失败\n";
		return 1;
	}
	
	cout << "正在传输压缩包...\n";
	
	// 计算传输进度并解压
	bool extraction_success = CompressionUtils::extractCompressedArchive(
		compressed_archive,
		r,
		[](int progress, const string& description) {
			ProgressDisplay::showCompressionProgress(progress, "解压", description);
		}
	);
	
	ProgressDisplay::finish();
	
	if (!extraction_success) {
		cerr << "解压失败\n";
		return 1;
	}
	
	string head = FileSystemUtils::readText(FileSystemUtils::headPath());
	cout << "推送完成到 " << remote << " (HEAD=" << head.substr(0, 12) << ")\n";
	cout << "传输了 " << ProgressDisplay::formatFileSize(compressed_archive.size()) 
		 << " 压缩数据，包含 " << files_to_push.size() << " 个文件\n";
	return 0;
}

int CommandsRemote::pull(vector<string> args)
{
	FileSystemUtils::ensureRepo();
	string remote = getRemote();
	if (remote.empty())
	{
		cerr << "No remote configured. Use: minigit set-remote <folder>\n";
		return 1;
	}

	// 检查是否为网络远程仓库 (server://格式)
	if (remote.find("server://") == 0)
	{
		// 解析命令行参数
		string password;
		for (size_t i = 0; i < args.size(); ++i)
		{
			if (args[i] == "--password" && i + 1 < args.size())
			{
				password = args[i + 1];
				i++;
			}
		}

		if (password.empty())
		{
			cerr << "Network pull requires --password option\n";
			cerr << "Usage: minigit pull --password <password>\n";
			return 1;
		}

		// 解析网络远程地址
		NetworkRemote network_remote = parseNetworkRemote(remote);
		if (network_remote.host.empty())
		{
			cerr << "Invalid network remote format: " << remote << "\n";
			return 1;
		}

		// 执行网络 pull
		return networkPull(network_remote, password);
	}

	// 原有的本地文件系统pull逻辑
	fs::path r = remote;
	if (!fs::exists(r))
	{
		cerr << "远程路径不存在: " << remote << "\n";
		return 1;
	}
	
	cout << "正在扫描需要拉取的文件...\n";
	
	// 收集需要拉取的文件
	vector<fs::path> files_to_pull;
	vector<fs::path> relative_paths;
	
	// 添加HEAD文件
	if (fs::exists(r / "HEAD")) {
		files_to_pull.push_back(r / "HEAD");
		relative_paths.push_back("HEAD");
	}
	
	// 添加对象文件
	if (fs::exists(r / "objects")) {
		for (auto& e : fs::directory_iterator(r / "objects")) {
			auto id = e.path().filename().string();
			if (!fs::exists(FileSystemUtils::objectsDir() / id)) {
				files_to_pull.push_back(e.path());
				relative_paths.push_back(fs::path("objects") / id);
			}
		}
	}
	
	if (files_to_pull.empty()) {
		cout << "没有文件需要拉取\n";
		return 0;
	}
	
	cout << "发现 " << files_to_pull.size() << " 个文件需要拉取\n";
	
	// 创建压缩归档
	vector<uint8_t> compressed_archive;
	bool compression_success = CompressionUtils::createCompressedArchive(
		relative_paths,
		r,
		compressed_archive,
		[](int progress, const string& description) {
			ProgressDisplay::showCompressionProgress(progress, "压缩", description);
		}
	);
	
	ProgressDisplay::finish();
	
	if (!compression_success) {
		cerr << "压缩失败\n";
		return 1;
	}
	
	cout << "正在传输压缩包...\n";
	
	// 解压到本地
	bool extraction_success = CompressionUtils::extractCompressedArchive(
		compressed_archive,
		FileSystemUtils::repoRoot() / ".minigit",
		[](int progress, const string& description) {
			ProgressDisplay::showCompressionProgress(progress, "解压", description);
		}
	);
	
	ProgressDisplay::finish();
	
	if (!extraction_success) {
		cerr << "解压失败\n";
		return 1;
	}
	
	string rhead = FileSystemUtils::readText(FileSystemUtils::headPath());
	cout << "拉取完成从 " << remote << " (HEAD=" << rhead.substr(0, 12) << ")\n";
	cout << "传输了 " << ProgressDisplay::formatFileSize(compressed_archive.size()) 
		 << " 压缩数据，包含 " << files_to_pull.size() << " 个文件\n";
	return 0;
}

CommandsRemote::NetworkRemote CommandsRemote::parseNetworkRemote(const string& remote_url)
{
	NetworkRemote remote;

	// 检查是否以 server:// 开头
	if (remote_url.find("server://") != 0)
	{
		return remote; // 返回空的NetworkRemote
	}

	// 移除 server:// 前缀
	string url_part = remote_url.substr(9); // "server://".length() = 9

	// 查找最后一个 /
	size_t slash_pos = url_part.find_last_of('/');
	if (slash_pos == string::npos)
	{
		return remote; // 格式错误
	}

	// 提取仓库名
	remote.repo_name = url_part.substr(slash_pos + 1);

	// 提取 host:port 部分
	string host_port = url_part.substr(0, slash_pos);

	// 解析 host:port
	size_t colon_pos = host_port.find(':');
	if (colon_pos != string::npos)
	{
		remote.host = host_port.substr(0, colon_pos);
		try
		{
			remote.port = stoi(host_port.substr(colon_pos + 1));
		}
		catch (const exception&)
		{
			remote.port = 8080; // 默认端口
		}
	}
	else
	{
		remote.host = host_port;
		remote.port = 8080; // 默认端口
	}

	return remote;
}

// 执行网络 push 操作
int CommandsRemote::networkPush(const NetworkRemote& remote, const string& password)
{
	cout << "Connecting to " << remote.host << ":" << remote.port << "...\n";

	// 创建客户端配置
	Config::getInstance().server_host = remote.host;
	Config::getInstance().server_port = remote.port;
	Config::getInstance().password = password;

	// 创建客户端并连接
	Client client;

	if (!client.connect())
	{
		cerr << "Failed to connect to server\n";
		return 1;
	}

	if (!client.authenticate())
	{
		cerr << "Authentication failed\n";
		return 1;
	}

	// 使用指定的仓库
	if (!client.useRepository(remote.repo_name))
	{
		cerr << "Failed to use repository: " << remote.repo_name << "\n";
		return 1;
	}

	// 执行 push
	if (!client.push())
	{
		cerr << "Push failed\n";
		return 1;
	}

	cout << "Push completed successfully!\n";
	return 0;
}

// 执行网络 pull 操作
int CommandsRemote::networkPull(const NetworkRemote& remote, const string& password)
{
	cout << "Connecting to " << remote.host << ":" << remote.port << "...\n";

	// 创建客户端配置
	Config::getInstance().server_host = remote.host;
	Config::getInstance().server_port = remote.port;
	Config::getInstance().password = password;

	// 创建客户端并连接
	Client client;

	if (!client.connect())
	{
		cerr << "Failed to connect to server\n";
		return 1;
	}

	if (!client.authenticate())
	{
		cerr << "Authentication failed\n";
		return 1;
	}

	// 使用指定的仓库
	if (!client.useRepository(remote.repo_name))
	{
		cerr << "Failed to use repository: " << remote.repo_name << "\n";
		return 1;
	}

	// 执行 pull
	if (!client.pull())
	{
		cerr << "Pull failed\n";
		return 1;
	}

	cout << "Pull completed successfully!\n";
	return 0;
}

int CommandsRemote::networkLog(const NetworkRemote& remote, const string& password, int max_count, bool line)
{
	cout << "Connecting to " << remote.host << ":" << remote.port << "...\n";

	// 创建客户端配置
	Config::getInstance().server_host = remote.host;
	Config::getInstance().server_port = remote.port;
	Config::getInstance().password = password;

	// 创建客户端并连接
	Client client;

	if (!client.connect())
	{
		cerr << "Failed to connect to server\n";
		return 1;
	}

	if (!client.authenticate())
	{
		cerr << "Authentication failed\n";
		return 1;
	}

	// 使用指定的仓库
	if (!client.useRepository(remote.repo_name))
	{
		cerr << "Failed to use repository: " << remote.repo_name << "\n";
		return 1;
	}

	// 执行 log
	auto logs = client.log(max_count, line);
	if (logs.size() == 0) {
		cout << "no commits" << endl;
	}

	for (const auto& log_entry : logs)
	{
		cout << log_entry << "\n";
	}

	return 0;
}