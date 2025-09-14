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
	std::string path_str = p.string();
	return path_str.find(MARKNAME) != string::npos;
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

void FileSystemUtils::writeBinary(const fs::path& p, const vector<uint8_t>& data) {
	fs::create_directories(p.parent_path());
	ofstream f(p, ios::binary);
	f.write(reinterpret_cast<const char*>(data.data()), data.size());
}

vector<uint8_t> FileSystemUtils::readBinary(const fs::path& p) {
	if (!fs::exists(p)) {
		return vector<uint8_t>();
	}
	ifstream f(p, ios::binary);
	f.seekg(0, ios::end);
	size_t size = f.tellg();
	f.seekg(0, ios::beg);
	
	vector<uint8_t> data(size);
	f.read(reinterpret_cast<char*>(data.data()), size);
	return data;
}