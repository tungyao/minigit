#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <cctype>
#include <algorithm>

namespace fs = std::filesystem;
extern std::vector<std::string> load_ignore();
extern bool should_ignore(std::vector<std::string> rules, const fs::path& path) ;
// 辅助函数：匹配路径与规则
extern bool match_pattern(const std::string& path, const std::string& pattern);