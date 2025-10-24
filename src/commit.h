#pragma once

#include "common.h"
#include "filesystem_utils.h"
#include "sha256.h"

/**
 * 提交数据结构
 */
struct Commit {
	string id;
	string parent;
	string message;
	string timestamp;
	map<string, string> tree;
};

/**
 * 提交管理类
 * 管理提交的创建、序列化和反序列化
 */
class CommitManager {
  public:
	// 提交操作
	static string storeCommit(const Commit &c);
	static optional<Commit> loadCommit(const string &id);
	static optional<Commit> loadCommit(string repo_name, const string &id);

	// 序列化操作
	static string serializeCommit(const Commit &c);
	static Commit deserializeCommit(const string &raw);

	// 工具函数
	static string nowISO8601();
	static std::set<string> commitsCount(const string &newer, const string &older);
};