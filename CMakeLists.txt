cmake_minimum_required(VERSION 3.16)
project(AolaNuanNuan LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows 特殊处理: 使用 GUI 子系统避免弹出控制台
if(WIN32)
    # 如果需要看控制台输出用于调试，注释掉下面这行
    set(CMAKE_WIN32_EXECUTABLE ON)
endif()

# 查找依赖包
if(WIN32 AND MINGW)
    # MinGW 交叉编译: 使用 MINGW_DEPS_DIR 指定依赖目录
    if(NOT DEFINED MINGW_DEPS_DIR)
        set(MINGW_DEPS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/mingw-deps/prefix")
    endif()
    message(STATUS "MinGW 依赖目录: ${MINGW_DEPS_DIR}")

    find_package(PkgConfig REQUIRED)
    set(ENV{PKG_CONFIG_PATH} "${MINGW_DEPS_DIR}/lib/pkgconfig")
    set(ENV{PKG_CONFIG_LIBDIR} "${MINGW_DEPS_DIR}/lib/pkgconfig")
    pkg_check_modules(SDL2 REQUIRED sdl2)
    pkg_check_modules(SDL2_IMAGE REQUIRED SDL2_image)
    pkg_check_modules(SDL2_TTF REQUIRED SDL2_ttf)
    pkg_check_modules(SQLITE3 REQUIRED sqlite3)

    # nlohmann_json: header-only，直接指定路径
    set(NLOHMANN_JSON_INCLUDE_DIR "${MINGW_DEPS_DIR}/include")
    # curl: 预编译库，手动指定
    set(CURL_INCLUDE_DIR "${MINGW_DEPS_DIR}/include")
    set(CURL_LIBRARY_DIR "${MINGW_DEPS_DIR}/lib")
    set(CURL_LIBRARIES curl)
else()
    # macOS / Linux: 使用 pkg-config
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED sdl2)
    pkg_check_modules(SDL2_IMAGE REQUIRED SDL2_image)
    pkg_check_modules(SDL2_TTF REQUIRED SDL2_ttf)
    find_package(nlohmann_json REQUIRED)
    find_package(CURL REQUIRED)
    pkg_check_modules(SQLITE3 REQUIRED sqlite3)
endif()

# 源文件
set(SOURCES
    src/main.cpp
    src/ConfigLoader.cpp
    src/ResourceManager.cpp
    src/AvatarRenderer.cpp
    src/Game.cpp
)

# 可执行文件
add_executable(${PROJECT_NAME} ${SOURCES})

# 根据平台设置包含目录和链接
if(FALSE)
    # vcpkg 模式预留（暂未使用）
else()
    # pkg-config 模式 (macOS/Linux/MinGW)
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${SDL2_INCLUDE_DIRS}
        ${SDL2_IMAGE_INCLUDE_DIRS}
        ${SDL2_TTF_INCLUDE_DIRS}
        ${SQLITE3_INCLUDE_DIRS}
    )

    target_link_directories(${PROJECT_NAME} PRIVATE
        ${SDL2_LIBRARY_DIRS}
        ${SDL2_IMAGE_LIBRARY_DIRS}
        ${SDL2_TTF_LIBRARY_DIRS}
        ${SQLITE3_LIBRARY_DIRS}
    )

    if(MINGW)
        # MinGW: 手动指定 nlohmann_json 和 curl
        target_include_directories(${PROJECT_NAME} PRIVATE
            ${NLOHMANN_JSON_INCLUDE_DIR}
            ${CURL_INCLUDE_DIR}
        )
        target_link_directories(${PROJECT_NAME} PRIVATE
            ${CURL_LIBRARY_DIR}
        )
        target_link_libraries(${PROJECT_NAME} PRIVATE
            ${SDL2_LIBRARIES}
            ${SDL2_IMAGE_LIBRARIES}
            ${SDL2_TTF_LIBRARIES}
            ${CURL_LIBRARIES}
            ${SQLITE3_LIBRARIES}
            ws2_32 crypt32 wldap32
        )
    else()
        # macOS/Linux: 使用 find_package
        target_link_libraries(${PROJECT_NAME} PRIVATE
            ${SDL2_LIBRARIES}
            ${SDL2_IMAGE_LIBRARIES}
            ${SDL2_TTF_LIBRARIES}
            nlohmann_json::nlohmann_json
            CURL::libcurl
            ${SQLITE3_LIBRARIES}
        )
    endif()

    target_compile_options(${PROJECT_NAME} PRIVATE
        ${SDL2_CFLAGS_OTHER}
    )
endif()
