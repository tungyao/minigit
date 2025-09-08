#include "objects.h"

string Objects::storeBlob(const fs::path &file) {
    ifstream in(file, ios::binary);
    vector<unsigned char> data((istreambuf_iterator<char>(in)), {});
    string id = sha256_bytes(data);
    fs::create_directories(FileSystemUtils::objectsDir());
    fs::path dst = FileSystemUtils::objectsDir() / id;
    if (!fs::exists(dst)) {
        ofstream out(dst, ios::binary);
        out.write((char *)data.data(), data.size());
    }
    return id;
}

bool Objects::hasObject(const string &id) {
    return fs::exists(FileSystemUtils::objectsDir() / id);
}

void Objects::copyObjectTo(const string &id, const fs::path &to) {
    fs::create_directories(to);
    fs::copy_file(FileSystemUtils::objectsDir() / id, to / id,
                  fs::copy_options::overwrite_existing);
}

void Objects::copyObjectFrom(const fs::path &from, const string &id) {
    fs::create_directories(FileSystemUtils::objectsDir());
    fs::copy_file(from / id, FileSystemUtils::objectsDir() / id,
                  fs::copy_options::overwrite_existing);
}