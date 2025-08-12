#!/bin/bash

# PADetect C++ 构建脚本
# 用于在Xcode中调用Makefile编译C++代码

set -e  # 遇到错误时退出

# 获取脚本所在目录
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "=== PADetect C++ Build Script ==="
echo "Working directory: $(pwd)"

# 检查Makefile是否存在
if [ ! -f "Makefile" ]; then
    echo "Error: Makefile not found in current directory"
    exit 1
fi

# 设置构建类型
BUILD_TYPE="${1:-release}"  # 默认为release

echo "Build type: $BUILD_TYPE"

# 清理之前的构建
echo "Cleaning previous build..."
make clean

# 根据构建类型进行编译
case "$BUILD_TYPE" in
    "debug")
        echo "Building debug version..."
        make debug
        ;;
    "release")
        echo "Building release version..."
        make release
        ;;
    *)
        echo "Building default version..."
        make all
        ;;
esac

echo "=== Build completed successfully ==="

# 检查生成的可执行文件
if [ -f "PADetect" ]; then
    echo "Executable created: PADetect"
    ls -la PADetect
else
    echo "Warning: Executable PADetect not found"
    exit 1
fi

echo "=== C++ Build Script Finished ==="