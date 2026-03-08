#!/bin/bash
# build_windows.sh - 在 macOS 上使用 MinGW 交叉编译 Windows 版本
# 前置条件: brew install mingw-w64 cmake pkg-config
set -e

echo "========================================="
echo "  奥拉暖暖 - Windows 交叉编译构建"
echo "  (macOS MinGW-w64)"
echo "========================================="

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$PROJECT_DIR"

DEPS_DIR="$PROJECT_DIR/mingw-deps"
PREFIX="$DEPS_DIR/prefix"
DOWNLOAD_DIR="$DEPS_DIR/download"
BUILD_DIR="build-windows"
OUTPUT_DIR="AolaNuanNuan-Windows-Portable"

CROSS=x86_64-w64-mingw32
MINGW_SYSROOT="$(${CROSS}-gcc -print-sysroot)"

# SDL2 预编译包版本
SDL2_VERSION="2.30.12"
SDL2_IMAGE_VERSION="2.8.4"
SDL2_TTF_VERSION="2.22.0"
CURL_VERSION="8.12.1"
SQLITE_YEAR="2025"
SQLITE_VERSION="3490100"
NLOHMANN_JSON_VERSION="3.11.3"

# ========== 辅助函数 ==========
download_if_not_exists() {
    local url="$1"
    local dest="$2"
    if [ ! -f "$dest" ]; then
        echo "  下载: $(basename "$dest")"
        curl -L -o "$dest" "$url"
    else
        echo "  已缓存: $(basename "$dest")"
    fi
}

# ========== 1. 下载和安装依赖 ==========
echo ""
echo "[1/4] 准备 Windows 依赖库..."
mkdir -p "$PREFIX"/{include,lib,bin} "$DOWNLOAD_DIR"

# --- SDL2 ---
echo "  --- SDL2 ---"
SDL2_ARCHIVE="SDL2-devel-${SDL2_VERSION}-mingw.tar.gz"
download_if_not_exists \
    "https://github.com/libsdl-org/SDL/releases/download/release-${SDL2_VERSION}/${SDL2_ARCHIVE}" \
    "$DOWNLOAD_DIR/$SDL2_ARCHIVE"
if [ ! -f "$PREFIX/lib/libSDL2.dll.a" ]; then
    tar xf "$DOWNLOAD_DIR/$SDL2_ARCHIVE" -C "$DOWNLOAD_DIR"
    cp -r "$DOWNLOAD_DIR/SDL2-${SDL2_VERSION}/x86_64-w64-mingw32/include/"* "$PREFIX/include/"
    cp -r "$DOWNLOAD_DIR/SDL2-${SDL2_VERSION}/x86_64-w64-mingw32/lib/"* "$PREFIX/lib/"
    cp -r "$DOWNLOAD_DIR/SDL2-${SDL2_VERSION}/x86_64-w64-mingw32/bin/"* "$PREFIX/bin/"
    # 复制 pkg-config 文件并修正路径
    mkdir -p "$PREFIX/lib/pkgconfig"
    for pc in "$DOWNLOAD_DIR/SDL2-${SDL2_VERSION}/x86_64-w64-mingw32/lib/pkgconfig/"*.pc; do
        if [ -f "$pc" ]; then
            sed "s|^prefix=.*|prefix=$PREFIX|" "$pc" > "$PREFIX/lib/pkgconfig/$(basename "$pc")"
        fi
    done
    echo "  SDL2 ${SDL2_VERSION} 安装完成"
fi

# --- SDL2_image ---
echo "  --- SDL2_image ---"
SDL2_IMAGE_ARCHIVE="SDL2_image-devel-${SDL2_IMAGE_VERSION}-mingw.tar.gz"
download_if_not_exists \
    "https://github.com/libsdl-org/SDL_image/releases/download/release-${SDL2_IMAGE_VERSION}/${SDL2_IMAGE_ARCHIVE}" \
    "$DOWNLOAD_DIR/$SDL2_IMAGE_ARCHIVE"
if [ ! -f "$PREFIX/lib/libSDL2_image.dll.a" ]; then
    tar xf "$DOWNLOAD_DIR/$SDL2_IMAGE_ARCHIVE" -C "$DOWNLOAD_DIR"
    cp -r "$DOWNLOAD_DIR/SDL2_image-${SDL2_IMAGE_VERSION}/x86_64-w64-mingw32/include/"* "$PREFIX/include/"
    cp -r "$DOWNLOAD_DIR/SDL2_image-${SDL2_IMAGE_VERSION}/x86_64-w64-mingw32/lib/"* "$PREFIX/lib/"
    cp -r "$DOWNLOAD_DIR/SDL2_image-${SDL2_IMAGE_VERSION}/x86_64-w64-mingw32/bin/"* "$PREFIX/bin/"
    for pc in "$DOWNLOAD_DIR/SDL2_image-${SDL2_IMAGE_VERSION}/x86_64-w64-mingw32/lib/pkgconfig/"*.pc; do
        if [ -f "$pc" ]; then
            sed "s|^prefix=.*|prefix=$PREFIX|" "$pc" > "$PREFIX/lib/pkgconfig/$(basename "$pc")"
        fi
    done
    echo "  SDL2_image ${SDL2_IMAGE_VERSION} 安装完成"
