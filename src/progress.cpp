#include "progress.h"
#include <iomanip>
#include <sstream>

void ProgressDisplay::showProgress(int progress, const string &description, bool show_percentage) {
	// 确保进度在0-100范围内
	progress = max(0, min(100, progress));

	int bar_width = PROGRESS_BAR_WIDTH;
	int filled = (progress * bar_width) / 100;

	cout << "\r"; // 回到行首
	cout << description << " [";

	// 绘制进度条
	for (int i = 0; i < bar_width; ++i) {
		if (i < filled) {
			cout << "=";
		} else if (i == filled && progress < 100) {
			cout << ">";
		} else {
			cout << " ";
		}
	}

	cout << "]";

	if (show_percentage) {
		cout << " " << setw(3) << progress << "%";
	}

	cout << flush;
}

void ProgressDisplay::showTransferProgress(size_t current, size_t total,
										   const string &description) {
	if (total == 0) {
		showProgress(100, description);
		return;
	}

	int progress = static_cast<int>((current * 100) / total);

	cout << "\r"; // 回到行首
	cout << description << " [";

	int bar_width = PROGRESS_BAR_WIDTH;
	int filled = (progress * bar_width) / 100;

	// 绘制进度条
	for (int i = 0; i < bar_width; ++i) {
		if (i < filled) {
			cout << "=";
		} else if (i == filled && progress < 100) {
			cout << ">";
		} else {
			cout << " ";
		}
	}

	cout << "] " << setw(3) << progress << "% ";
	cout << "(" << formatFileSize(current) << "/" << formatFileSize(total) << ")";
	cout << flush;
}

void ProgressDisplay::showCompressionProgress(int progress, const string &operation,
											  const string &filename) {
	// 确保进度在0-100范围内
	progress = max(0, min(100, progress));

	string description = operation + ": " + filename;

	// 如果描述太长，截断并添加省略号
	const size_t max_desc_length = 40;
	if (description.length() > max_desc_length) {
		description = description.substr(0, max_desc_length - 3) + "...";
	}

	showProgress(progress, description, true);
}

void ProgressDisplay::clearLine() {
	cout << "\r" << string(80, ' ') << "\r" << flush;
}

void ProgressDisplay::finish() {
	cout << endl;
}

string ProgressDisplay::formatFileSize(size_t bytes) {
	const vector<string> units = {"B", "KB", "MB", "GB"};
	double size = static_cast<double>(bytes);
	size_t unit_index = 0;

	while (size >= 1024.0 && unit_index < units.size() - 1) {
		size /= 1024.0;
		unit_index++;
	}

	ostringstream oss;
	if (unit_index == 0) {
		oss << static_cast<size_t>(size) << " " << units[unit_index];
	} else {
		oss << fixed << setprecision(1) << size << " " << units[unit_index];
	}

	return oss.str();
}