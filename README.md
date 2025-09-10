# MiniGit

一个支持init、add、commit、push、pull、reset操作的微型Git工具，使用C++17实现。

## 项目结构

项目已重构为模块化设计，便于维护和扩展：

```
src/
├── common.h                 # 公共头文件
├── sha256.h/.cpp           # SHA-1哈希计算（高性能实现）
├── filesystem_utils.h/.cpp # 文件系统工具
├── index.h/.cpp            # 暂存区管理
├── objects.h/.cpp          # Git对象管理
├── commit.h/.cpp           # 提交管理
├── commands.h/.cpp         # 命令实现
└── main.cpp                # 主程序入口
```

详细的重构说明请参见 [REFACTOR_GUIDE.md](REFACTOR_GUIDE.md)。

## 今日重要更新说明（对应于 v2.1） ✨

### 1. **修复了 `add .` 命令无法检测删除文件的问题**
- ✅ **核心修复**：现在 `add .` 能够正确检测并暂存删除的文件
- ✅ **路径处理修复**：统一使用 `.generic_string()` 确保跨平台兼容性
- ✅ **目录扫描增强**：在目录扫描时明确包含已删除的文件

### 2. **修复了 `reset --hard` 不能正确恢复删除文件的问题**
- ✅ **逻辑错误修复**：解决了在更新暂存区后才检测工作目录状态的问题
- ✅ **文件恢复增强**：现在能正确恢复在目标提交中存在但被删除的文件
- ✅ **清理多余文件**：正确删除不在目标提交中的文件

### 3. **修复了暂存区管理问题**
- ✅ **提交后状态修复**：提交后暂存区不再被错误地清空，保持与HEAD一致
- ✅ **后续操作保障**：确保后续的add、status等操作正常工作

### 4. **增强了 `log` 命令显示删除文件**
- ✨ **完整变更显示**：现在log命令能显示完整的文件变更信息
- ✨ **分类显示**：
  - `+` 新增的文件
  - `~` 修改的文件  
  - `-` 删除的文件
- ✨ **统计信息**：显示总变更数和分类统计

### 5. **新增了历史文件追踪功能**
- ✨ **`HD` 状态**：新增历史删除文件状态，检测在历史提交中存在但现在不存在的文件
- ✨ **智能检测**：能够检测可能被意外删除的历史文件
- ✨ **防循环保护**：遥历历史提交时添加了防循环机制

---

- 单文件实现，无外部依赖
- 支持基本的Git操作：init、add、commit、push、pull、reset、log、status、diff
- **智能add操作**：自动检测修改、新增、删除和重命名的文件
- 跨平台兼容（Windows、Linux、macOS）
- 使用SHA-1哈希算法（高性能优化）
- 简单的文件系统存储

### 智能Add操作功能

新增的智能add操作能够自动检测和处理各种文件状态变化：

#### 支持的文件状态：
- **`M` 修改的文件**：检测文件与HEAD提交的差异，只添加真正有变化的文件
- **`??` 新增的文件**：检测并添加未跟踪的新文件
- **`D` 删除的文件**：检测并处理已删除的文件，从暂存区移除 ✨ **今日修复**
- **`HD` 历史删除的文件**：检测在历史提交中存在但现在完全不存在的文件 ✨ **今日新增**
- **`R` 重命名的文件**：通过内容哈希对比自动检测文件重命名操作

#### 功能特点：
- **智能过滤**：自动跳过未修改的文件，提高效率
- **删除检测**：正确检测并处理已删除的文件 ✨ **今日修复**
- **历史文件追踪**：检测在历史提交中存在但被意外删除的文件 ✨ **今日新增**
- **重命名检测**：通过SHA-1哈希对比检测文件重命名
- **批量处理**：支持目录递归扫描，一次性处理所有变化
- **状态反馈**：提供详细的操作反馈信息

```bash
# 智能add示例
./minigit add file1.txt          # 只处理修改过的文件
./minigit add .                  # 递归处理所有变化（推荐）
./minigit add src/              # 处理指定目录下的变化

# 示例输出：
# add file1.txt (modified)
# rename old_file.txt -> new_file.txt
# add new_file.txt (new)
# remove deleted_file.txt (deleted)
# skip unchanged_file.txt (unchanged)
# Processed 4 file change(s) in staging area.
```

### 性能优化

本项目在哈希算法上进行了重大性能优化：

- **算法升级**: 从 SHA-256 更换为更快的 SHA-1 算法
- **内存优化**: 使用固定缓冲区替代动态vector，减少内存分配
- **流式处理**: 64KB缓冲区分块读取大文件，避免一次性加载整个文件到内存
- **性能提升**: 5MB文件处理时间从 30秒 减少到 0.1-0.2秒，**提升约150-300倍**！

### Log功能说明

log命令支持查看提交历史，**包含完整的文件变更信息**：

- **默认格式**: 显示详细的提交信息（commit hash、父提交、时间、消息、**完整的文件变更统计**）
- **文件变更显示**：
  - `+ filename (hash)` - 新增的文件
  - `~ filename (hash)` - 修改的文件  
  - `- filename (deleted)` - 删除的文件
