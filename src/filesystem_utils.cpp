#include "filesystem_utils.h"

fs::path FileSystemUtils::repoRoot() {
	return fs::current_path();
}

fs::path FileSystemUtils::mgDir() {
	return repoRoot() / MARKNAME;
}

fs::path FileSystemUtils::objectsDir() {
	return mgDir() / "objects";
}

fs::path FileSystemUtils::indexPath() {
	return mgDir() / "index";
}

fs::path FileSystemUtils::headPath() {
	return mgDir() / "HEAD";
}

fs::path FileSystemUtils::configPath() {
	return mgDir() / "config";
}

bool FileSystemUtils::isIgnored(const fs::path& p) {
	// ignore .minigit itself and common build/IDE directories
	string path_str = p.string();
	return path_str.find(MARKNAME) != string::npos ||
	       path_str.find("\\.vs\\") != string::npos ||
	       path_str.find("\\build\\") != string::npos ||
	       path_str.find("\\.git\\") != string::npos ||
	       path_str.find("\\CMakeFiles\\") != string::npos;
}

void FileSystemUtils::ensureRepo() {
	if (!fs::exists(mgDir()))
		throw runtime_error("Not a minigit repo. Run 'minigit init'.");
}

void FileSystemUtils::writeText(const fs::path& p, const string& s) {
	fs::create_directories(p.parent_path());
	ofstream f(p, ios::binary);
	f << s;
}

string FileSystemUtils::readText(const fs::path& p) {
	if (!fs::exists(p))
		return "";
	ifstream f(p, ios::binary);
	string s((istreambuf_iterator<char>(f)), {});
	return s;
}