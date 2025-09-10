## v2.1 重要更新（2025-09-10）🎉

### 1. 修复了 `add .` 命令无法检测删除文件的核心问题

#### 问题描述：
- `add .` 命令无法检测和暂存删除的文件
- 路径处理中存在跨平台兼容性问题
- 目录扫描时缺少已删除文件的包含逻辑

#### 解决方案：
```cpp
// 修复前：错误的路径处理
string rel_path = fs::relative(p, FileSystemUtils::repoRoot()).string();

// 修复后：跨平台兼容的路径处理
string rel_path = fs::relative(abs_p, FileSystemUtils::repoRoot()).generic_string();

// 增强的目录扫描逻辑：明确包含已删除文件
set<string> files_to_process(dir_files.begin(), dir_files.end());
for (const string& deleted_file : deleted_files) {
    if (dir_relative == "." || deleted_file.find(dir_relative + "/") == 0) {
        files_to_process.insert(deleted_file);
    }
}
```

### 2. 修复了 `reset --hard` 不能正确恢复删除文件的问题

#### 问题描述：
- `reset --hard` 在更新暂存区后才检测工作目录状态，导致状态检测错误
- 无法正确恢复在目标提交中存在但在工作目录中被删除的文件

#### 解决方案：
```cpp
// 修复前：错误的执行顺序
auto working_status = getWorkingDirectoryStatus(); // 在更新暂存区后调用

// 修复后：正确的执行顺序
vector<string> current_working_files;
scanWorkingDirectory(FileSystemUtils::repoRoot(), current_working_files);
// 收集当前暂存区和HEAD中跟踪的文件（在更新之前）
set<string> tracked_files;
for (const auto& kv : current_index) {
    tracked_files.insert(kv.first);
}
```

### 3. 修复了暂存区管理问题

#### 问题描述：
- 提交后暂存区被错误地清空，导致后续操作出现问题

#### 解决方案：
```cpp
// 修复前：错误地清空暂存区
Index::IndexMap empty_index;
Index::write(empty_index);

// 修复后：保持暂存区与HEAD一致
// 正确的做法：提交后暂存区应该与新的HEAD提交内容一致
// 而不是清空暂存区
// 暂存区已经包含了正确的文件，不需要清空
```

### 4. 增强了 `log` 命令显示删除文件

#### 新增功能：
- 完整的文件变更显示（新增、修改、删除）
- 变更统计信息
- 符号化的变更类型标识

#### 实现细节：
```cpp
// 新增的 showCommitChanges 方法
void Commands::showCommitChanges(const Commit& commit) {
    // 获取父提交的文件列表
    Index::IndexMap parent_files;
    if (!commit.parent.empty()) {
        auto parent_commit = CommitManager::loadCommit(commit.parent);
        if (parent_commit) {
            parent_files = parent_commit->tree;
        }
    }
    
    // 分类文件变更
    vector<pair<string, string>> added_files;   // 新增
    vector<pair<string, string>> modified_files; // 修改
    vector<string> deleted_files;               // 删除
    
    // 比较当前提交和父提交的差异...
}
```

### 5. 新增了历史文件追踪功能

#### 功能特点：
- **HD状态**：检测在历史提交中存在但现在完全不存在的文件
- **智能检测**：能够检测可能被意外删除的历史文件
- **防循环保护**：遍历历史提交时添加了防循环机制

#### 实现细节：
```cpp
// 历史文件追踪实现
Index::IndexMap Commands::getHistoricalFiles(const string& head_commit_id) {
    Index::IndexMap historical_files;
    string current_commit_id = head_commit_id;
    set<string> visited_commits; // 防止无限循环
    
    while (!current_commit_id.empty() && 
           visited_commits.find(current_commit_id) == visited_commits.end()) {
        visited_commits.insert(current_commit_id);
        // 遍历提交历史，收集所有曾经存在的文件
        // ...
    }
    return historical_files;
}
```

---

## 项目结构重构历史


## 新的项目结构

```
├── src/                           # 源代码目录
│   ├── common.h                   # 公共头文件（包含标准库）
│   ├── sha256.h/.cpp             # SHA-256哈希计算模块
│   ├── filesystem_utils.h/.cpp   # 文件系统工具模块
│   ├── index.h/.cpp              # 暂存区管理模块
│   ├── objects.h/.cpp            # Git对象管理模块
│   ├── commit.h/.cpp             # 提交管理模块
│   ├── commands.h/.cpp           # 命令实现模块
│   └── main.cpp                  # 主程序入口
├── CMakeLists.txt                # 更新的多文件构建配置
├── build/                        # 构建输出目录
├── README.md                     # 项目文档
└── 其他测试和构建脚本...

```

## 模块详细说明

### 1. common.h
- 包含所有标准C++库头文件
- 定义命名空间别名 (`namespace fs = std::filesystem`)
- 统一的 `using namespace std`

### 2. sha256.h/.cpp
- **SHA256类**：实现SHA-256哈希算法
- **sha256_bytes()**：计算字节数组的哈希
- **sha256_file()**：计算文件的哈希
- 来源于公共域实现，适配MiniGit需求

### 3. filesystem_utils.h/.cpp
- **FileSystemUtils类**：封装文件系统操作
- 仓库路径管理：`repoRoot()`, `mgDir()`, `objectsDir()` 等
- 文件读写：`writeText()`, `readText()`
- 仓库检查：`ensureRepo()`, `isIgnored()`

### 4. index.h/.cpp
- **Index类**：管理暂存区
- `IndexMap`：文件路径到blob SHA的映射类型
- **read()/write()**：读写暂存区文件
- **stagePath()**：将文件或目录添加到暂存区

