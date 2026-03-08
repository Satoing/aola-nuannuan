# MinGW-w64 交叉编译工具链文件（macOS 上使用 Homebrew 安装的 mingw-w64）
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# 交叉编译器（Homebrew 安装路径）
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)

# 依赖库路径
set(MINGW_DEPS_DIR "${CMAKE_CURRENT_LIST_DIR}/../mingw-deps/prefix")

# 搜索路径设置
set(CMAKE_FIND_ROOT_PATH "${MINGW_DEPS_DIR}" "/opt/homebrew/toolchains/x86_64-w64-mingw32")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# pkg-config 配置
set(ENV{PKG_CONFIG_PATH} "${MINGW_DEPS_DIR}/lib/pkgconfig")
set(ENV{PKG_CONFIG_LIBDIR} "${MINGW_DEPS_DIR}/lib/pkgconfig")
set(PKG_CONFIG_USE_CMAKE_PREFIX_PATH FALSE)
