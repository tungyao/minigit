#pragma once

#include "common.h"
#include "filesystem_utils.h"

/**
 * MiniGit远程命令实现类
 * 实现远程相关的Git命令功能：push, pull, setRemote
 */
class CommandsRemote {
  public:
	// 远程命令
	static int push(vector<string> args = {});
	static int pull(vector<string> args = {});
	static int setRemote(const string &path);

	// 远程配置管理
	static string getRemote();
	static void setRemoteConfig(const string &path);

	// 网络远程操作
	struct NetworkRemote {
		string host;
		int port;
		string repo_name;
	};
	static NetworkRemote parseNetworkRemote(const string &remote_url);
	static int networkPush(const NetworkRemote &remote, const string &password);
	static int networkPull(const NetworkRemote &remote, const string &password);
	static int networkLog(const NetworkRemote &remote, const string &password, int max_count,
						  bool line);
};