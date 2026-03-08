// ResourceManager.h - 资源管理器（下载 + 本地缓存）
#pragma once
#include <SDL.h>
#include <SDL_image.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>

// 部件的坐标信息
struct PartPosition {
    int x;
    int y;
};

class ResourceManager {
public:
    ResourceManager(SDL_Renderer* renderer);
    ~ResourceManager();

    // 从 URL 下载 PNG 并创建纹理（带缓存）
    SDL_Texture* LoadTextureFromURL(const std::string& url);

    // 从 URL 下载 JSON 并解析坐标（带缓存）
    PartPosition LoadPositionFromURL(const std::string& url);

    // 清除所有缓存
    void ClearCache();

    // 获取纹理尺寸
    static void GetTextureSize(SDL_Texture* tex, int* w, int* h);

// HTTP GET 请求，返回响应体（公开供外部异步使用）
    static std::vector<uint8_t> HttpGet(const std::string& url);

    // 带内存缓存的 HTTP GET（避免重复文件IO）
    static std::vector<uint8_t> HttpGetCached(const std::string& url);

    // 清除内存级数据缓存
    static void ClearDataCache();

private:
    SDL_Renderer* m_renderer;
    std::unordered_map<std::string, SDL_Texture*> m_textureCache;
    std::unordered_map<std::string, PartPosition> m_positionCache;

    // 内存级数据缓存（避免反复读磁盘文件）
    static std::unordered_map<std::string, std::vector<uint8_t>> s_dataCache;
    static std::mutex s_dataCacheMutex;
};
