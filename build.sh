#!/bin/bash
# 跨平台构建脚本

# 检测操作系统
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "win32" ]]; then
    PLATFORM="Windows"
    GENERATOR="Visual Studio 16 2019"
    if command -v "Visual Studio 17 2022" &> /dev/null; then
        GENERATOR="Visual Studio 17 2022"
    fi
else
    PLATFORM="Linux"
    GENERATOR="Unix Makefiles"
fi

echo "检测到平台: $PLATFORM"
echo "使用生成器: $GENERATOR"

# 创建构建目录
mkdir -p build
cd build

# 配置CMake
if [[ "$PLATFORM" == "Windows" ]]; then
    cmake .. -G "$GENERATOR" -A x64
else
    cmake .. -G "$GENERATOR" -DCMAKE_BUILD_TYPE=Release
fi

# 编译
if [[ "$PLATFORM" == "Windows" ]]; then
    cmake --build . --config Release
else
    cmake --build . -j$(nproc)
fi

echo "构建完成！可执行文件位于："
if [[ "$PLATFORM" == "Windows" ]]; then
    echo "  build/bin/Release/minigit.exe"
else
    echo "  build/bin/minigit"
fi