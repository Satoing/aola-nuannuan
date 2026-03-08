// android_platform.cpp - Android 平台适配层实现
// 使用 SDL2 的 Android 后端和 JNI 调用实现文件IO和HTTP

#ifdef ANDROID_PLATFORM

#include "android_platform.h"
#include <SDL.h>
#include <android/log.h>
#include <jni.h>
#include <sys/stat.h>
#include <fstream>
#include <sstream>

#define LOG_TAG "AolaNuanNuan"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace AndroidPlatform {

static std::string s_internalPath;

void Init() {
    // 获取 Android 内部存储路径
    // SDL_AndroidGetInternalStoragePath() 返回 /data/data/<package>/files/
    const char* path = SDL_AndroidGetInternalStoragePath();
    if (path) {
        s_internalPath = path;
        if (!s_internalPath.empty() && s_internalPath.back() != '/') {
            s_internalPath += '/';
        }
        LOGI("内部存储路径: %s", s_internalPath.c_str());
    } else {
        LOGE("无法获取内部存储路径！");
        s_internalPath = "/data/local/tmp/aola/";
    }
}

std::string GetInternalStoragePath() {
    return s_internalPath;
}

std::string GetAssetsPath() {
    return "";  // Android 上 assets 通过 SDL_RWFromFile 使用相对路径访问
}

std::vector<uint8_t> ReadAsset(const std::string& filename) {
    // 使用 SDL_RWops 从 assets 中读取
    SDL_RWops* rw = SDL_RWFromFile(filename.c_str(), "rb");
    if (!rw) {
        LOGE("无法打开 asset: %s (%s)", filename.c_str(), SDL_GetError());
        return {};
    }

    Sint64 size = SDL_RWsize(rw);
    if (size <= 0) {
        SDL_RWclose(rw);
        LOGE("asset 文件大小无效: %s", filename.c_str());
        return {};
    }

    std::vector<uint8_t> data(size);
    size_t read = SDL_RWread(rw, data.data(), 1, size);
    SDL_RWclose(rw);

    if (read != (size_t)size) {
        LOGE("读取 asset 不完整: %s (期望 %ld 字节，实际 %zu)", filename.c_str(), (long)size, read);
        data.resize(read);
    }

    LOGI("读取 asset 成功: %s (%zu 字节)", filename.c_str(), data.size());
    return data;
}

std::vector<uint8_t> HttpGet(const std::string& url) {
    // 通过 JNI 调用 Java 的 HttpURLConnection 进行网络请求
    JNIEnv* env = (JNIEnv*)SDL_AndroidGetJNIEnv();
    if (!env) {
        LOGE("无法获取 JNI 环境");
        return {};
    }

    // 获取工具类
    jclass clazz = env->FindClass("com/aola/nuannuan/HttpHelper");
    if (!clazz) {
        // 如果 HttpHelper 类不存在，清除异常并使用备用方案
        env->ExceptionClear();
        LOGE("HttpHelper 类未找到，尝试使用 java.net.URL");

        // 使用 java.net.URL 直接下载
        jclass urlClass = env->FindClass("java/net/URL");
        if (!urlClass) {
            env->ExceptionClear();
            LOGE("无法找到 java.net.URL 类");
            return {};
        }

        jstring jUrl = env->NewStringUTF(url.c_str());
        jmethodID urlCtor = env->GetMethodID(urlClass, "<init>", "(Ljava/lang/String;)V");
        jobject urlObj = env->NewObject(urlClass, urlCtor, jUrl);

        // 打开连接并读取
        jmethodID openStream = env->GetMethodID(urlClass, "openStream", "()Ljava/io/InputStream;");
        jobject inputStream = env->CallObjectMethod(urlObj, openStream);

        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            LOGE("HTTP 请求失败: %s", url.c_str());
            env->DeleteLocalRef(jUrl);
            return {};
        }

        if (!inputStream) {
            LOGE("HTTP 流为空: %s", url.c_str());
            env->DeleteLocalRef(jUrl);
            return {};
        }

        // 读取所有字节
        jclass isClass = env->GetObjectClass(inputStream);
        jmethodID readMethod = env->GetMethodID(isClass, "read", "([B)I");
        jmethodID closeMethod = env->GetMethodID(isClass, "close", "()V");

        std::vector<uint8_t> result;
        jbyteArray buffer = env->NewByteArray(8192);

        while (true) {
            jint bytesRead = env->CallIntMethod(inputStream, readMethod, buffer);
            if (env->ExceptionCheck()) {
                env->ExceptionClear();
                break;
            }
            if (bytesRead <= 0) break;

            jbyte* bytes = env->GetByteArrayElements(buffer, nullptr);
            result.insert(result.end(), bytes, bytes + bytesRead);
            env->ReleaseByteArrayElements(buffer, bytes, JNI_ABORT);
        }

        env->CallVoidMethod(inputStream, closeMethod);
        env->DeleteLocalRef(buffer);
        env->DeleteLocalRef(inputStream);
        env->DeleteLocalRef(jUrl);

        LOGI("HTTP GET 成功: %s (%zu 字节)", url.c_str(), result.size());
        return result;
    }

    // 使用 HttpHelper.get(url) 静态方法
    jmethodID getMethod = env->GetStaticMethodID(clazz, "get", "(Ljava/lang/String;)[B");
    if (!getMethod) {
        env->ExceptionClear();
        LOGE("HttpHelper.get 方法未找到");
        return {};
    }

    jstring jUrl = env->NewStringUTF(url.c_str());
    jbyteArray result = (jbyteArray)env->CallStaticObjectMethod(clazz, getMethod, jUrl);

    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        env->DeleteLocalRef(jUrl);
        LOGE("HTTP GET 异常: %s", url.c_str());
        return {};
    }

    if (!result) {
        env->DeleteLocalRef(jUrl);
        return {};
    }

    jsize len = env->GetArrayLength(result);
    std::vector<uint8_t> data(len);
    env->GetByteArrayRegion(result, 0, len, (jbyte*)data.data());

    env->DeleteLocalRef(result);
    env->DeleteLocalRef(jUrl);

    LOGI("HTTP GET 成功: %s (%zu 字节)", url.c_str(), data.size());
    return data;
}

