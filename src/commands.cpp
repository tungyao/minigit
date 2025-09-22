#include "commands.h"
#include "commands_basic.h"
#include "commands_history.h"
#include "commands_remote.h"

// Basic commands - delegate to CommandsBasic
int Commands::init() {
	return CommandsBasic::init();
}

int Commands::add(vector<string> args) {
	return CommandsBasic::add(args);
}

int Commands::commit(vector<string> args) {
	return CommandsBasic::commit(args);
}

int Commands::status() {
	return CommandsBasic::status();
}

int Commands::checkout() {
	return CommandsBasic::checkout();
}

// History commands - delegate to CommandsHistory
int Commands::reset(vector<string> args) {
	return CommandsHistory::reset(args);
}

int Commands::log(vector<string> args) {
	return CommandsHistory::log(args);
}

int Commands::diff(vector<string> args) {
	return CommandsHistory::diff(args);
}

// Remote commands - delegate to CommandsRemote
int Commands::push(vector<string> args) {
	return CommandsRemote::push(args);
}

int Commands::pull(vector<string> args) {
	return CommandsRemote::pull(args);
}

int Commands::setRemote(const string &path) {
	return CommandsRemote::setRemote(path);
}

// Legacy helper methods (kept for compatibility but now delegate to appropriate modules)
string Commands::getRemote() {
	return CommandsRemote::getRemote();
}

void Commands::setRemoteConfig(const string &path) {
	CommandsRemote::setRemoteConfig(path);
}

Commands::NetworkRemote Commands::parseNetworkRemote(const string &remote_url) {
	auto remote = CommandsRemote::parseNetworkRemote(remote_url);
	NetworkRemote result;
	result.host = remote.host;
	result.port = remote.port;
	result.repo_name = remote.repo_name;
	return result;
}

int Commands::networkPush(const NetworkRemote &remote, const string &password) {
	CommandsRemote::NetworkRemote remote_obj;
	remote_obj.host = remote.host;
	remote_obj.port = remote.port;
	remote_obj.repo_name = remote.repo_name;
	return CommandsRemote::networkPush(remote_obj, password);
}

int Commands::networkPull(const NetworkRemote &remote, const string &password) {
	CommandsRemote::NetworkRemote remote_obj;
	remote_obj.host = remote.host;
	remote_obj.port = remote.port;
	remote_obj.repo_name = remote.repo_name;
	return CommandsRemote::networkPull(remote_obj, password);
}

int Commands::networkLog(const NetworkRemote &remote, const string &password, int max_count,
						 bool line) {
	CommandsRemote::NetworkRemote remote_obj;
	remote_obj.host = remote.host;
	remote_obj.port = remote.port;
	remote_obj.repo_name = remote.repo_name;
	return CommandsRemote::networkLog(remote_obj, password, max_count, line);
}

vector<Commands::FileStatus> Commands::getWorkingDirectoryStatus() {
	auto basic_statuses = CommandsBasic::getWorkingDirectoryStatus();
	vector<FileStatus> result;
	for (const auto &status : basic_statuses) {
		FileStatus fs;
		fs.path = status.path;
		fs.status = status.status;
		fs.staged_hash = status.staged_hash;
		fs.working_hash = status.working_hash;
		fs.old_path = status.old_path;
		result.push_back(fs);
	}
	return result;
}

string Commands::calculateWorkingFileHash(const fs::path &file_path) {
	return CommandsBasic::calculateWorkingFileHash(file_path);
}

void Commands::scanWorkingDirectory(const fs::path &dir, vector<string> &files) {
	CommandsBasic::scanWorkingDirectory(dir, files);
}

void Commands::detectRenames(vector<FileStatus> &statuses) {
	// Convert to basic format
	vector<CommandsBasic::FileStatus> basic_statuses;
	for (const auto &status : statuses) {
		CommandsBasic::FileStatus fs;
		fs.path = status.path;
		fs.status = status.status;
		fs.staged_hash = status.staged_hash;
		fs.working_hash = status.working_hash;
		fs.old_path = status.old_path;
		basic_statuses.push_back(fs);
	}

	// Call the actual implementation
	CommandsBasic::detectRenames(basic_statuses);

	// Convert back
	statuses.clear();
	for (const auto &status : basic_statuses) {
		FileStatus fs;
		fs.path = status.path;
		fs.status = status.status;
		fs.staged_hash = status.staged_hash;
		fs.working_hash = status.working_hash;
		fs.old_path = status.old_path;
		statuses.push_back(fs);
	}
}

Index::IndexMap Commands::getHistoricalFiles(const string &head_commit_id) {
	return CommandsBasic::getHistoricalFiles(head_commit_id);
}

void Commands::showCommitChanges(const Commit &commit) {
	CommandsHistory::showCommitChanges(commit);
}