### 5. objects.h/.cpp
- **Objects类**：管理Git对象
- **storeBlob()**：存储文件blob对象
- **hasObject()**：检查对象是否存在
- **copyObjectTo()/copyObjectFrom()**：对象复制（用于push/pull）

### 6. commit.h/.cpp
- **Commit结构体**：提交数据结构
- **CommitManager类**：提交管理
- **storeCommit()/loadCommit()**：存储和加载提交
- **serializeCommit()/deserializeCommit()**：提交序列化
- **nowISO8601()**：生成ISO8601时间戳

### 7. commands.h/.cpp
- **Commands类**：实现所有Git命令
- 基本命令：`init()`, `add()`, `commit()`, `status()`, `checkout()`, `reset()`, `log()`, `diff()`
- 远程命令：`push()`, `pull()`, `setRemote()`
- 私有辅助方法：`getRemote()`, `setRemoteConfig()`
- **文件状态检测**：`getWorkingDirectoryStatus()`, `calculateWorkingFileHash()`, `scanWorkingDirectory()`

### 8. main.cpp
- 主程序入口
- 命令行参数解析
- 异常处理

## 构建配置更新

### CMakeLists.txt 主要变更：
1. **源文件列表**：明确列出所有.cpp和.h文件
2. **包含目录**：设置 `src/` 为头文件搜索路径
3. **描述更新**：反映新增的reset功能
4. **调试信息**：显示源文件列表

### 构建命令：
```bash
# Windows
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release

# Linux/macOS  
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
```

## 重构优势

### 1. **模块化设计**
- 每个模块职责单一，便于理解和维护
- 模块间依赖关系清晰
- 便于单元测试

### 2. **可扩展性**
- 新功能可以独立模块实现
- 现有模块修改不影响其他部分
- 支持插件式功能扩展

### 3. **代码复用**
- 工具类可以被多个模块使用
- 避免代码重复
- 统一的错误处理和接口

### 4. **编译优化**
- 只重新编译修改的模块
- 支持并行编译
- 减少整体编译时间

## 兼容性

- ✅ 保持所有原有功能
- ✅ 命令行接口完全兼容
- ✅ 文件格式和存储结构不变
- ✅ 遵循C++17标准
- ✅ 跨平台兼容（Windows/Linux/macOS）

## 测试验证

所有原有功能均通过测试：
- ✅ init, add, commit, status, checkout
- ✅ push, pull, set-remote
- ✅ reset (--soft, --mixed, --hard) **✨ 今日修复 --hard 模式**
- ✅ **log (显示提交历史)** ✨ **今日增强 - 完整显示删除文件**
  - 支持详细和单行显示模式
  - 支持限制显示数量
  - **新增：完整的文件变更显示（+/-/~）**
  - **新增：变更统计信息**
- ✅ **diff/status 增强 (文件修改检测)** ✨ **今日增强 - 历史文件追踪**
  - 检测工作目录中的文件变化
  - 支持修改、新增、删除状态检测
  - **新增：HD状态（历史删除文件）**
  - 增强status命令显示工作目录状态
- ✅ **智能 add 操作** ✨ **今日修复 - 删除文件检测**
  - **修复：`add .` 现在能正确检测和暂存删除的文件**
  - **修复：路径处理跨平台兼容性问题**
  - **修复：目录扫描包含删除文件逻辑**

### 今日测试结果（v2.1）：
✅ **核心问题全部修复**：
- `add .` 能正确检测并暂存删除的文件
- `reset --hard` 能正确恢复被删除的文件
- `log` 显示完整的文件变更信息（包括删除文件）
- 新增历史文件追踪功能（HD状态）
- 暂存区管理正确性修复

生成的可执行文件大小：**167,936 字节**（多文件版本 vs 156,672 字节单文件版本）

## Log功能新增

### 功能特性：
- **提交历史查看**：显示从当前HEAD开始的提交链
- **多种显示模式**：详细格式和单行格式
- **数量限制**：支持`-n`和`--max-count`参数
- **错误处理**：对空仓库和无效提交的友好提示

### 实现细节：
- 使用parent字段遵循提交链
- 支持提交数量限制防止输出过多
- 显示文件变更统计和blob哈希

## Diff/Status增强功能

### 功能特性：
- **工作目录状态检测**：实时检测文件修改、新增、删除
- **增强Status命令**：显示暂存区和工作目录状态
- **新Diff命令**：查看文件变化详情
- **多种显示模式**：详细信息和仅文件名模式
- **文件状态标识**：M(修改)、D(删除)、??(未跟踪)

### 实现细节：
- **哈希比较**：通过SHA-256哈希比较文件内容变化
- **文件扫描**：递归扫描工作目录中的所有文件
- **状态分类**：精确识别文件在暂存区和工作目录中的不同状态
- **容错处理**：对文件系统错误的静默处理

## 后续扩展建议

基于新的模块化结构和 v2.1 的更新，可以考虑以下扩展：

### 短期优先级：
1. **分支模块**：添加基本的分支支持（branch/checkout）
2. **合并模块**：实现简单的三路合并
3. **性能模块**：大文件和大仓库优化
4. **网络模块**：支持真正的远程仓库（TCP/SSH）

### 中期目标：
5. **日志模块**：统一的日志记录和调试输出
6. **配置模块**：更丰富的配置管理（不仅仅是remote）
7. **差异模块**：实现文件diff功能
8. **压缩模块**：对象存储压缩

### 长期愿景：
9. **验证模块**：数据完整性检查
10. **单元测试模块**：全面的单元测试覆盖
11. **文档生成器**：自动生成API文档
12. **插件系统**：支持第三方扩展