bool FileExists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

bool WriteFile(const std::string& path, const std::vector<uint8_t>& data) {
    std::ofstream ofs(path, std::ios::binary);
    if (!ofs.is_open()) {
        LOGE("无法写入文件: %s", path.c_str());
        return false;
    }
    ofs.write(reinterpret_cast<const char*>(data.data()), data.size());
    return ofs.good();
}

std::vector<uint8_t> ReadFile(const std::string& path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs.is_open()) return {};
    return std::vector<uint8_t>(
        std::istreambuf_iterator<char>(ifs),
        std::istreambuf_iterator<char>()
    );
}

bool CreateDirectories(const std::string& path) {
    // 简单的递归创建目录
    std::string current;
    for (char c : path) {
        current += c;
        if (c == '/') {
            mkdir(current.c_str(), 0755);
        }
    }
    mkdir(path.c_str(), 0755);
    return true;
}

bool SaveToGallery(const std::string& pngPath) {
    JNIEnv* env = (JNIEnv*)SDL_AndroidGetJNIEnv();
    if (!env) {
        LOGE("SaveToGallery: 无法获取 JNI 环境");
        return false;
    }

    jclass clazz = env->FindClass("com/aola/nuannuan/AolaNuanNuanActivity");
    if (!clazz) {
        env->ExceptionClear();
        LOGE("SaveToGallery: 找不到 AolaNuanNuanActivity 类");
        return false;
    }

    jmethodID mid = env->GetStaticMethodID(clazz, "saveToGallery", "(Ljava/lang/String;)Z");
    if (!mid) {
        env->ExceptionClear();
        LOGE("SaveToGallery: 找不到 saveToGallery 方法");
        env->DeleteLocalRef(clazz);
        return false;
    }

    jstring jPath = env->NewStringUTF(pngPath.c_str());
    jboolean result = env->CallStaticBooleanMethod(clazz, mid, jPath);
    env->DeleteLocalRef(jPath);
    env->DeleteLocalRef(clazz);

    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        LOGE("SaveToGallery: Java 异常");
        return false;
    }

    LOGI("SaveToGallery: %s -> %s", pngPath.c_str(), result ? "成功" : "失败");
    return result;
}

void ShareImage(const std::string& pngPath) {
    JNIEnv* env = (JNIEnv*)SDL_AndroidGetJNIEnv();
    if (!env) {
        LOGE("ShareImage: 无法获取 JNI 环境");
        return;
    }

    jclass clazz = env->FindClass("com/aola/nuannuan/AolaNuanNuanActivity");
    if (!clazz) {
        env->ExceptionClear();
        LOGE("ShareImage: 找不到 AolaNuanNuanActivity 类");
        return;
    }

    jmethodID mid = env->GetStaticMethodID(clazz, "shareImage", "(Ljava/lang/String;)V");
    if (!mid) {
        env->ExceptionClear();
        LOGE("ShareImage: 找不到 shareImage 方法");
        env->DeleteLocalRef(clazz);
        return;
    }

    jstring jPath = env->NewStringUTF(pngPath.c_str());
    env->CallStaticVoidMethod(clazz, mid, jPath);
    env->DeleteLocalRef(jPath);
    env->DeleteLocalRef(clazz);

    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        LOGE("ShareImage: Java 异常");
    }
}

} // namespace AndroidPlatform

#endif // ANDROID_PLATFORM