fi

# --- SDL2_ttf ---
echo "  --- SDL2_ttf ---"
SDL2_TTF_ARCHIVE="SDL2_ttf-devel-${SDL2_TTF_VERSION}-mingw.tar.gz"
download_if_not_exists \
    "https://github.com/libsdl-org/SDL_ttf/releases/download/release-${SDL2_TTF_VERSION}/${SDL2_TTF_ARCHIVE}" \
    "$DOWNLOAD_DIR/$SDL2_TTF_ARCHIVE"
if [ ! -f "$PREFIX/lib/libSDL2_ttf.dll.a" ]; then
    tar xf "$DOWNLOAD_DIR/$SDL2_TTF_ARCHIVE" -C "$DOWNLOAD_DIR"
    cp -r "$DOWNLOAD_DIR/SDL2_ttf-${SDL2_TTF_VERSION}/x86_64-w64-mingw32/include/"* "$PREFIX/include/"
    cp -r "$DOWNLOAD_DIR/SDL2_ttf-${SDL2_TTF_VERSION}/x86_64-w64-mingw32/lib/"* "$PREFIX/lib/"
    cp -r "$DOWNLOAD_DIR/SDL2_ttf-${SDL2_TTF_VERSION}/x86_64-w64-mingw32/bin/"* "$PREFIX/bin/"
    for pc in "$DOWNLOAD_DIR/SDL2_ttf-${SDL2_TTF_VERSION}/x86_64-w64-mingw32/lib/pkgconfig/"*.pc; do
        if [ -f "$pc" ]; then
            sed "s|^prefix=.*|prefix=$PREFIX|" "$pc" > "$PREFIX/lib/pkgconfig/$(basename "$pc")"
        fi
    done
    echo "  SDL2_ttf ${SDL2_TTF_VERSION} 安装完成"
fi

# --- SQLite3 (从合并源码编译) ---
echo "  --- SQLite3 ---"
SQLITE_ARCHIVE="sqlite-amalgamation-${SQLITE_VERSION}.zip"
download_if_not_exists \
    "https://www.sqlite.org/${SQLITE_YEAR}/${SQLITE_ARCHIVE}" \
    "$DOWNLOAD_DIR/$SQLITE_ARCHIVE"
if [ ! -f "$PREFIX/lib/libsqlite3.a" ]; then
    cd "$DOWNLOAD_DIR"
    unzip -o "$SQLITE_ARCHIVE" > /dev/null
    cd "sqlite-amalgamation-${SQLITE_VERSION}"
    ${CROSS}-gcc -c -O2 -DSQLITE_ENABLE_JSON1 sqlite3.c -o sqlite3.o
    ${CROSS}-ar rcs libsqlite3.a sqlite3.o
    cp libsqlite3.a "$PREFIX/lib/"
    cp sqlite3.h sqlite3ext.h "$PREFIX/include/"
    # 生成 pkg-config 文件
    cat > "$PREFIX/lib/pkgconfig/sqlite3.pc" << PCEOF
prefix=$PREFIX
exec_prefix=\${prefix}
libdir=\${prefix}/lib
includedir=\${prefix}/include

Name: SQLite
Description: SQL database engine
Version: 3.49.1
Libs: -L\${libdir} -lsqlite3
Cflags: -I\${includedir}
PCEOF
    cd "$PROJECT_DIR"
    echo "  SQLite3 编译安装完成"
fi

# --- zlib (curl 依赖) ---
echo "  --- zlib ---"
ZLIB_VERSION="1.3.1"
ZLIB_ARCHIVE="zlib-${ZLIB_VERSION}.tar.gz"
download_if_not_exists \
    "https://github.com/madler/zlib/releases/download/v${ZLIB_VERSION}/${ZLIB_ARCHIVE}" \
    "$DOWNLOAD_DIR/$ZLIB_ARCHIVE"
if [ ! -f "$PREFIX/lib/libz.a" ]; then
    cd "$DOWNLOAD_DIR"
    tar xf "$ZLIB_ARCHIVE"
    cd "zlib-${ZLIB_VERSION}"
    # macOS 的 libtool 不兼容 MinGW 的 .o 文件，手动编译
    CC=${CROSS}-gcc AR=${CROSS}-ar RANLIB=${CROSS}-ranlib \
        ./configure --prefix="$PREFIX" --static > /dev/null 2>&1
    # 只编译源文件，不执行 make（避免 libtool 错误）
    for src in adler32.c crc32.c deflate.c infback.c inffast.c inflate.c \
               inftrees.c trees.c zutil.c compress.c uncompr.c \
               gzclose.c gzlib.c gzread.c gzwrite.c; do
        ${CROSS}-gcc -O3 -D_LARGEFILE64_SOURCE=1 -c "$src"
    done
    ${CROSS}-ar rcs libz.a *.o
    cp libz.a "$PREFIX/lib/"
    cp zlib.h zconf.h "$PREFIX/include/"
    cd "$PROJECT_DIR"
    echo "  zlib ${ZLIB_VERSION} 编译安装完成"
