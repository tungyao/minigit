#include "commit.h"

string CommitManager::nowISO8601() {
	using namespace std::chrono;
	auto t = system_clock::now();
	time_t tt = system_clock::to_time_t(t);
	std::tm tm{};
#ifdef _WIN32
	localtime_s(&tm, &tt);
#else
	localtime_r(&tt, &tm);
#endif
	char buf[32];
	strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tm);
	return string(buf);
}

std::set<string> CommitManager::commitsCount(const string &newer, const string &older) {
	string parent_head = newer;
	int commits_count = 0;
	std::set<string> head;
	while (parent_head != older) {
		auto commits = CommitManager::loadCommit(parent_head);

		if (commits) {
			commits_count += 1;
			head.insert(parent_head);
			parent_head = commits->parent;
		}
	}
	return head;
}

string CommitManager::serializeCommit(const Commit &c) {
	// naive JSON (no escaping for simplicity)
	string s = "{\n";
	s += R"(  "parent": ")" + c.parent + "\",\n";
	s += R"(  "message": ")" + c.message + "\",\n";
	s += R"(  "timestamp": ")" + c.timestamp + "\",\n";
	s += R"(  "tree": {\n)";
	// bool first = true;
	for (const auto &[fst, snd] : c.tree) {
		s += string("    ") + "\"" + fst + "\": \"" + snd + "\",\n";
	}
	if (!c.tree.empty())
		s.pop_back(), s.pop_back(), s += "\n"; // remove last comma
	s += "  }\n";
	s += "}\n";
	// compute id as sha256 of this text
	vector<unsigned char> bytes(s.begin(), s.end());
	string id = sha256_bytes(bytes);
	return id + "\n" + s; // return with leading id on first line for storage convenience
}

Commit CommitManager::deserializeCommit(const string &raw) {
	Commit c;
	string s = raw;
	auto nl = s.find('\n');
	string idline = s.substr(0, nl);
	c.id = idline;
	string j = s.substr(nl + 1);
	auto fp = [&](const string &k) {
		size_t p = j.find("\"" + k + "\"");
		if (p == string::npos)
			return string("");
		p = j.find(':', p);
		size_t q = j.find('"', p + 1);
		size_t r = j.find('"', q + 1);
		return j.substr(q + 1, r - q - 1);
	};
	c.parent = fp("parent");
	c.message = fp("message");
	c.timestamp = fp("timestamp");
	// parse tree lines: "path": "hash"
	size_t tp = j.find("\"tree\"");
	if (tp != string::npos) {
		tp = j.find('{', tp);
		size_t tend = j.find('}', tp);
		string body = j.substr(tp + 1, tend - tp - 1);
		stringstream ss(body);
		string line;
		while (getline(ss, line, ',')) {
			auto a = line.find('"');
			if (a == string::npos)
				continue;
			auto b = line.find('"', a + 1);
			string path = line.substr(a + 1, b - a - 1);
			auto c1 = line.find('"', b + 1);
			if (c1 == string::npos)
				continue;
			auto d = line.find('"', c1 + 1);
			string hash = line.substr(c1 + 1, d - c1 - 1);
			if (!path.empty() && !hash.empty())
				c.tree[path] = hash;
		}
	}
	return c;
}

string CommitManager::storeCommit(const Commit &c) {
	string payload = serializeCommit(c);
	auto nl = payload.find('\n');
	string id = payload.substr(0, nl);
	string body = payload.substr(nl + 1);
	fs::path dst = FileSystemUtils::getInstance().objectsDir() / id;
	if (!fs::exists(dst)) {
		ofstream f(dst, ios::binary);
		f << body;
	}
	FileSystemUtils::getInstance().writeText(FileSystemUtils::getInstance().headPath(), id);
	return id;
}

optional<Commit> CommitManager::loadCommit(const string &id) {
	fs::path p = FileSystemUtils::getInstance().objectsDir() / id;
	// cout << "loadCommit " << p.string() << endl;
	if (!fs::exists(p))
		return nullopt;
	string body = FileSystemUtils::getInstance().readText(p);
	Commit c = deserializeCommit(id + "\n" + body);
	return c;
}

optional<Commit> CommitManager::loadCommit(const string repo_name, const string &id) {
	fs::path p = FileSystemUtils::getInstance().objectsDir() / id;
	if (!fs::exists(p))
		return nullopt;
	string body = FileSystemUtils::getInstance().readText(p);
	Commit c = deserializeCommit(id + "\n" + body);
	return c;
}