// android_platform.h - Android 平台适配层头文件
// 提供 Android 平台下的文件I/O、HTTP 下载等替代实现
#pragma once

#ifdef ANDROID_PLATFORM

#include <string>
#include <vector>

namespace AndroidPlatform {

// 初始化 Android 平台层（设置内部存储路径等）
void Init();

// 获取应用的内部存储路径（用于缓存文件）
std::string GetInternalStoragePath();

// 获取应用的 assets 基础路径前缀
// Android 上 assets 需要通过 SDL_RWops 读取
std::string GetAssetsPath();

// 从 Android assets 中读取文件内容
std::vector<uint8_t> ReadAsset(const std::string& filename);

// 使用 Java 的 HttpURLConnection 执行 HTTP GET（通过 JNI）
// 替代桌面平台的 system("curl ...")
std::vector<uint8_t> HttpGet(const std::string& url);

// 检查文件是否存在于内部存储
bool FileExists(const std::string& path);

// 写入文件到内部存储
bool WriteFile(const std::string& path, const std::vector<uint8_t>& data);

// 读取内部存储文件
std::vector<uint8_t> ReadFile(const std::string& path);

// 创建目录（递归）
bool CreateDirectories(const std::string& path);

// 保存图片到系统相册（通过 JNI 调用 MediaStore）
bool SaveToGallery(const std::string& pngPath);

// 分享图片到社交平台（通过 JNI 调用 Intent.ACTION_SEND）
void ShareImage(const std::string& pngPath);

} // namespace AndroidPlatform

#endif // ANDROID_PLATFORM