fi

# --- curl (从源码交叉编译，使用 WinSSL/Schannel) ---
echo "  --- curl ---"
CURL_ARCHIVE="curl-${CURL_VERSION}.tar.gz"
download_if_not_exists \
    "https://curl.se/download/${CURL_ARCHIVE}" \
    "$DOWNLOAD_DIR/$CURL_ARCHIVE"
if [ ! -f "$PREFIX/lib/libcurl.a" ]; then
    cd "$DOWNLOAD_DIR"
    tar xf "$CURL_ARCHIVE"
    cd "curl-${CURL_VERSION}"
    ./configure \
        --host=${CROSS} \
        --prefix="$PREFIX" \
        --with-schannel \
        --with-zlib="$PREFIX" \
        --disable-shared \
        --enable-static \
        --disable-ldap \
        --disable-manual \
        --disable-dict \
        --disable-telnet \
        --disable-tftp \
        --disable-pop3 \
        --disable-imap \
        --disable-smb \
        --disable-smtp \
        --disable-gopher \
        --disable-mqtt \
        --disable-rtsp \
        --without-libpsl \
        --without-brotli \
        --without-zstd \
        --without-libidn2 \
        --without-nghttp2 \
        CFLAGS="-O2 -I$PREFIX/include" \
        LDFLAGS="-L$PREFIX/lib" \
        LIBS="-lz" \
        > /dev/null 2>&1
    make -j$(sysctl -n hw.ncpu) > /dev/null 2>&1
    make install > /dev/null 2>&1
    cd "$PROJECT_DIR"
    echo "  curl ${CURL_VERSION} 编译安装完成"
fi

# --- nlohmann/json (header-only) ---
echo "  --- nlohmann/json ---"
NLOHMANN_HEADER="$PREFIX/include/nlohmann/json.hpp"
if [ ! -f "$NLOHMANN_HEADER" ]; then
    mkdir -p "$PREFIX/include/nlohmann"
    download_if_not_exists \
        "https://github.com/nlohmann/json/releases/download/v${NLOHMANN_JSON_VERSION}/json.hpp" \
        "$NLOHMANN_HEADER"
    echo "  nlohmann/json ${NLOHMANN_JSON_VERSION} 安装完成"
fi

echo "  所有依赖准备完成！"

# ========== 2. CMake 配置 & 编译 ==========
echo ""
echo "[2/4] CMake 配置..."
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

cmake -B "$BUILD_DIR" \
    -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-mingw64.cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$PREFIX" \
    -DPKG_CONFIG_EXECUTABLE="$(which pkg-config)"

echo ""
echo "[3/4] 编译..."
cmake --build "$BUILD_DIR" -j$(sysctl -n hw.ncpu)

# ========== 3. 打包 ==========
echo ""
echo "[4/4] 打包 Portable 应用..."
rm -rf "$OUTPUT_DIR" "${OUTPUT_DIR}.zip"
mkdir -p "$OUTPUT_DIR/fonts"

# 复制可执行文件
cp "$BUILD_DIR/AolaNuanNuan.exe" "$OUTPUT_DIR/"

# 复制图标
cp icon.png "$OUTPUT_DIR/" 2>/dev/null || true

# 复制 DLL（从预编译包中提取）
echo "  复制 DLL 文件..."
for dll in "$PREFIX/bin/"*.dll; do
    [ -f "$dll" ] && cp "$dll" "$OUTPUT_DIR/"
done

# 复制 MinGW 运行时 DLL
echo "  复制 MinGW 运行时 DLL..."
MINGW_LIB_DIR="$(dirname "$(${CROSS}-gcc -print-libgcc-file-name)")"
for dll in libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll; do
    find "$(dirname "$MINGW_LIB_DIR")" -name "$dll" -exec cp {} "$OUTPUT_DIR/" \; 2>/dev/null
done

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
echo "  压缩..."
zip -r "${OUTPUT_DIR}.zip" "$OUTPUT_DIR/" -x "*.DS_Store" > /dev/null

# 清理打包目录
rm -rf "$OUTPUT_DIR"

ZIP_SIZE=$(du -h "${OUTPUT_DIR}.zip" | cut -f1)
echo ""
echo "========================================="
echo "  构建完成！"
echo "  输出: ${OUTPUT_DIR}.zip (${ZIP_SIZE})"
echo "========================================="
