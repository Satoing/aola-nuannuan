// ResourceManager.cpp - 资源管理器实现
#include "ResourceManager.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <sstream>
#include <cstdio>
#include <fstream>
#include <mutex>

#ifdef ANDROID_PLATFORM
#include "android_platform.h"
#include <android/log.h>
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "ResMgr", __VA_ARGS__)
#else
#include <curl/curl.h>
#include <filesystem>
namespace fs = std::filesystem;
#endif

#ifndef ANDROID_PLATFORM
// libcurl 写回调
static size_t ResWriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    auto* buf = static_cast<std::vector<uint8_t>*>(userp);
    size_t totalSize = size * nmemb;
    buf->insert(buf->end(), (uint8_t*)contents, (uint8_t*)contents + totalSize);
    return totalSize;
}

// 使用 libcurl 下载到文件
static bool CurlDownloadToFile(const std::string& url, const std::string& filePath) {
    CURL* curl = curl_easy_init();
    if (!curl) return false;

    FILE* fp = fopen(filePath.c_str(), "wb");
    if (!fp) {
        curl_easy_cleanup(curl);
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);

    CURLcode res = curl_easy_perform(curl);
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_cleanup(curl);
    fclose(fp);

    if (res != CURLE_OK || httpCode != 200) {
        std::remove(filePath.c_str());
        return false;
    }
    return true;
}
#endif  // !ANDROID_PLATFORM

// macOS / Unix: 使用 curl 命令行下载（简单可靠，避免引入 libcurl 依赖）
// 如果需要更高性能，可以替换为 libcurl API

using json = nlohmann::json;

// 静态成员定义
std::unordered_map<std::string, std::vector<uint8_t>> ResourceManager::s_dataCache;
std::mutex ResourceManager::s_dataCacheMutex;

// 缓存目录
#ifdef ANDROID_PLATFORM
static std::string getCacheDir() {
    return AndroidPlatform::GetInternalStoragePath() + "cache/";
}
#define CACHE_DIR getCacheDir()
#else
static const std::string CACHE_DIR = "cache/";
#endif

ResourceManager::ResourceManager(SDL_Renderer* renderer) : m_renderer(renderer) {
    // 创建缓存目录
#ifdef ANDROID_PLATFORM
    AndroidPlatform::CreateDirectories(getCacheDir());
#else
    fs::create_directories(CACHE_DIR);
#endif
}

ResourceManager::~ResourceManager() {
    ClearCache();
}

void ResourceManager::ClearCache() {
    for (auto& [url, tex] : m_textureCache) {
        if (tex) SDL_DestroyTexture(tex);
    }
    m_textureCache.clear();
    m_positionCache.clear();
}

void ResourceManager::ClearDataCache() {
    std::lock_guard<std::mutex> lock(s_dataCacheMutex);
    s_dataCache.clear();
}

std::vector<uint8_t> ResourceManager::HttpGetCached(const std::string& url) {
    // 先检查内存缓存
    {
        std::lock_guard<std::mutex> lock(s_dataCacheMutex);
        auto it = s_dataCache.find(url);
        if (it != s_dataCache.end()) {
            return it->second;  // 内存缓存命中，直接返回
        }
    }

    // 未命中内存缓存，调用 HttpGet（会检查文件缓存或下载）
    auto data = HttpGet(url);

    // 存入内存缓存
    if (!data.empty()) {
        std::lock_guard<std::mutex> lock(s_dataCacheMutex);
        s_dataCache[url] = data;
    }

    return data;
}

