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

## 特性

- 单文件实现，无外部依赖
- 支持基本的Git操作：init、add、commit、push、pull、reset、log、status、diff
- 跨平台兼容（Windows、Linux、macOS）
- 使用SHA-1哈希算法（高性能优化）
- 简单的文件系统存储

### 性能优化

本项目在哈希算法上进行了重大性能优化：

- **算法升级**: 从 SHA-256 更换为更快的 SHA-1 算法
- **内存优化**: 使用固定缓冲区替代动态vector，减少内存分配
- **流式处理**: 64KB缓冲区分块读取大文件，避免一次性加载整个文件到内存
- **性能提升**: 5MB文件处理时间从 30秒 减少到 0.1-0.2秒，**提升约150-300倍**！

### Log功能说明

log命令支持查看提交历史：

- **默认格式**: 显示详细的提交信息（commit hash、父提交、时间、消息、文件列表）
- `--oneline`: 单行显示（commit hash + 消息）
- `-n <count>`: 限制显示的提交数量
- `--max-count=<count>`: 另一种限制数量的方式

```bash
# 显示所有提交历史
./minigit log

# 单行显示所有提交
./minigit log --oneline

# 显示最近5次提交
./minigit log -n 5

# 单行显示最近3次提交
./minigit log --oneline -n 3
```

### Diff和Status功能说明

新增的文件修改检测功能：

#### Status命令增强：
- **暂存区状态**：显示已暂存的文件
- **工作目录状态**：检测修改、新增、删除的文件
- **文件状态标识**：
  - `M` - 已修改的文件
  - `D` - 已删除的文件
  - `??` - 未跟踪的新文件

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

reset命令支持三种模式：

- `--soft`: 只重置HEAD指针，保留暂存区和工作目录
- `--mixed` (默认): 重置HEAD和暂存区，保留工作目录
- `--hard`: 重置HEAD、暂存区和工作目录到指定提交

```bash
# 重置到当前提交（清理暂存区）
./minigit reset

# 软重置到指定提交
./minigit reset --soft <commit-hash>

# 硬重置到指定提交
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

# 添加文件到暂存区
./minigit add <file-or-dir>

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
- 不支持文件重命名
- 远程仓库必须是本地文件夹路径
- 不支持并发操作
- 简化的JSON格式（无转义）

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