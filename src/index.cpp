#include "index.h"
#include "objects.h"

Index::IndexMap Index::read() {
    IndexMap m;
    if (!fs::exists(FileSystemUtils::indexPath()))
        return m;
    ifstream f(FileSystemUtils::indexPath());
    string line;
    while (getline(f, line)) {
        auto t = line.find('\t');
        if (t == string::npos)
            continue;
        string k = line.substr(0, t);
        string v = line.substr(t + 1);
        m[k] = v;
    }
    return m;
}

void Index::write(const IndexMap &m) {
    ofstream f(FileSystemUtils::indexPath());
    for (auto &kv : m)
        f << kv.first << "\t" << kv.second << "\n";
}

void Index::stagePath(const fs::path &p, IndexMap &idx) {
    if (fs::is_directory(p)) {
        for (auto &e : fs::recursive_directory_iterator(p)) {
            if (e.is_directory())
                continue;
            if (FileSystemUtils::isIgnored(e.path()))
                continue;
            string rel = fs::relative(e.path(), FileSystemUtils::repoRoot()).generic_string();
            string h = Objects::storeBlob(e.path());
            idx[rel] = h;
            cout << "add " << rel << " -> " << h.substr(0, 12) << "\n";
        }
    } else {
        string rel = fs::relative(p, FileSystemUtils::repoRoot()).generic_string();
        if (FileSystemUtils::isIgnored(p))
            return;
        string h = Objects::storeBlob(p);
        idx[rel] = h;
        cout << "add " << rel << " -> " << h.substr(0, 12) << "\n";
    }
}