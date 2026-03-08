# 奥拉暖暖 (AolaNuanNuan)

基于 SDL2 + C++17，支持 Windows、macOS 和 Android。

从奥拉星服务器加载服装配置和图片资源，提供可视化换装体验，支持套装一键穿戴、装扮方案保存/加载/分享等功能。

## 项目目录结构

```
aola-nuannuan/
├── CMakeLists.txt              # 主 CMake 构建脚本（Desktop 端）
├── README.md                   # 项目说明
├── .gitignore                  # Git 忽略规则
├── icon.png                    # 应用图标
│
├── src/                        # C++ 源码
│   ├── main.cpp                # 程序入口
│   ├── Game.cpp / Game.h       # 游戏主类（UI、事件、渲染）
│   ├── AvatarRenderer.cpp/.h   # 角色渲染器（图层合成）
│   ├── ConfigLoader.cpp/.h     # 配置加载（网络/本地 JSON）
│   ├── ResourceManager.cpp/.h  # 资源管理（图片缓存、异步下载）
│   └── ClothDefine.h           # 服装类型定义
│
├── data/                       # 游戏数据文件
│   ├── clothesdata.json        # 服装数据
│   └── clothespartdata.json    # 服装部件数据
│
├── cmake/                      # CMake 工具链文件
│   └── toolchain-mingw64.cmake # MinGW-w64 交叉编译工具链
│
├── scripts/                    # 构建 & 打包脚本
│   ├── make_macos_portable.sh  # macOS .app 打包脚本
│   ├── make_windows_portable.sh# Windows Portable 一键打包（Docker 方式）
│   ├── build_windows.sh        # Windows 交叉编译脚本（Docker 内执行）
│   ├── build_windows_native.bat# Windows 原生构建脚本（vcpkg）
│   └── Dockerfile.windows      # Docker 交叉编译环境
│
├── android/                    # Android 工程
│   ├── app/
│   │   ├── build.gradle.kts
│   │   └── src/main/
│   │       ├── AndroidManifest.xml
│   │       ├── jni/            # NDK 原生代码
│   │       │   ├── CMakeLists.txt
│   │       │   ├── android_platform.cpp/.h
│   │       │   ├── setup_deps.sh   # 依赖下载脚本
│   │       │   ├── SDL2/           # (自动下载)
│   │       │   ├── SDL2_image/     # (自动下载)
│   │       │   ├── SDL2_ttf/       # (自动下载)
│   │       │   ├── nlohmann/       # (自动下载)
│   │       │   └── sqlite3/        # (自动下载)
│   │       ├── assets/         # 游戏资源
│   │       ├── java/           # Java 代码
│   │       └── res/            # Android 资源
│   ├── build.gradle.kts
│   ├── settings.gradle.kts
│   └── gradlew / gradlew.bat
│
└── mingw-deps/                 # MinGW 交叉编译依赖（不纳入版本控制）
```

## 依赖说明

| 依赖库 | 版本 | 用途 |
|--------|------|------|
| SDL2 | 2.28+ | 窗口管理、事件处理、渲染 |
| SDL2_image | 2.8+ | 图片加载（PNG 等格式） |
| SDL2_ttf | 2.22+ | 字体渲染（TTF/TTC） |
| nlohmann/json | 3.11+ | JSON 解析 |
| libcurl | 8.x | 网络请求（下载配置和图片） |
| SQLite3 | 3.47+ | 本地数据存储（装扮方案） |

---

## 打包方式

### macOS

**环境要求：**
- macOS 11.0+
- Xcode Command Line Tools
- Homebrew

**安装依赖：**

```bash
brew install sdl2 sdl2_image sdl2_ttf nlohmann-json curl sqlite pkg-config cmake
```

**编译 & 打包：**

```bash
# 一键构建 macOS Portable .app
bash scripts/make_macos_portable.sh
```