std::vector<uint8_t> ResourceManager::HttpGet(const std::string& url) {
    // 将 URL 转换为缓存文件名（简单 hash）
    std::string safeName;
    for (char c : url) {
        if (isalnum(c) || c == '.' || c == '_' || c == '-') {
            safeName += c;
        } else {
            safeName += '_';
        }
    }

#ifdef ANDROID_PLATFORM
    std::string cacheDir = getCacheDir();
#else
    std::string cacheDir = CACHE_DIR;
#endif
    std::string cachePath = cacheDir + safeName;

    // 如果缓存文件已存在，直接读取
#ifdef ANDROID_PLATFORM
    if (AndroidPlatform::FileExists(cachePath)) {
        return AndroidPlatform::ReadFile(cachePath);
    }

    // 使用 Android JNI HTTP 下载
    auto data = AndroidPlatform::HttpGet(url);
    if (data.empty()) {
        return {};
    }

    // 写入缓存
    AndroidPlatform::WriteFile(cachePath, data);
    return data;
#else
    if (fs::exists(cachePath)) {
        std::ifstream ifs(cachePath, std::ios::binary);
        return std::vector<uint8_t>(
            std::istreambuf_iterator<char>(ifs),
            std::istreambuf_iterator<char>()
        );
    }

    // 使用 libcurl 下载
    if (!CurlDownloadToFile(url, cachePath)) {
        std::cerr << "下载失败: " << url << std::endl;
        return {};
    }

    // 读取下载的文件
    std::ifstream ifs(cachePath, std::ios::binary);
    if (!ifs.is_open()) return {};
    return std::vector<uint8_t>(
        std::istreambuf_iterator<char>(ifs),
        std::istreambuf_iterator<char>()
    );
#endif
}

SDL_Texture* ResourceManager::LoadTextureFromURL(const std::string& url) {
    // 检查缓存
    auto it = m_textureCache.find(url);
    if (it != m_textureCache.end()) return it->second;

    // 下载
    auto data = HttpGet(url);
    if (data.empty()) {
        m_textureCache[url] = nullptr;
        return nullptr;
    }

    // 从内存创建纹理
    SDL_RWops* rw = SDL_RWFromMem(data.data(), (int)data.size());
    if (!rw) {
        std::cerr << "SDL_RWFromMem 失败: " << SDL_GetError() << std::endl;
        m_textureCache[url] = nullptr;
        return nullptr;
    }

    SDL_Surface* surface = IMG_Load_RW(rw, 1); // 1 = 自动关闭 RW
    if (!surface) {
        std::cerr << "IMG_Load_RW 失败: " << IMG_GetError() << " URL: " << url << std::endl;
        m_textureCache[url] = nullptr;
        return nullptr;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(m_renderer, surface);
    SDL_FreeSurface(surface);

    if (!texture) {
        std::cerr << "SDL_CreateTextureFromSurface 失败: " << SDL_GetError() << std::endl;
    }

    // 设置纹理混合模式为 Alpha 混合
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);

    m_textureCache[url] = texture;
    return texture;
}

PartPosition ResourceManager::LoadPositionFromURL(const std::string& url) {
    // 检查缓存
    auto it = m_positionCache.find(url);
    if (it != m_positionCache.end()) return it->second;

    auto data = HttpGet(url);
    if (data.empty()) {
        PartPosition pos = {0, 0};
        m_positionCache[url] = pos;
        return pos;
    }

    try {
        std::string jsonStr(data.begin(), data.end());
        auto j = json::parse(jsonStr);
        auto& rect = j["rect"];
        // 前端逻辑: pivotX = -rect[0], pivotY = -rect[1]
        // Laya pivot 效果: 显示位置 = (rect[0], rect[1])
        PartPosition pos = {rect[0].get<int>(), rect[1].get<int>()};
        m_positionCache[url] = pos;
        return pos;
    } catch (const std::exception& e) {
        std::cerr << "解析 JSON 失败: " << e.what() << " URL: " << url << std::endl;
        PartPosition pos = {0, 0};
        m_positionCache[url] = pos;
        return pos;
    }
}

void ResourceManager::GetTextureSize(SDL_Texture* tex, int* w, int* h) {
    if (tex) {
        SDL_QueryTexture(tex, nullptr, nullptr, w, h);
    } else {
        *w = 0;
        *h = 0;
    }
}
