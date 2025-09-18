#include "client.h"
#include "commands.h"
#include "server.h"

int main(int argc, char **argv) {
	ios::sync_with_stdio(false);
	if (argc < 2) {
		cerr << "Usage: minigit "
				"<init|add|commit|push|pull|status|checkout|reset|log|diff|set-remote|server|"
				"connect|clone> [args]\n";
		return 1;
	}

	string cmd = argv[1];

	try {
		if (cmd == "init")
			return Commands::init();
		else if (cmd == "add") {
			vector<string> a;
			for (int i = 2; i < argc; ++i)
				a.push_back(argv[i]);
			return Commands::add(a);
		} else if (cmd == "commit") {
			vector<string> a;
			for (int i = 2; i < argc; ++i)
				a.push_back(argv[i]);
			return Commands::commit(a);
		} else if (cmd == "push") {
			vector<string> a;
			for (int i = 2; i < argc; ++i)
				a.push_back(argv[i]);
			return Commands::push(a);
		} else if (cmd == "pull") {
			vector<string> a;
			for (int i = 2; i < argc; ++i)
				a.push_back(argv[i]);
			return Commands::pull(a);
		} else if (cmd == "status")
			return Commands::status();
		else if (cmd == "checkout")
			return Commands::checkout();
		else if (cmd == "reset") {
			vector<string> a;
			for (int i = 2; i < argc; ++i)
				a.push_back(argv[i]);
			return Commands::reset(a);
		} else if (cmd == "log") {
			vector<string> a;
			for (int i = 2; i < argc; ++i)
				a.push_back(argv[i]);
			return Commands::log(a);
		} else if (cmd == "diff") {
			vector<string> a;
			for (int i = 2; i < argc; ++i)
				a.push_back(argv[i]);
			return Commands::diff(a);
		} else if (cmd == "set-remote") {
			if (argc < 3) {
				cerr << "Usage: minigit set-remote <folder>\n";
				return 1;
			}
			return Commands::setRemote(argv[2]);
		} else if (cmd == "server") {
			vector<string> a;
			for (int i = 2; i < argc; ++i)
				a.push_back(argv[i]);
			return ServerCommand::parseAndRun(a);
		} else if (cmd == "connect") {
			vector<string> a;
			for (int i = 2; i < argc; ++i)
				a.push_back(argv[i]);
			return ClientCommand::parseAndRun(a);
		} else if (cmd == "clone") {
			vector<string> a;
			for (int i = 2; i < argc; ++i)
				a.push_back(argv[i]);
			return CloneCommand::parseAndRun(a);
		} else {
			cerr << "Unknown command." << "\n";
			return 1;
		}
	} catch (const exception &e) {
		cerr << "Error: " << e.what() << "\n";
		return 2;
	}
}