// main.cpp - 奥拉暖暖入口
#include "Game.h"
#include "ConfigLoader.h"
#include <SDL.h>
#include <iostream>

#ifdef ANDROID_PLATFORM
#include "android_platform.h"
#include <android/log.h>
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "AolaNuanNuan", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "AolaNuanNuan", __VA_ARGS__)
#else
#include <filesystem>
#endif

int main(int argc, char* argv[]) {
#ifndef ANDROID_PLATFORM
    // Portable 模式：将工作目录切换到可执行文件所在位置
    // 在 macOS .app bundle 中，SDL_GetBasePath() 返回 xxx.app/Contents/MacOS/
    // 我们需要切换到 .app 所在的目录，这样 outfits.db、cache/ 等都在 .app 旁边
    {
        namespace fs = std::filesystem;
        char* sdlBase = SDL_GetBasePath();
        if (sdlBase) {
            fs::path basePath(sdlBase);
            SDL_free(sdlBase);
            
            // 检测是否在 .app bundle 中（路径包含 .app/Contents/MacOS）
            std::string pathStr = basePath.string();
            auto appPos = pathStr.find(".app/Contents/MacOS");
            if (appPos != std::string::npos) {
                // 跳到 .app 所在目录
                fs::path appPath = pathStr.substr(0, appPos);
                fs::path appDir = fs::path(appPath).parent_path();
                if (fs::exists(appDir)) {
                    fs::current_path(appDir);
                    std::cout << "工作目录(app): " << appDir.string() << std::endl;
                }
            } else {
                // 非 .app 模式，直接用可执行文件所在目录
                if (fs::exists(basePath)) {
                    fs::current_path(basePath);
                    std::cout << "工作目录: " << basePath.string() << std::endl;
                }
            }
        }
    }
#endif

    std::cout << "========================================" << std::endl;
    std::cout << "  奥拉暖暖 (SDL2 C++ 版)" << std::endl;
    std::cout << "========================================" << std::endl;

#ifdef ANDROID_PLATFORM
    // Android 平台初始化（必须在任何文件操作之前调用）
    AndroidPlatform::Init();
#endif

    // 从网络加载配置
    std::string changeJsonUrl = "https://aola.100bt.com/h5/data/change.json";
    std::cout << "从网络加载配置: " << changeJsonUrl << std::endl;

    if (!LoadConfigFromNetwork(changeJsonUrl)) {
        std::cerr << "网络配置加载失败，尝试从本地加载..." << std::endl;

        // 网络失败时回退到本地文件加载
#ifdef ANDROID_PLATFORM
        std::string clothesDataPath = "clothesdata.json";
        std::string clothesPartDataPath = "clothespartdata.json";
#else
        namespace fs = std::filesystem;
        std::string basePath = fs::current_path().string();
        std::string clothesDataPath = basePath + "/clothesdata.json";
        std::string clothesPartDataPath = basePath + "/clothespartdata.json";
        if (!fs::exists(clothesDataPath)) {
            clothesDataPath = basePath + "/../clothesdata.json";
            clothesPartDataPath = basePath + "/../clothespartdata.json";
        }
#endif
        if (!LoadConfig(clothesDataPath, clothesPartDataPath)) {
            std::cerr << "配置加载失败，请检查网络或确保本地 JSON 文件可用" << std::endl;
            return 1;
        }
    }

    // 创建并运行游戏
    Game game;
#ifdef ANDROID_PLATFORM
    // Android 上使用全屏，传入 0x0 让 SDL 自动获取屏幕尺寸
    if (!game.Init("奥拉暖暖", 0, 0)) {
        LOGE("游戏初始化失败");
        return 1;
    }
#else
    if (!game.Init("奥拉暖暖", 1280, 800)) {
        std::cerr << "游戏初始化失败" << std::endl;
        return 1;
    }
#endif

    std::cout << std::endl;
    std::cout << "操作说明:" << std::endl;
    std::cout << "  - 左侧分类标签切换服装类型" << std::endl;
    std::cout << "  - 点击列表中的服装名称穿上/脱下" << std::endl;
    std::cout << "  - 鼠标滚轮滚动列表" << std::endl;
    std::cout << "  - 右上角切换人模(旧/新男/新女)" << std::endl;
    std::cout << "  - 保存按钮：保存当前装扮并生成编号" << std::endl;
    std::cout << "  - 加载按钮：输入编号加载已保存的装扮" << std::endl;
    std::cout << "  - R键重置 | F键搜索 | ESC键退出" << std::endl;
    std::cout << std::endl;

    game.Run();
    game.Cleanup();

    return 0;
}
