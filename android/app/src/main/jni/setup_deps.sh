#!/bin/bash
# ===========================================================================
# setup_deps.sh - 自动下载并配置 Android 编译所需的第三方依赖
# 使用方法: cd android/app/src/main/jni && bash setup_deps.sh
# ===========================================================================

set -e

JNI_DIR="$(cd "$(dirname "$0")" && pwd)"
echo "工作目录: $JNI_DIR"

# ----- SDL2 -----
SDL2_VERSION="2.28.5"
if [ ! -d "$JNI_DIR/SDL2" ]; then
    echo "下载 SDL2 ${SDL2_VERSION}..."
    curl -L -o /tmp/SDL2.tar.gz "https://github.com/libsdl-org/SDL/releases/download/release-${SDL2_VERSION}/SDL2-${SDL2_VERSION}.tar.gz"
    tar xzf /tmp/SDL2.tar.gz -C "$JNI_DIR"
    mv "$JNI_DIR/SDL2-${SDL2_VERSION}" "$JNI_DIR/SDL2"
    rm /tmp/SDL2.tar.gz
    echo "SDL2 已就绪"
else
    echo "SDL2 已存在，跳过"
fi

# ----- SDL2_image -----
SDL2_IMAGE_VERSION="2.8.2"
if [ ! -d "$JNI_DIR/SDL2_image" ]; then
    echo "下载 SDL2_image ${SDL2_IMAGE_VERSION}..."
    curl -L -o /tmp/SDL2_image.tar.gz "https://github.com/libsdl-org/SDL_image/releases/download/release-${SDL2_IMAGE_VERSION}/SDL2_image-${SDL2_IMAGE_VERSION}.tar.gz"
    tar xzf /tmp/SDL2_image.tar.gz -C "$JNI_DIR"
    mv "$JNI_DIR/SDL2_image-${SDL2_IMAGE_VERSION}" "$JNI_DIR/SDL2_image"
    rm /tmp/SDL2_image.tar.gz
    echo "SDL2_image 已就绪"
else
    echo "SDL2_image 已存在，跳过"
fi

# ----- SDL2_ttf -----
SDL2_TTF_VERSION="2.22.0"
if [ ! -d "$JNI_DIR/SDL2_ttf" ]; then
    echo "下载 SDL2_ttf ${SDL2_TTF_VERSION}..."
    curl -L -o /tmp/SDL2_ttf.tar.gz "https://github.com/libsdl-org/SDL_ttf/releases/download/release-${SDL2_TTF_VERSION}/SDL2_ttf-${SDL2_TTF_VERSION}.tar.gz"
    tar xzf /tmp/SDL2_ttf.tar.gz -C "$JNI_DIR"
    mv "$JNI_DIR/SDL2_ttf-${SDL2_TTF_VERSION}" "$JNI_DIR/SDL2_ttf"
    rm /tmp/SDL2_ttf.tar.gz
    echo "SDL2_ttf 已就绪"
else
    echo "SDL2_ttf 已存在，跳过"
fi

# ----- nlohmann/json (header-only) -----
if [ ! -f "$JNI_DIR/nlohmann/nlohmann/json.hpp" ]; then
    echo "下载 nlohmann/json..."
    mkdir -p "$JNI_DIR/nlohmann/nlohmann"
    curl -L -o "$JNI_DIR/nlohmann/nlohmann/json.hpp" "https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp"
    echo "nlohmann/json 已就绪"
else
    echo "nlohmann/json 已存在，跳过"
fi

# ----- SQLite3 amalgamation -----
if [ ! -f "$JNI_DIR/sqlite3/sqlite3.c" ]; then
    echo "下载 SQLite3 amalgamation..."
    mkdir -p "$JNI_DIR/sqlite3"
    SQLITE_URL="https://www.sqlite.org/2024/sqlite-amalgamation-3450100.zip"
    curl -L -o /tmp/sqlite3.zip "$SQLITE_URL"
    unzip -q -o /tmp/sqlite3.zip -d /tmp/sqlite3_tmp
    cp /tmp/sqlite3_tmp/sqlite-amalgamation-*/sqlite3.c "$JNI_DIR/sqlite3/"
    cp /tmp/sqlite3_tmp/sqlite-amalgamation-*/sqlite3.h "$JNI_DIR/sqlite3/"
    cp /tmp/sqlite3_tmp/sqlite-amalgamation-*/sqlite3ext.h "$JNI_DIR/sqlite3/" 2>/dev/null || true
    rm -rf /tmp/sqlite3.zip /tmp/sqlite3_tmp
    echo "SQLite3 已就绪"