- **变更统计**：显示总变更数和分类统计（如：`Files changed: 2 (-2 deleted)`）
- `--oneline`: 单行显示（commit hash + 消息）
- `-n <count>`: 限制显示的提交数量
- `--max-count=<count>`: 另一种限制数量的方式

```bash
# 显示所有提交历史（包含完整变更信息）
./minigit log

# 示例输出：
# commit f6be97487f25
# Date: 2025-09-10T17:42:08
#     Added fileD
#     Files changed: 1 (+1 added)
#         + fileD.txt (b38def6ba1c5)
#
# commit 89923410f729  
# Date: 2025-09-10T17:42:08
#     Deleted fileA and fileC
#     Files changed: 2 (-2 deleted)
#         - fileA.txt (deleted)
#         - fileC.txt (deleted)

# 单行显示所有提交
./minigit log --oneline

# 显示最近5次提交
./minigit log -n 5
```

### Diff和Status功能说明

新增的文件修改检测功能：

#### Status命令增强：
- **暂存区状态**：显示已暂存的文件
- **工作目录状态**：检测修改、新增、删除的文件
- **文件状态标识**：
  - `M` - 已修改的文件
  - `D` - 已删除的文件
  - `HD` - 历史删除的文件（在历史提交中存在但现在不存在） ✨ **今日新增**
  - `??` - 未跟踪的新文件
  - `R` - 重命名的文件

#### Diff命令：
- **默认模式**：显示工作目录与暂存区的差异
- `--cached/--staged`：显示暂存区与HEAD的差异
- `--name-only`：仅显示变更的文件名

```bash
# 查看工作目录中的所有变化
./minigit diff

# 仅显示变更的文件名
./minigit diff --name-only

# 查看暂存区中的变化
./minigit diff --cached

# 增强的status命令（显示暂存区+工作目录状态）
./minigit status

### Reset功能说明

reset命令支持三种模式，**现在支持正确恢复删除的文件**： ✨ **今日修复**

- `--soft`: 只重置HEAD指针，保留暂存区和工作目录
- `--mixed` (默认): 重置HEAD和暂存区，保留工作目录
- `--hard`: 重置HEAD、暂存区和工作目录到指定提交，**正确恢复被删除的文件**

#### `reset --hard` 修复的问题：
- ✅ **正确恢复删除文件**：现在能够正确恢复在目标提交中存在但在工作目录中被删除的文件
- ✅ **正确删除多余文件**：正确删除在工作目录中存在但在目标提交中不存在的文件
- ✅ **修复逻辑错误**：解决了在更新暂存区后才检测工作目录状态的错误

```bash
# 重置到当前提交（清理暂存区）
./minigit reset

# 软重置到指定提交
./minigit reset --soft <commit-hash>

# 硬重置到指定提交（现在能正确恢复删除的文件）
./minigit reset --hard <commit-hash>
```

## 构建要求

- C++17兼容的编译器
- CMake 3.16或更高版本

### Windows
- Visual Studio 2019/2022 或 MinGW-w64
- 推荐使用Visual Studio

### Linux/macOS
- GCC 7+ 或 Clang 5+
- 标准构建工具

## 快速开始

### 方法1：使用构建脚本（推荐）

**Windows:**
```bash
build.bat
```

**Linux/macOS:**
```bash
chmod +x build.sh
./build.sh
```

### 方法2：手动使用CMake

```bash
# 创建构建目录
mkdir build
cd build

# 配置（Windows）
cmake .. -G "Visual Studio 17 2022" -A x64

# 配置（Linux/macOS）
cmake .. -DCMAKE_BUILD_TYPE=Release

# 编译
cmake --build . --config Release
```

## 使用方法

构建完成后，可执行文件位于：
- Windows: `build/bin/Release/minigit.exe`
- Linux/macOS: `build/bin/minigit`

### 基本命令

```bash
# 初始化仓库
./minigit init

# 智能add：只添加修改、新增、删除和重命名的文件
./minigit add <file-or-dir>
./minigit add .                  # 推荐：批量处理所有变化

# 提交更改
./minigit commit -m "提交信息"

# 查看仓库状态（包含工作目录变化）
./minigit status

# 查看文件修改详情
./minigit diff [--name-only] [--cached]

# 查看提交历史
./minigit log [--oneline] [-n <count>]

# 重置到指定提交
./minigit reset [--soft|--mixed|--hard] [<commit>]

# 设置远程仓库（本地目录）
./minigit set-remote /path/to/remote/repo

# 推送到远程
./minigit push

# 从远程拉取
./minigit pull

# 检出文件
./minigit checkout
```

## 限制

- 不支持分支和合并
- 远程仓库必须是本地文件夹路径
- 不支持并发操作
- 简化的JSON格式（无转义）
- 重命名检测基于内容哈希，可能出现误判

## 仓库结构

```
.minigit/
├── objects/     # 存储blob和commit对象（按SHA-1命名）
├── index        # 暂存区映射（文件路径 -> blob SHA）
├── HEAD         # 当前commit SHA
└── config       # 远程配置
```

## 许可证

本项目包含公共域的SHA-1实现。