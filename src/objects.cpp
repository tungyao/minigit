#include "objects.h"

string Objects::storeBlob(const fs::path &file) {
	ifstream in(file, ios::binary);
	vector<unsigned char> data((istreambuf_iterator<char>(in)), {});
	string id = sha256_bytes(data);
	fs::create_directories(FileSystemUtils::getInstance().objectsDir());
	fs::path dst = FileSystemUtils::getInstance().objectsDir() / id;
	if (!fs::exists(dst)) {
		ofstream out(dst, ios::binary);
		out.write((char *)data.data(), data.size());
	}
	return id;
}

bool Objects::hasObject(const string &id) {
	return fs::exists(FileSystemUtils::getInstance().objectsDir() / id);
}

void Objects::copyObjectTo(const string &id, const fs::path &to) {
	fs::create_directories(to);
	fs::copy_file(FileSystemUtils::getInstance().objectsDir() / id, to / id,
				  fs::copy_options::overwrite_existing);
}

void Objects::copyObjectFrom(const fs::path &from, const string &id) {
	fs::create_directories(FileSystemUtils::getInstance().objectsDir());
	fs::copy_file(from / id, FileSystemUtils::getInstance().objectsDir() / id,
				  fs::copy_options::overwrite_existing);
}