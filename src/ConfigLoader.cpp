// ConfigLoader.cpp - 配置加载器实现
#include "ConfigLoader.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <set>

#ifndef ANDROID_PLATFORM
#include <curl/curl.h>
#endif

#ifdef ANDROID_PLATFORM
#include "android_platform.h"
#include <SDL.h>
#include <android/log.h>
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "ConfigLoader", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "ConfigLoader", __VA_ARGS__)
#endif

using json = nlohmann::json;

// 全局服装数据
std::unordered_map<int, Cloth> g_clothData;
// 服装影响身体显示的配置 (clothID -> hideFlags)
std::unordered_map<int, int> g_clothAffectBody;
// 套装数据
std::unordered_map<int, ClothesSuit> g_suitData;

// ---- 网络下载辅助 ----
#ifdef ANDROID_PLATFORM
// Android: 通过 JNI 调用 Java HttpURLConnection
static std::string HttpGetString(const std::string& url) {
    auto data = AndroidPlatform::HttpGet(url);
    if (data.empty()) {
        LOGE("[网络] 下载失败: %s", url.c_str());
        return "";
    }
    LOGI("[网络] 下载成功: %s (%zu 字节)", url.c_str(), data.size());
    return std::string(data.begin(), data.end());
}
#else
// 桌面平台: 使用 libcurl
static size_t CurlWriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t totalSize = size * nmemb;
    static_cast<std::string*>(userp)->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

static std::string HttpGetString(const std::string& url) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "[网络] curl 初始化失败" << std::endl;
        return "";
    }
    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        std::cerr << "[网络] 下载失败: " << url << " -> " << curl_easy_strerror(res) << std::endl;
        curl_easy_cleanup(curl);
        return "";
    }
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_cleanup(curl);
    if (httpCode != 200) {
        std::cerr << "[网络] HTTP " << httpCode << ": " << url << std::endl;
        return "";
    }
    std::cout << "[网络] 下载成功: " << url << " (" << response.size() << " 字节)" << std::endl;
    return response;
}
#endif

// ---- 公共 JSON 解析逻辑 ----
static bool ParseClothConfig(const json& clothJson, const json& partJson) {
    const auto& clothData = clothJson["data"];
    const auto& partData = partJson["data"];

    for (auto& [key, val] : clothData.items()) {
        try {
            if (!val.is_array() || val.size() < 12) continue;

            Cloth cloth;
            cloth.clothID = val[0].get<int>();
            cloth.clothName = val[1].get<std::string>();
            cloth.clothType = val[8].get<int>();
            cloth.classify = val[10].get<int>();
            cloth.clothVal = val[11].get<int>();

            // 查找 partdata
            if (partData.contains(key)) {
                const auto& pd = partData[key];
                cloth.fileName = pd[1].get<std::string>();

                for (const auto& part : pd[0]) {
                    ClothPart cp;
                    std::string rawFileName = part[0].get<std::string>();
                    // 检测 forHide: 文件名以 '_' 结尾的部件是隐藏标记部件
                    if (!rawFileName.empty() && rawFileName.back() == '_') {
                        cp.forHide = true;
                        cp.clothPartFileName = rawFileName.substr(0, rawFileName.size() - 1);
                    } else {
                        cp.forHide = false;
                        cp.clothPartFileName = rawFileName;
                    }
                    cp.clothesPos = part[1].get<int>();
                    cloth.clothParts.push_back(cp);
                }
                g_clothData[cloth.clothID] = cloth;
            }
        } catch (const std::exception& e) {
            continue;
        }
    }

    std::cout << "已加载 " << g_clothData.size() << " 件服装配置" << std::endl;

    // 加载 CLOTHES_AFFECT_BODY 配置
    if (clothJson.contains("CLOTHES_AFFECT_BODY")) {
        const auto& cabData = clothJson["CLOTHES_AFFECT_BODY"];
        for (auto& [key, val] : cabData.items()) {
            try {
                if (!val.is_array() || val.size() < 2) continue;
                int clothID = val[0].get<int>();
                int hideFlags = val[1].get<int>();
                g_clothAffectBody[clothID] = hideFlags;
            } catch (...) { continue; }
        }
        std::cout << "已加载 " << g_clothAffectBody.size() << " 条身体影响配置" << std::endl;
    }

    // 加载套装数据
    if (clothJson.contains("suit")) {
        const auto& suitJson = clothJson["suit"];
        for (auto& [key, val] : suitJson.items()) {
            try {
                if (!val.is_array() || val.size() < 3) continue;
                ClothesSuit suit;
                suit.suitId = val[0].get<int>();
                suit.suitName = val[1].get<std::string>();
                if (val[2].is_array()) {
                    for (const auto& cid : val[2]) {
                        suit.clothesList.push_back(cid.get<int>());
                    }
                }
                g_suitData[suit.suitId] = suit;
            } catch (...) { continue; }
        }
        std::cout << "已加载 " << g_suitData.size() << " 个套装配置" << std::endl;
    }

    return true;
}

#ifdef ANDROID_PLATFORM
// Android 平台：通过 SDL_RWFromFile 从 assets 读取 JSON 文件
static std::string ReadFileAsString(const std::string& path) {
    SDL_RWops* rw = SDL_RWFromFile(path.c_str(), "rb");
    if (!rw) {
        LOGE("无法打开文件: %s (%s)", path.c_str(), SDL_GetError());
        return "";
    }
    Sint64 size = SDL_RWsize(rw);
    if (size <= 0) {
        SDL_RWclose(rw);
        return "";
    }
    std::string content(size, '\0');
    SDL_RWread(rw, &content[0], 1, size);
    SDL_RWclose(rw);
    LOGI("读取文件成功: %s (%ld 字节)", path.c_str(), (long)size);
    return content;
}
#endif

