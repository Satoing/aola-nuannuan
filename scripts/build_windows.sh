#!/bin/bash
# build_windows.sh - Windows 交叉编译 + 打包脚本
# 在 Docker 容器中运行，或在安装了 mingw-w64 的 Linux 系统中运行
set -e

echo "========================================="
echo "  奥拉暖暖 - Windows Portable 构建"
echo "========================================="

BUILD_DIR="build-windows"
OUTPUT_DIR="AolaNuanNuan-Windows-Portable"

# 清理旧构建
rm -rf "$BUILD_DIR" "$OUTPUT_DIR" "${OUTPUT_DIR}.zip"
mkdir -p "$BUILD_DIR"

# CMake 配置
echo "[1/4] CMake 配置..."
cmake -B "$BUILD_DIR" \
    -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-mingw64.cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="/mingw-deps" \
    -DCMAKE_FIND_ROOT_PATH="/mingw-deps;/usr/x86_64-w64-mingw32" \
    -DPKG_CONFIG_EXECUTABLE=/usr/bin/pkg-config

# 编译
echo "[2/4] 编译..."
cmake --build "$BUILD_DIR" -j$(nproc)

# 打包
echo "[3/4] 打包 Portable 应用..."
mkdir -p "$OUTPUT_DIR"

# 复制可执行文件
cp "$BUILD_DIR/AolaNuanNuan.exe" "$OUTPUT_DIR/"

# 复制图标
cp icon.png "$OUTPUT_DIR/" 2>/dev/null || true

# 复制 SDL2 DLL
echo "  复制 SDL2 DLL..."
for dll in SDL2.dll SDL2_image.dll SDL2_ttf.dll; do
    find /mingw-deps -name "$dll" -exec cp {} "$OUTPUT_DIR/" \; 2>/dev/null
done

# 复制 SDL2_image 依赖的图片格式 DLL
for dll in libpng16-16.dll libjpeg-62.dll libtiff-6.dll libwebp-7.dll zlib1.dll; do
    find /mingw-deps -name "$dll" -exec cp {} "$OUTPUT_DIR/" \; 2>/dev/null
done

# 复制 SDL2_ttf 依赖 DLL
for dll in libfreetype-6.dll libharfbuzz-0.dll; do
    find /mingw-deps -name "$dll" -exec cp {} "$OUTPUT_DIR/" \; 2>/dev/null
done

# 复制 curl DLL
echo "  复制 curl DLL..."
find /mingw-deps -name "libcurl*.dll" -exec cp {} "$OUTPUT_DIR/" \; 2>/dev/null
find /mingw-deps -name "libssl*.dll" -exec cp {} "$OUTPUT_DIR/" \; 2>/dev/null
find /mingw-deps -name "libcrypto*.dll" -exec cp {} "$OUTPUT_DIR/" \; 2>/dev/null

# 复制 MinGW 运行时 DLL
echo "  复制 MinGW 运行时 DLL..."
MINGW_BIN="/usr/x86_64-w64-mingw32/lib"
for dll in libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll; do
    find /usr -name "$dll" -exec cp {} "$OUTPUT_DIR/" \; 2>/dev/null
done

# 创建字体目录（Windows 用户可以放自己的中文字体进去）
mkdir -p "$OUTPUT_DIR/fonts"

# 如果系统有中文字体，复制一份
if [ -f "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc" ]; then
    cp /usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc "$OUTPUT_DIR/fonts/NotoSansCJK.ttc"
fi

# 创建启动说明
cat > "$OUTPUT_DIR/说明.txt" << 'EOF'
奥拉暖暖 - Windows Portable 版

【使用方法】
双击 AolaNuanNuan.exe 运行

【字体说明】
程序会自动使用 Windows 系统中文字体（微软雅黑等）。
如果中文显示异常，请将中文字体文件（如 msyh.ttc）复制到 fonts 目录。

【操作说明】
- 左侧分类标签切换服装类型
- 点击列表中的服装名称穿上/脱下
- 鼠标滚轮滚动列表
- 右上角切换人模(旧/新男/新女)
- 保存按钮：保存当前装扮并生成编号
- 加载按钮：输入编号加载已保存的装扮
- R键重置 | F键搜索 | ESC键退出

【注意事项】
- 需要网络连接（从服务器加载配置和图片资源）
- 数据保存在程序同目录下的 outfits.db 文件中
- 图片缓存在 cache 目录中
EOF

# 打 ZIP 包
echo "[4/4] 压缩..."
zip -r "${OUTPUT_DIR}.zip" "$OUTPUT_DIR/" -x "*.DS_Store"

echo ""
echo "========================================="
echo "  构建完成！"
echo "  输出: ${OUTPUT_DIR}.zip"
echo "========================================="

# 显示文件列表
echo ""
echo "文件列表:"
ls -la "$OUTPUT_DIR/"
