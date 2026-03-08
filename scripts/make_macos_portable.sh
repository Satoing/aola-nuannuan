#!/bin/bash
# make_macos_portable.sh - 构建 macOS Portable 应用 (.app bundle)
# 使用 dylibbundler 或手动方式将所有 homebrew 依赖打包进 .app
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_DIR"

APP_NAME="奥拉暖暖"
BUNDLE_NAME="AolaNuanNuan"
BUILD_DIR="build-portable"
APP_DIR="${BUNDLE_NAME}.app"
CONTENTS="${APP_DIR}/Contents"
MACOS_DIR="${CONTENTS}/MacOS"
RES_DIR="${CONTENTS}/Resources"
FRAMEWORKS_DIR="${CONTENTS}/Frameworks"

echo "========================================="
echo "  奥拉暖暖 - macOS Portable 构建"
echo "========================================="
echo ""

# ============================================================
# Step 1: 编译
# ============================================================
echo "[1/5] 编译..."
if [ ! -d "$BUILD_DIR" ]; then
    cmake -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
fi
cmake --build "$BUILD_DIR" -j$(sysctl -n hw.ncpu) -- 2>&1 | tail -3

if [ ! -f "${BUILD_DIR}/${BUNDLE_NAME}" ]; then
    echo "错误: 编译产物 ${BUILD_DIR}/${BUNDLE_NAME} 不存在!"
    exit 1
fi

# ============================================================
# Step 2: 创建 .app 目录结构
# ============================================================
echo "[2/5] 创建 .app bundle 结构..."
rm -rf "$APP_DIR"
mkdir -p "$MACOS_DIR" "$RES_DIR" "$FRAMEWORKS_DIR"

# 复制可执行文件
cp "${BUILD_DIR}/${BUNDLE_NAME}" "$MACOS_DIR/"

# 复制图标
cp icon.png "$RES_DIR/" 2>/dev/null || true

# 如果有 .icns 图标就用，没有就从 png 生成
if [ -f "icon.icns" ]; then
    cp icon.icns "$RES_DIR/AppIcon.icns"
elif [ -f "icon.png" ]; then
    echo "  从 icon.png 生成 .icns 图标..."
    ICONSET_DIR="/tmp/AppIcon.iconset"
    mkdir -p "$ICONSET_DIR"
    for size in 16 32 64 128 256 512; do
        sips -z $size $size icon.png --out "${ICONSET_DIR}/icon_${size}x${size}.png" > /dev/null 2>&1
        double=$((size * 2))
        sips -z $double $double icon.png --out "${ICONSET_DIR}/icon_${size}x${size}@2x.png" > /dev/null 2>&1
    done
    iconutil -c icns "$ICONSET_DIR" -o "$RES_DIR/AppIcon.icns" 2>/dev/null || true
    rm -rf "$ICONSET_DIR"
fi

# ============================================================
# Step 3: 创建 Info.plist
# ============================================================
echo "[3/5] 生成 Info.plist..."
cat > "${CONTENTS}/Info.plist" << 'PLIST'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleDevelopmentRegion</key>
    <string>zh_CN</string>
    <key>CFBundleExecutable</key>
    <string>AolaDressUp</string>
    <key>CFBundleIconFile</key>
    <string>AppIcon</string>
    <key>CFBundleIdentifier</key>
    <string>com.aola.dressup</string>
    <key>CFBundleInfoDictionaryVersion</key>
    <string>6.0</string>
    <key>CFBundleName</key>
    <string>奥拉暖暖</string>
    <key>CFBundleDisplayName</key>
    <string>奥拉暖暖</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleShortVersionString</key>
    <string>1.0.0</string>
    <key>CFBundleVersion</key>
    <string>1</string>
    <key>LSMinimumSystemVersion</key>
    <string>11.0</string>
    <key>NSHighResolutionCapable</key>
    <true/>
    <key>NSSupportsAutomaticGraphicsSwitching</key>
    <true/>
    <key>NSAppTransportSecurity</key>
    <dict>
        <key>NSAllowsArbitraryLoads</key>
        <true/>
    </dict>
</dict>
</plist>
PLIST

# ============================================================
# Step 4: 收集并打包所有 dylib 依赖
# ============================================================
echo "[4/5] 收集动态库依赖..."

EXECUTABLE="${MACOS_DIR}/${BUNDLE_NAME}"