脚本会自动完成以下步骤：
1. CMake 配置 & 编译（Release 模式）
2. 创建 `.app` bundle 目录结构
3. 收集所有 Homebrew 动态库（dylib）到 `Frameworks/`
4. 修正动态库路径（`install_name_tool`）
5. Ad-hoc 代码签名
6. 打包为 `AolaDressUp-macOS-Portable.zip`

**产物：**
- `AolaDressUp.app` — 可直接双击运行
- `AolaDressUp-macOS-Portable.zip` — 可分发的压缩包

> ⚠️ 首次打开时 macOS 可能提示安全警告，请在 **系统设置 > 隐私与安全** 中点击「仍要打开」

---

### Windows

提供两种构建方式：

#### 方式一：Docker 交叉编译（推荐，在 macOS/Linux 上操作）

**环境要求：**
- Docker Desktop

```bash
# 一键构建 Windows Portable 版
bash scripts/make_windows_portable.sh
```

脚本会自动完成以下步骤：
1. 构建 Docker 编译环境（基于 Ubuntu + MinGW-w64）
2. 在容器中交叉编译
3. 收集所有 DLL 依赖
4. 打包为 `AolaNuanNuan-Windows-Portable.zip`

**产物：**
- `AolaNuanNuan-Windows-Portable.zip` — 解压到 Windows 电脑即可运行

#### 方式二：Windows 原生构建（在 Windows 上操作）

**环境要求：**
- Visual Studio 2019/2022（含 C++ 桌面开发工作负载）
- [vcpkg](https://github.com/microsoft/vcpkg)
- CMake 3.16+

**安装依赖：**

```bat
REM 克隆并初始化 vcpkg
git clone https://github.com/microsoft/vcpkg
cd vcpkg && bootstrap-vcpkg.bat
set VCPKG_ROOT=%CD%

REM 安装依赖包
vcpkg install sdl2:x64-windows sdl2-image:x64-windows sdl2-ttf:x64-windows
vcpkg install nlohmann-json:x64-windows curl:x64-windows sqlite3:x64-windows
```

**编译 & 打包：**

```bat
scripts\build_windows_native.bat
```

**产物：**
- `AolaNuanNuan-Windows-Portable.zip`

---

### Android

**环境要求：**
- Android Studio（Hedgehog 或更新版本）
- Android NDK r25+（通过 SDK Manager 安装 `25.2.9519653`）
- CMake 3.22.1+（通过 SDK Manager 安装）
- JDK 17

**步骤一：下载 NDK 依赖**

```bash
cd android/app/src/main/jni
bash setup_deps.sh
```

此脚本会自动下载并配置：
- SDL2 源码
- SDL2_image 源码
- SDL2_ttf 源码
- nlohmann/json 头文件
- SQLite3 amalgamation 源码
- SDL2 Java 层代码
- 中文字体（Noto Sans SC）
- 游戏数据文件

**步骤二：构建 APK**

```bash
# Debug 版
cd android
./gradlew assembleDebug

# Release 版
./gradlew assembleRelease
```

或直接在 Android Studio 中打开 `android/` 目录，点击 Run 即可。

**产物：**
- Debug: `android/app/build/outputs/apk/debug/app-debug.apk`
- Release: `android/app/build/outputs/apk/release/app-release-unsigned.apk`

**支持架构：**
- `arm64-v8a`（主流 ARM 64位）
- `armeabi-v7a`（旧款 ARM 32位）
- `x86_64`（模拟器）

---

## 操作说明

| 操作 | 桌面端 | 移动端 |
|------|--------|--------|
| 切换分类 | 点击左侧分类标签 | 点击左侧分类标签 |
| 穿脱服装 | 点击服装卡片 | 点击服装卡片 |
| 滚动列表 | 鼠标滚轮 | 手指上下滑动 |
| 切换人模 | 点击右上角按钮 | 点击右上角按钮 |
| 搜索服装 | 按 F 键 | — |
| 重置装扮 | 按 R 键 | — |
| 退出 | 按 ESC 键 | 系统返回键 |

## 许可证

本项目仅供学习交流使用，游戏素材和数据版权归原作者所有。