bool LoadConfig(const std::string& clothesDataPath, const std::string& clothesPartDataPath) {
    try {
#ifdef ANDROID_PLATFORM
        // Android: 通过 SDL_RWFromFile 读取 assets 中的 JSON
        std::string clothContent = ReadFileAsString(clothesDataPath);
        if (clothContent.empty()) {
            LOGE("无法读取 %s", clothesDataPath.c_str());
            return false;
        }
        json clothJson = json::parse(clothContent);

        std::string partContent = ReadFileAsString(clothesPartDataPath);
        if (partContent.empty()) {
            LOGE("无法读取 %s", clothesPartDataPath.c_str());
            return false;
        }
        json partJson = json::parse(partContent);
#else
        // 桌面平台: 通过 std::ifstream 读取
        // 读取 clothesdata.json
        std::ifstream clothFile(clothesDataPath);
        if (!clothFile.is_open()) {
            std::cerr << "无法打开 " << clothesDataPath << std::endl;
            return false;
        }
        json clothJson = json::parse(clothFile);
        clothFile.close();

        // 读取 clothespartdata.json
        std::ifstream partFile(clothesPartDataPath);
        if (!partFile.is_open()) {
            std::cerr << "无法打开 " << clothesPartDataPath << std::endl;
            return false;
        }
        json partJson = json::parse(partFile);
        partFile.close();
#endif

        return ParseClothConfig(clothJson, partJson);

    } catch (const std::exception& e) {
        std::cerr << "加载配置失败: " << e.what() << std::endl;
        return false;
    }
}

bool LoadConfigFromNetwork(const std::string& changeJsonUrl) {
    try {
#ifndef ANDROID_PLATFORM
        curl_global_init(CURL_GLOBAL_DEFAULT);
#endif

        // 1. 下载 change.json（包含 clothesdata.json）
        std::cout << "[网络] 正在下载配置: " << changeJsonUrl << std::endl;
        std::string changeContent = HttpGetString(changeJsonUrl);
        if (changeContent.empty()) {
            std::cerr << "[网络] 下载 change.json 失败" << std::endl;
#ifndef ANDROID_PLATFORM
            curl_global_cleanup();
#endif
            return false;
        }

        json changeJson = json::parse(changeContent);

        // 从 change.json 中提取 clothesdata.json
        if (!changeJson.contains("clothesdata.json")) {
            std::cerr << "[网络] change.json 中未找到 clothesdata.json" << std::endl;
#ifndef ANDROID_PLATFORM
            curl_global_cleanup();
#endif
            return false;
        }
        json clothJson = changeJson["clothesdata.json"];
        // 释放大 JSON 内存
        changeContent.clear();
        changeContent.shrink_to_fit();

        // 2. 单独下载 clothespartdata.json（change.json 中没有此数据）
        // 从 changeJsonUrl 推导基础路径
        std::string baseDataUrl = changeJsonUrl.substr(0, changeJsonUrl.rfind('/') + 1);
        std::string partUrl = baseDataUrl + "clothespartdata.json";

        std::cout << "[网络] 正在下载部件数据: " << partUrl << std::endl;
        std::string partContent = HttpGetString(partUrl);
        if (partContent.empty()) {
            std::cerr << "[网络] 下载 clothespartdata.json 失败" << std::endl;
#ifndef ANDROID_PLATFORM
            curl_global_cleanup();
#endif
            return false;
        }

        json partJson = json::parse(partContent);
        partContent.clear();
        partContent.shrink_to_fit();

#ifndef ANDROID_PLATFORM
        curl_global_cleanup();
#endif

        // 3. 解析配置
        return ParseClothConfig(clothJson, partJson);

    } catch (const std::exception& e) {
        std::cerr << "[网络] 加载配置失败: " << e.what() << std::endl;
#ifndef ANDROID_PLATFORM
        curl_global_cleanup();
#endif
        return false;
    }
}

std::string GetModelSuffix(int classify, int model) {
    // 需要根据 model 切换后缀的分类（三模通用 / 旧+男 / 旧+女 / 新人模专属）
    static const std::set<int> needModelSuffix = {
        CLASSIFY_ALL, CLASSIFY_OLD_AND_BOY, CLASSIFY_OLD_AND_GIRL, CLASSIFY_NEW_ALL_SPECIAL
    };
    // 强制使用 _0 的分类（背景类/新人模通用）
    static const std::set<int> forceModel0 = {
        CLASSIFY_OLD_BG, CLASSIFY_NEW_BG, CLASSIFY_NEW_ALL
    };

    // 新男模专属 classify=1 → 固定后缀 _1
    if (classify == CLASSIFY_NEW_BOY) return "1";
    // 新女模专属 classify=2 → 固定后缀 _2
    if (classify == CLASSIFY_NEW_GIRL) return "2";

    if (forceModel0.count(classify)) return "0";
    if (needModelSuffix.count(classify)) return std::to_string(model);
    return "0";
}

std::string GetPngURL(const std::string& baseUrl, const Cloth& cloth,
                      const ClothPart& part, int model) {
    std::string suffix = GetModelSuffix(cloth.classify, model);
    std::string dirName = cloth.fileName + "_" + suffix;
    return baseUrl + "clothes/clothespart/" + dirName + "/" + part.clothPartFileName + "_1.png";
}

std::string GetJsonURL(const std::string& baseUrl, const Cloth& cloth,
                       const ClothPart& part, int model) {
    std::string suffix = GetModelSuffix(cloth.classify, model);
    std::string dirName = cloth.fileName + "_" + suffix;
    return baseUrl + "clothes/clothespart/" + dirName + "/" + part.clothPartFileName + "_config.json";
}
