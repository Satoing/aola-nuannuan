# ===========================================================================
# 奥拉暖暖 - Android NDK CMake 构建脚本
# ===========================================================================
cmake_minimum_required(VERSION 3.22.1)
project(aola_nuannuan LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ===========================================================================
# SDL2 依赖（使用预编译库或源码编译）
# ===========================================================================
# 方案一: 使用 SDL2 源码编译（推荐，确保版本兼容）
# 需要将 SDL2、SDL2_image、SDL2_ttf 源码放到 jni/ 目录下
# 方案二: 使用预编译 AAR（Android Archive）

# SDL2 源码路径（需要用户配置）
set(SDL2_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/SDL2" CACHE PATH "SDL2 source directory")
set(SDL2_IMAGE_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/SDL2_image" CACHE PATH "SDL2_image source directory")
set(SDL2_TTF_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/SDL2_ttf" CACHE PATH "SDL2_ttf source directory")

# 检查 SDL2 源码是否存在
if(EXISTS "${SDL2_SRC_DIR}/CMakeLists.txt")
    message(STATUS "使用 SDL2 源码编译: ${SDL2_SRC_DIR}")
    add_subdirectory(${SDL2_SRC_DIR} SDL2)
else()
    message(FATAL_ERROR "未找到 SDL2 源码！请将 SDL2 源码放到 ${SDL2_SRC_DIR}")
endif()

if(EXISTS "${SDL2_IMAGE_SRC_DIR}/CMakeLists.txt")
    message(STATUS "使用 SDL2_image 源码编译: ${SDL2_IMAGE_SRC_DIR}")
    # SDL2_image 需要知道 SDL2 的位置
    set(SDL2_DIR "${SDL2_SRC_DIR}")
    add_subdirectory(${SDL2_IMAGE_SRC_DIR} SDL2_image)
else()
    message(FATAL_ERROR "未找到 SDL2_image 源码！请将 SDL2_image 源码放到 ${SDL2_IMAGE_SRC_DIR}")
endif()

if(EXISTS "${SDL2_TTF_SRC_DIR}/CMakeLists.txt")
    message(STATUS "使用 SDL2_ttf 源码编译: ${SDL2_TTF_SRC_DIR}")
    add_subdirectory(${SDL2_TTF_SRC_DIR} SDL2_ttf)
else()
    message(FATAL_ERROR "未找到 SDL2_ttf 源码！请将 SDL2_ttf 源码放到 ${SDL2_TTF_SRC_DIR}")
endif()

# ===========================================================================
# nlohmann/json（header-only，直接包含即可）
# ===========================================================================
set(JSON_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/nlohmann" CACHE PATH "nlohmann/json include directory")
if(NOT EXISTS "${JSON_INCLUDE_DIR}/nlohmann/json.hpp")
    message(FATAL_ERROR "未找到 nlohmann/json.hpp！请将 nlohmann 目录放到 ${JSON_INCLUDE_DIR}")
endif()

# ===========================================================================
# SQLite3（使用 amalgamation 源码编译）
# ===========================================================================
set(SQLITE3_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/sqlite3" CACHE PATH "SQLite3 amalgamation directory")
if(EXISTS "${SQLITE3_SRC_DIR}/sqlite3.c")
    message(STATUS "使用 SQLite3 amalgamation 源码编译: ${SQLITE3_SRC_DIR}")
    add_library(sqlite3 STATIC ${SQLITE3_SRC_DIR}/sqlite3.c)
    target_include_directories(sqlite3 PUBLIC ${SQLITE3_SRC_DIR})
    target_compile_definitions(sqlite3 PRIVATE
        SQLITE_THREADSAFE=1
        SQLITE_ENABLE_FTS5
    )
else()
    message(FATAL_ERROR "未找到 SQLite3 源码！请将 sqlite3.c 和 sqlite3.h 放到 ${SQLITE3_SRC_DIR}")
endif()

# ===========================================================================
# 游戏源文件
# ===========================================================================
# 游戏 C++ 源码路径（指向项目根目录的 src 目录）
set(GAME_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../src")

set(GAME_SOURCES
    ${GAME_SRC_DIR}/main.cpp
    ${GAME_SRC_DIR}/ConfigLoader.cpp
    ${GAME_SRC_DIR}/ResourceManager.cpp
    ${GAME_SRC_DIR}/AvatarRenderer.cpp
    ${GAME_SRC_DIR}/Game.cpp
    # Android 平台适配层
    ${CMAKE_CURRENT_SOURCE_DIR}/android_platform.cpp
)

# ===========================================================================
# 构建共享库
# ===========================================================================
add_library(${PROJECT_NAME} SHARED ${GAME_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${GAME_SRC_DIR}
    ${SDL2_SRC_DIR}/include
    ${SDL2_IMAGE_SRC_DIR}
    ${SDL2_TTF_SRC_DIR}
    ${JSON_INCLUDE_DIR}
    ${SQLITE3_SRC_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    SDL2
    SDL2_image
    SDL2_ttf
    sqlite3
    android
    log
)

# 定义 ANDROID 平台宏
target_compile_definitions(${PROJECT_NAME} PRIVATE
    ANDROID_PLATFORM=1
)
