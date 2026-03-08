// AvatarRenderer.h - 角色渲染器
#pragma once
#include "ClothDefine.h"
#include "ResourceManager.h"
#include <SDL.h>
#include <vector>
#include <string>
#include <mutex>
#include <queue>
#include <atomic>
#include <unordered_map>

// 裁剪模式（用于旧人模 pos=53 躯干+腿合并部件的遮罩渲染）
enum ClipMode {
    CLIP_NONE = 0,       // 不裁剪，正常显示
    CLIP_SHOW_UPPER = 1, // 只显示上半身（隐藏腿部）
    CLIP_SHOW_LOWER = 2, // 只显示下半身（隐藏上半身）
};

// 渲染部件信息（已排序、已加载）
struct RenderPart {
    int clothesPos;
    int classify = 0;  // 服装分类（用于判断背景/前景）
    SDL_Texture* texture;
    int x, y;        // 偏移位置
    int w, h;        // 纹理尺寸
    ClipMode clipMode = CLIP_NONE;  // 裁剪模式
};

// 后台线程下载完成的部件原始数据（等待主线程创建纹理）
struct PendingPartData {
    int clothesPos;
    int classify = 0;                 // 服装分类（用于判断背景/前景）
    std::vector<uint8_t> pngData;    // PNG 图片原始字节
    int posX, posY;                   // 从 JSON 解析出的坐标
    ClipMode clipMode = CLIP_NONE;   // 裁剪模式（从 LoadParts 传递到 Render）
};

class AvatarRenderer {
public:
    AvatarRenderer(ResourceManager* resMgr, const std::string& baseUrl);
    ~AvatarRenderer();

    // 设置人模类型 (0=旧, 1=新男, 2=新女)
    void SetModel(int model) { m_model = model; }
    int GetModel() const { return m_model; }

    // 设置/获取当前穿着的服装
    void SetClothes(const std::vector<int>& clothIDs);
    const std::vector<int>& GetClothes() const { return m_clothIDs; }

    // 设置默认底色挂载点映射（参考前端 ClothesDefaultConfig）
    // key=clothesPos（挂载点）, value=默认clothID
    void SetDefaultSkinMap(const std::unordered_map<int, int>& skinMap) { m_defaultSkinMap = skinMap; }

    // 设置额外默认部件（头发、眼睛等，参考前端 extraDefaultSkins）
    // extraPositions: 需要检查的挂载点列表（如 TOU_FA__QIAN=19, TOU_FA__HOU=73, YAN_JING__ZUI_BA=24）
    // extraClothIDs: 对应的默认服装ID列表
    void SetExtraDefaults(const std::vector<int>& extraPositions, const std::vector<int>& extraClothIDs) {
        m_extraDefaultPositions = extraPositions;
        m_extraDefaultClothIDs = extraClothIDs;
    }

    // 添加/移除单件服装
    void AddCloth(int clothID);
    void RemoveCloth(int clothID);

    // 检查是否穿着某件服装
    bool IsWearing(int clothID) const;

    // 计算当前穿着的总时尚值
    int GetTotalFashionValue() const;

    // 异步加载所有部件资源（后台线程下载）
    void LoadParts();

    // 主线程每帧调用：处理后台下载完成的数据，创建纹理
    void Update(SDL_Renderer* renderer);

    // 是否正在加载中
    bool IsLoading() const { return m_loading.load(); }

    // 渲染角色到指定位置
    void Render(SDL_Renderer* renderer, int centerX, int bottomY);

private:
    ResourceManager* m_resMgr;
    std::string m_baseUrl;
    int m_model;
    std::vector<int> m_clothIDs;
    std::unordered_map<int, int> m_defaultSkinMap;  // 默认底色挂载点映射: pos -> clothID

    // 额外默认部件（头发、眼睛等，参考前端 extraDefaultSkins / extraDefaultSkinsId）
    std::vector<int> m_extraDefaultPositions;  // 需检查的挂载点列表
    std::vector<int> m_extraDefaultClothIDs;   // 对应的默认服装ID列表

    // 检查并补回缺失的默认服装（参考前端 checkDefaultClothes + addDefaultCloth）
    void CheckAndRestoreDefaults();
    std::vector<RenderPart> m_renderParts;
    bool m_dirty;  // 标记是否需要重新加载

    // 异步加载相关
    std::atomic<bool> m_loading{false};          // 是否正在后台加载
    int m_loadGeneration{0};                      // 加载代数（用于取消过期的加载）
    std::mutex m_pendingMutex;
    std::vector<PendingPartData> m_pendingParts;  // 后台线程准备好的数据
    bool m_pendingReady{false};                   // 数据是否就绪
    int m_pendingGeneration{0};                   // 待处理数据的代数
};
