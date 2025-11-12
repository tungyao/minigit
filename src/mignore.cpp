#include "mignore.h"
#include "common.h"

namespace fs = std::filesystem;
std::vector<std::string> load_ignore() {
	std::ifstream ignore_file(".mignore");
	if (!ignore_file) {
		return {}; // 文件不存在，不忽略任何路径
	}

	std::string line;
	std::vector<std::string> rules;
	// 解析 .mignore 文件
	while (std::getline(ignore_file, line)) {
		// 跳过空行和注释
		size_t start = line.find_first_not_of(" \t");
		if (start == std::string::npos)
			continue; // 空行
		if (line[start] == '#')
			continue; // 注释行

		// 去除行尾空白
		size_t end = line.find_last_not_of(" \t");
		line = line.substr(start, end - start + 1);
		rules.push_back(line);
	}
	return rules;
}
bool should_ignore(std::vector<std::string> rules, const fs::path &path) {
	// 读取 .mignore 文件

	// 将路径转换为统一的 / 分隔符（跨平台处理）
	std::string path_str = path.string();
	std::replace(path_str.begin(), path_str.end(), '\\', '/');

	// 检查路径是否匹配任何规则
	for (const auto &rule : rules) {
		if (match_pattern(path_str, rule)) {
			std::cout << "ignore file " << path_str << std::endl;
			return true;
		}
	}
	return false;
}

// 辅助函数：匹配路径与规则
bool match_pattern(const std::string &path, const std::string &pattern) {
	// 规则以 * 或 / 结尾时，视为通配符（匹配任意后缀）
	bool ends_with_star = (pattern.back() == '*' || pattern.back() == '/');

	// 分割规则为通配符片段
	std::vector<std::string> tokens;
	std::string token;
	for (char c : pattern) {
		if (c == '*') {
			tokens.push_back(token);
			token.clear();
		} else {
			token += c;
		}
	}
	tokens.push_back(token);

	size_t start = 0;
	for (const auto &token : tokens) {
		if (token.empty())
			continue; // 跳过空片段（连续 * 的情况）

		size_t pos = path.find(token, start);
		if (pos == std::string::npos) {
			return false;
		}
		start = pos + token.size();
	}

	// 如果规则未以 * 或 / 结尾，必须匹配到路径末尾
	if (!ends_with_star && start != path.size()) {
		return false;
	}
	return true;
}