# 递归收集所有非系统 dylib
collect_dylibs() {
    local binary="$1"
    local processed="$2"  # 已处理的列表文件
    
    # 获取所有依赖
    otool -L "$binary" 2>/dev/null | tail -n +2 | awk '{print $1}' | while read -r lib; do
        # 跳过系统库
        case "$lib" in
            /usr/lib/*|/System/*|@rpath/*|@executable_path/*|@loader_path/*)
                continue
                ;;
        esac
        
        # 跳过已处理的
        if grep -qF "$lib" "$processed" 2>/dev/null; then
            continue
        fi
        
        echo "$lib" >> "$processed"
        
        # 真实路径（解析符号链接）
        local reallib="$lib"
        if [ -L "$lib" ]; then
            reallib=$(readlink -f "$lib")
        fi
        
        if [ -f "$reallib" ]; then
            local libname=$(basename "$lib")
            echo "  复制: $libname"
            cp -f "$reallib" "${FRAMEWORKS_DIR}/${libname}"
            
            # 递归处理这个 dylib 的依赖
            collect_dylibs "$reallib" "$processed"
        fi
    done
}

PROCESSED_FILE=$(mktemp)
collect_dylibs "$EXECUTABLE" "$PROCESSED_FILE"
rm -f "$PROCESSED_FILE"

# 确保所有复制的 dylib 都可写（homebrew 的 dylib 可能是只读的）
chmod -R u+w "${FRAMEWORKS_DIR}/" 2>/dev/null || true

# ============================================================
# Step 5: 修正 dylib 路径 (install_name_tool)
# ============================================================
echo "[5/5] 修正动态库路径..."

# 修正可执行文件中的库路径
for dylib in "${FRAMEWORKS_DIR}"/*.dylib; do
    [ -f "$dylib" ] || continue
    libname=$(basename "$dylib")
    old_path=$(otool -L "$EXECUTABLE" | grep "$libname" | head -1 | awk '{print $1}')
    if [ -n "$old_path" ]; then
        install_name_tool -change "$old_path" "@executable_path/../Frameworks/${libname}" "$EXECUTABLE" 2>/dev/null || true
    fi
done

# 修正每个 dylib 自身的 install name 和内部依赖
for dylib in "${FRAMEWORKS_DIR}"/*.dylib; do
    [ -f "$dylib" ] || continue
    libname=$(basename "$dylib")
    
    # 修正自身 install name
    install_name_tool -id "@executable_path/../Frameworks/${libname}" "$dylib" 2>/dev/null || true
    
    # 修正内部对其他 dylib 的引用
    for other_dylib in "${FRAMEWORKS_DIR}"/*.dylib; do
        [ -f "$other_dylib" ] || continue
        other_name=$(basename "$other_dylib")
        [ "$libname" = "$other_name" ] && continue
        
        old_ref=$(otool -L "$dylib" | grep "$other_name" | head -1 | awk '{print $1}')
        if [ -n "$old_ref" ]; then
            case "$old_ref" in
                /usr/lib/*|/System/*|@rpath/*|@executable_path/*|@loader_path/*)
                    ;;
                *)
                    install_name_tool -change "$old_ref" "@executable_path/../Frameworks/${other_name}" "$dylib" 2>/dev/null || true
                    ;;
            esac
        fi
    done
done

# 对所有文件逐个清除扩展属性后重签（ad-hoc）
echo "  清除扩展属性并签名..."
# 先处理 Frameworks 中的所有 dylib
for f in "${FRAMEWORKS_DIR}"/*.dylib; do
    [ -f "$f" ] || continue
    xattr -c "$f" 2>/dev/null || true
    codesign --force --sign - "$f" 2>/dev/null || true
done
# 处理可执行文件
xattr -c "$EXECUTABLE" 2>/dev/null || true
codesign --force --sign - "$EXECUTABLE" 2>/dev/null || true
# 处理 Resources 中的文件
for f in "${RES_DIR}"/*; do
    [ -f "$f" ] || continue
    xattr -c "$f" 2>/dev/null || true
done
# 最后签名整个 .app
xattr -c "${CONTENTS}/Info.plist" 2>/dev/null || true
codesign --force --sign - "$APP_DIR" 2>/dev/null || true

# 验证签名
if codesign -v --deep "$APP_DIR" 2>/dev/null; then
    echo "  签名验证通过 ✓"
else
    echo "  注意: ad-hoc 签名，首次打开需在安全设置中允许"
fi

# ============================================================
# 验证
# ============================================================
echo ""
echo "验证依赖路径..."
echo "--- 可执行文件依赖 ---"
otool -L "$EXECUTABLE" | head -15

echo ""
echo "--- Frameworks 内容 ---"
ls -lh "${FRAMEWORKS_DIR}/"

# 创建 DMG 或 ZIP
echo ""
echo "打包 ZIP..."
ZIP_NAME="${BUNDLE_NAME}-macOS-Portable.zip"
rm -f "$ZIP_NAME"
ditto -c -k --keepParent "$APP_DIR" "$ZIP_NAME"

echo ""
echo "========================================="
echo "  构建完成！"
echo "========================================="
echo ""
echo "  .app 路径: ${APP_DIR}"
echo "  ZIP 路径:  ${ZIP_NAME}"
echo "  ZIP 大小:  $(du -h "$ZIP_NAME" | cut -f1)"
echo ""
echo "  使用方法:"
echo "    1. 双击 ${APP_DIR} 直接运行"
echo "    2. 或将 ${ZIP_NAME} 分享给其他 Mac 用户"
echo "       解压后双击即可运行（无需安装任何依赖）"
echo ""
echo "  注意: 首次打开时 macOS 可能提示安全警告"
echo "        请在 系统设置 > 隐私与安全 中点击「仍要打开」"
echo "========================================="
