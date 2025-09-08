#pragma once

#include "common.h"
#include "filesystem_utils.h"
#include "sha256.h"

/**
 * Git对象管理类
 * 管理blob对象的存储和检索
 */
class Objects {
public:
    // Blob对象操作
    static string storeBlob(const fs::path& file);
    static bool hasObject(const string& id);
    
    // 对象复制操作（用于push/pull）
    static void copyObjectTo(const string& id, const fs::path& to);
    static void copyObjectFrom(const fs::path& from, const string& id);
};