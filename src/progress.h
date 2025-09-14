#pragma once

#include "common.h"

/**
 * 进度显示工具类
 * 用于在控制台显示传输和压缩进度
 */
class ProgressDisplay {
public:
    /**
     * 显示进度条
     * @param progress 进度百分比 (0-100)
     * @param description 操作描述
     * @param show_percentage 是否显示百分比
     */
    static void showProgress(int progress, const string& description, bool show_percentage = true);

    /**
     * 显示文件传输进度
     * @param current 当前传输字节数
     * @param total 总字节数
     * @param description 操作描述
     */
    static void showTransferProgress(size_t current, size_t total, const string& description);

    /**
     * 显示压缩进度
     * @param progress 进度百分比 (0-100)
     * @param operation 操作类型（压缩/解压缩）
     * @param filename 当前处理的文件名
     */
    static void showCompressionProgress(int progress, const string& operation, const string& filename);

    /**
     * 清除当前行
     */
    static void clearLine();

    /**
     * 完成进度显示，换行
     */
    static void finish();

    /**
     * 格式化文件大小
     * @param bytes 字节数
     * @return 格式化的大小字符串
     */
    static string formatFileSize(size_t bytes);

private:
    static const int PROGRESS_BAR_WIDTH = 50;
};