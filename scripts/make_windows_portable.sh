#!/bin/bash
# make_windows_portable.sh - 在 macOS/Linux 上一键交叉编译 Windows Portable 版
# 需要安装 Docker
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_DIR"

echo "========================================="
echo "  奥拉暖暖 - 构建 Windows Portable 版"
echo "========================================="
echo ""

# 检查 Docker
if ! command -v docker &> /dev/null; then
    echo "错误: 未检测到 Docker，请先安装 Docker Desktop"
    echo "  macOS: brew install --cask docker"
    echo "  或从 https://www.docker.com/products/docker-desktop 下载"
    exit 1
fi

# 检查 Docker 是否在运行
if ! docker info &> /dev/null 2>&1; then
    echo "错误: Docker 未运行，请先启动 Docker Desktop"
    exit 1
fi

echo "[1/3] 构建 Docker 编译环境（首次需要较长时间）..."
docker build -t aola-win-builder -f scripts/Dockerfile.windows . 2>&1 | tail -5

echo ""
echo "[2/3] 在 Docker 中交叉编译..."
docker run --rm \
    -v "$PROJECT_DIR:/src" \
    -w /src \
    aola-win-builder \
    bash scripts/build_windows.sh

echo ""
echo "[3/3] 构建完成！"
echo ""

OUTPUT="AolaNuanNuan-Windows-Portable"
if [ -f "${OUTPUT}.zip" ]; then
    echo "输出文件: ${OUTPUT}.zip"
    ls -lh "${OUTPUT}.zip"
    echo ""
    echo "将 ${OUTPUT}.zip 拷贝到 Windows 电脑上解压即可运行！"
else
    echo "构建产物在: ${OUTPUT}/"
    ls -la "${OUTPUT}/"
fi
