// ConfigLoader.h - 配置加载器
#pragma once
#include "ClothDefine.h"
#include <string>
#include <unordered_map>

// 全局服装数据存储
extern std::unordered_map<int, Cloth> g_clothData;

// 服装影响身体显示的配置 (clothID -> BodyHideFlags)
extern std::unordered_map<int, int> g_clothAffectBody;

// 套装数据存储 (suitId -> ClothesSuit)
extern std::unordered_map<int, ClothesSuit> g_suitData;

// 从网络加载配置（change.json URL）
bool LoadConfigFromNetwork(const std::string& changeJsonUrl);

// 从本地文件加载配置（保留兼容）
bool LoadConfig(const std::string& clothesDataPath, const std::string& clothesPartDataPath);

// 获取 model 后缀
std::string GetModelSuffix(int classify, int model);

// 获取 PNG 资源 URL
std::string GetPngURL(const std::string& baseUrl, const Cloth& cloth,
                      const ClothPart& part, int model);

// 获取 config JSON 资源 URL
std::string GetJsonURL(const std::string& baseUrl, const Cloth& cloth,
                       const ClothPart& part, int model);