else
    echo "SQLite3 已存在，跳过"
fi

# ----- SDL2 Java 源码（Android Activity） -----
SDL2_JAVA_SRC="$JNI_DIR/SDL2/android-project/app/src/main/java/org/libsdl/app"
TARGET_JAVA_DIR="$JNI_DIR/../java/org/libsdl/app"
if [ -d "$SDL2_JAVA_SRC" ] && [ ! -d "$TARGET_JAVA_DIR" ]; then
    echo "复制 SDL2 Java 源码..."
    mkdir -p "$TARGET_JAVA_DIR"
    cp -r "$SDL2_JAVA_SRC/"*.java "$TARGET_JAVA_DIR/"
    echo "SDL2 Java 源码已复制到: $TARGET_JAVA_DIR"
else
    if [ -d "$TARGET_JAVA_DIR" ]; then
        echo "SDL2 Java 源码已存在，跳过"
    else
        echo "警告: 未找到 SDL2 Java 源码，请手动复制"
    fi
fi

# ----- 中文字体 -----
FONT_DIR="$JNI_DIR/../assets/fonts"
if [ ! -f "$FONT_DIR/NotoSansSC-Regular.ttf" ]; then
    echo "下载 Noto Sans SC 中文字体..."
    mkdir -p "$FONT_DIR"
    curl -L -o "$FONT_DIR/NotoSansSC-Regular.ttf" "https://github.com/google/fonts/raw/main/ofl/notosanssc/NotoSansSC%5Bwght%5D.ttf" 2>/dev/null || \
    curl -L -o "$FONT_DIR/NotoSansSC-Regular.ttf" "https://github.com/googlefonts/noto-cjk/raw/main/Sans/Variable/TTF/NotoSansSC-VF.ttf" 2>/dev/null || \
    echo "警告: 中文字体下载失败，请手动放置字体文件到 $FONT_DIR"
    echo "中文字体已就绪"
else
    echo "中文字体已存在，跳过"
fi

# ----- 复制游戏数据到 assets -----
ASSETS_DIR="$JNI_DIR/../assets"
GAME_DATA_DIR="$JNI_DIR/../../../../.."
if [ ! -f "$ASSETS_DIR/clothesdata.json" ]; then
    if [ -f "$GAME_DATA_DIR/clothesdata.json" ]; then
        echo "复制游戏数据到 assets..."
        cp "$GAME_DATA_DIR/clothesdata.json" "$ASSETS_DIR/"
        cp "$GAME_DATA_DIR/clothespartdata.json" "$ASSETS_DIR/"
        echo "游戏数据已复制"
    elif [ -f "$JNI_DIR/../../../../clothesdata.json" ]; then
        cp "$JNI_DIR/../../../../clothesdata.json" "$ASSETS_DIR/"
        cp "$JNI_DIR/../../../../clothespartdata.json" "$ASSETS_DIR/"
        echo "游戏数据已复制"
    else
        echo "警告: 未找到游戏数据文件，请手动复制 clothesdata.json 和 clothespartdata.json 到 $ASSETS_DIR"
    fi
else
    echo "游戏数据已存在，跳过"
fi

echo ""
echo "============================================"
echo "  依赖配置完成！"
echo "============================================"
echo ""
echo "目录结构:"
echo "  jni/SDL2/           - SDL2 源码"
echo "  jni/SDL2_image/     - SDL2_image 源码"
echo "  jni/SDL2_ttf/       - SDL2_ttf 源码"
echo "  jni/nlohmann/       - nlohmann/json"
echo "  assets/fonts/       - 中文字体"
echo "  assets/*.json       - 游戏数据"
echo ""
echo "接下来请执行:"
echo "  1. 安装 Android Studio + NDK (r25+)"
echo "  2. cd android && ./gradlew assembleDebug"
echo ""
