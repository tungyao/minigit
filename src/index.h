#pragma once

#include "common.h"
#include "filesystem_utils.h"

/**
 * 暂存区(Index)管理类
 * 管理文件路径到blob SHA的映射
 */
class Index {
public:
	using IndexMap = map<string, string>;

	// 读取和写入暂存区
	static IndexMap read();
	static void write(const IndexMap &index);

	// 暂存文件操作
	static void stagePath(const fs::path &p, IndexMap &idx);
};