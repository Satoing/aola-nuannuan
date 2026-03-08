// AvatarRenderer.cpp - 角色渲染器实现
#include "AvatarRenderer.h"
#include "ConfigLoader.h"
#include <algorithm>
#include <iostream>
#include <unordered_set>
#include <thread>
#include <condition_variable>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

AvatarRenderer::AvatarRenderer(ResourceManager* resMgr, const std::string& baseUrl)
    : m_resMgr(resMgr), m_baseUrl(baseUrl), m_model(0), m_dirty(true) {
}

AvatarRenderer::~AvatarRenderer() {
    for (auto& rp : m_renderParts) {
        if (rp.texture) SDL_DestroyTexture(rp.texture);
    }
    m_renderParts.clear();
}

void AvatarRenderer::SetClothes(const std::vector<int>& clothIDs) {
    m_clothIDs = clothIDs;
    // 设置服装后检查并补回缺失的默认部件（头发、眼睛等）
    CheckAndRestoreDefaults();
    m_dirty = true;
}

void AvatarRenderer::AddCloth(int clothID) {
    // 检查是否已穿着
    for (int id : m_clothIDs) {
        if (id == clothID) return;
    }

    // 查找新服装的数据
    auto itNew = g_clothData.find(clothID);
    if (itNew == g_clothData.end()) return;

    // 收集新服装所有非 forHide 部件的 clothesPos（挂载点）
    // forHide 部件只用于渲染时隐藏对应位置的部件，不参与互斥逻辑
    std::unordered_set<int> newPositions;
    for (const auto& part : itNew->second.clothParts) {
        if (!part.forHide) {
            newPositions.insert(part.clothesPos);
        }
    }

    // 移除与新服装挂载点冲突的旧服装（基于 clothesPos 互斥）
    // 只比较双方的非 forHide 部件
    m_clothIDs.erase(
        std::remove_if(m_clothIDs.begin(), m_clothIDs.end(), [&newPositions](int id) {
            auto it = g_clothData.find(id);
            if (it == g_clothData.end()) return false;
            for (const auto& part : it->second.clothParts) {
                if (!part.forHide && newPositions.count(part.clothesPos)) {
                    return true;  // 有挂载点冲突，移除此服装
                }
            }
            return false;
        }),
        m_clothIDs.end()
    );

    m_clothIDs.push_back(clothID);

    // 参考前端 checkDefaultClothes + addDefaultCloth 逻辑：
    // 检查默认服装的挂载点是否仍需要补回
    CheckAndRestoreDefaults();

    m_dirty = true;
}

void AvatarRenderer::CheckAndRestoreDefaults() {
    // 参考前端 ClothesService.checkDefaultClothes 逻辑：
    // 1. 建立默认挂载点映射表 defaultIds[pos] = clothID
    // 2. 遍历当前穿着服装的所有部件，覆盖的挂载点从映射中清除
    // 3. 映射中剩余未被覆盖的 clothID 需要补回

    // 复制一份默认映射（避免修改原始数据）
    auto defaultIds = m_defaultSkinMap;

    // 遍历当前穿着的所有服装，标记已覆盖的挂载点
    // 只有非 forHide 的普通部件才算真正覆盖了该挂载点
    // forHide 部件只在渲染时隐藏对应位置，不影响默认服装的补回判断
    for (int clothID : m_clothIDs) {
        auto it = g_clothData.find(clothID);
        if (it == g_clothData.end()) continue;
        for (const auto& part : it->second.clothParts) {
            if (part.forHide) continue;  // forHide 部件不算覆盖
            auto defIt = defaultIds.find(part.clothesPos);
            if (defIt != defaultIds.end()) {
                defaultIds.erase(defIt);  // 该挂载点已被覆盖，无需默认服装
            }
        }
    }

    // 收集需要补回的默认服装ID（去重）
    std::unordered_set<int> shouldRestore;
    for (const auto& [pos, defClothID] : defaultIds) {
        shouldRestore.insert(defClothID);
    }

    // 补回缺失的默认服装
    for (int defID : shouldRestore) {
        bool alreadyWearing = false;
        for (int id : m_clothIDs) {
            if (id == defID) { alreadyWearing = true; break; }
        }
        if (!alreadyWearing) {
            m_clothIDs.push_back(defID);
        }
    }

    // ---- 额外默认部件检查（头发前/后、眼睛嘴巴等） ----
    // 参考前端 ClothesService.extraDefaultSkins / extraDefaultSkinsId 逻辑：
    // 检查当前穿着中是否有部件占据了这些特殊挂载点，如果没有，补回默认服装
    for (size_t i = 0; i < m_extraDefaultPositions.size() && i < m_extraDefaultClothIDs.size(); i++) {
        int extraPos = m_extraDefaultPositions[i];
        int extraClothID = m_extraDefaultClothIDs[i];
        if (extraPos <= 0 || extraClothID <= 0) continue;

        // 检查当前穿着中是否有任何服装的部件占据了此挂载点
        bool hasPos = false;
        for (int clothID : m_clothIDs) {
            auto it = g_clothData.find(clothID);
            if (it == g_clothData.end()) continue;
            for (const auto& part : it->second.clothParts) {
                if (part.forHide) continue;
                if (part.clothesPos == extraPos) {
                    hasPos = true;
                    break;
                }
            }
            if (hasPos) break;
        }

        // 如果没有部件在此挂载点，补回默认服装
        if (!hasPos) {
            bool alreadyWearing = false;
            for (int id : m_clothIDs) {
                if (id == extraClothID) { alreadyWearing = true; break; }
            }
            if (!alreadyWearing) {
                m_clothIDs.push_back(extraClothID);
            }
        }
    }
}

void AvatarRenderer::RemoveCloth(int clothID) {
    m_clothIDs.erase(
        std::remove(m_clothIDs.begin(), m_clothIDs.end(), clothID),
        m_clothIDs.end()
    );
    // 脱衣后检查并补回缺失的默认服装
    CheckAndRestoreDefaults();
    m_dirty = true;
}

bool AvatarRenderer::IsWearing(int clothID) const {
    for (int id : m_clothIDs) {
        if (id == clothID) return true;
    }
    return false;
}

int AvatarRenderer::GetTotalFashionValue() const {
    int total = 0;
    for (int clothID : m_clothIDs) {
        auto it = g_clothData.find(clothID);
        if (it != g_clothData.end()) {
            total += it->second.clothVal;
        }
    }
    return total;
}

void AvatarRenderer::LoadParts() {
    if (!m_dirty) return;
    m_dirty = false;

    // 增加加载代数，用于取消过期的后台加载
    m_loadGeneration++;
    int currentGen = m_loadGeneration;

    // 计算 hideBodyParts 标志（参考前端 updateContainerView）
    int hideBodyParts = 0;
    for (int clothID : m_clothIDs) {
        auto cabIt = g_clothAffectBody.find(clothID);
        if (cabIt != g_clothAffectBody.end()) {
            hideBodyParts |= cabIt->second;
        }
    }

    // 收集当前所有 forHide 部件占据的挂载点（用于隐藏对应位置的其他部件）
    std::unordered_set<int> hidePositions;
    for (int clothID : m_clothIDs) {
        auto it = g_clothData.find(clothID);
        if (it == g_clothData.end()) continue;
        for (const auto& part : it->second.clothParts) {
            if (part.forHide) {
                hidePositions.insert(part.clothesPos);
            }
        }
    }

    // SkinPosInfo 常量（与前端一致）
    const int POS_DI_SE_SHOU_BI_QIAN = 36;   // 底色前手臂（右手）
    const int POS_DI_SE_SHOU_BI_HOU = 67;    // 底色后手臂（左手）
    const int POS_DI_SE_QU_GAN_HE_TUI = 53;  // 底色躯干和腿（旧人模合并部件）
    const int POS_DI_SE_QU_GAN = 54;         // 底色躯干（新人模独立部件）
    const int POS_DI_SE_TUI = 55;            // 底色腿（新人模独立部件）

    // 收集所有部件信息
    struct PartLoadInfo {
        int clothesPos;
        int classify = 0;  // 服装分类（用于判断背景/前景）
        std::string pngUrl;
        std::string jsonUrl;
        ClipMode clipMode = CLIP_NONE;
    };
    std::vector<PartLoadInfo> loadInfos;

    for (int clothID : m_clothIDs) {
        auto it = g_clothData.find(clothID);
        if (it == g_clothData.end()) continue;
        const Cloth& cloth = it->second;
        for (const auto& part : cloth.clothParts) {
            // 跳过 forHide 部件（不实际渲染，只用于标记隐藏）
            if (part.forHide) continue;

            // 跳过被 forHide 位置覆盖的部件
            if (hidePositions.count(part.clothesPos)) continue;

            // 参考前端 updateBodyPart: 根据 hideBodyParts 隐藏底色部件
            if (part.clothesPos == POS_DI_SE_SHOU_BI_QIAN && (hideBodyParts & HIDE_RIGHT_HAND)) continue;
            if (part.clothesPos == POS_DI_SE_SHOU_BI_HOU && (hideBodyParts & HIDE_LEFT_HAND)) continue;
            // 旧人模的躯干和腿合在一起（pos=53），参考前端 updateMainBodyState:
            // - 同时 HIDE_LEG + HIDE_UP_BODY → 完全隐藏
            // - 只 HIDE_LEG → 用遮罩只显示上半身
            // - 只 HIDE_UP_BODY → 用遮罩只显示下半身
            ClipMode partClipMode = CLIP_NONE;
            if (part.clothesPos == POS_DI_SE_QU_GAN_HE_TUI) {
                bool hideLeg = (hideBodyParts & HIDE_LEG) != 0;
                bool hideUp  = (hideBodyParts & HIDE_UP_BODY) != 0;
                if (hideLeg && hideUp) continue;  // 两个都隐藏 → 完全不显示
                if (hideLeg) partClipMode = CLIP_SHOW_UPPER;  // 只隐藏腿 → 只显示上半身
                if (hideUp)  partClipMode = CLIP_SHOW_LOWER;  // 只隐藏上半身 → 只显示下半身
            }
            // 新人模独立的腿部部件(pos=55)
            if (part.clothesPos == POS_DI_SE_TUI && (hideBodyParts & HIDE_LEG)) continue;
            // 新人模独立的躯干部件(pos=54)
            if (part.clothesPos == POS_DI_SE_QU_GAN && (hideBodyParts & HIDE_UP_BODY)) continue;

            PartLoadInfo info;
            info.clothesPos = part.clothesPos;
            info.classify = cloth.classify;
            info.pngUrl = GetPngURL(m_baseUrl, cloth, part, m_model);
            info.jsonUrl = GetJsonURL(m_baseUrl, cloth, part, m_model);
            info.clipMode = partClipMode;
            loadInfos.push_back(info);
        }
    }

    if (loadInfos.empty()) {
        // 释放旧纹理
        for (auto& rp : m_renderParts) {
            if (rp.texture) SDL_DestroyTexture(rp.texture);
        }
        m_renderParts.clear();
        return;
    }

    m_loading.store(true);

    // 后台线程：使用并发下载所有资源（大幅提升加载速度）
    std::thread([this, loadInfos, currentGen]() {
        // 为每个部件创建一个结果槽位
        std::vector<PendingPartData> results(loadInfos.size());
        std::vector<std::thread> workers;
        std::atomic<bool> cancelled{false};

        // 限制并发线程数（避免过多线程开销和网络拥塞）
        const int MAX_CONCURRENT = 8;
        std::atomic<int> activeCount{0};
        std::mutex cvMutex;
        std::condition_variable cv;

        for (size_t i = 0; i < loadInfos.size(); i++) {
            // 等待并发槽位可用
            {
                std::unique_lock<std::mutex> lock(cvMutex);
                cv.wait(lock, [&]{ return activeCount.load() < MAX_CONCURRENT || cancelled.load(); });
            }

            if (cancelled.load() || currentGen != m_loadGeneration) {
                cancelled.store(true);
                break;
            }

            activeCount++;
            workers.emplace_back([this, &loadInfos, &results, &activeCount, &cv, &cancelled, currentGen, i]() {
                if (cancelled.load() || currentGen != m_loadGeneration) {
                    cancelled.store(true);
                    activeCount--;
                    cv.notify_one();
                    return;
                }

                const auto& info = loadInfos[i];

                // 并发下载 PNG（使用内存缓存版本）
                auto pngData = ResourceManager::HttpGetCached(info.pngUrl);

                // 并发下载 JSON 并解析坐标
                int posX = 0, posY = 0;
                if (!pngData.empty()) {
                    auto jsonData = ResourceManager::HttpGetCached(info.jsonUrl);
                    if (!jsonData.empty()) {
                        try {
                            std::string jsonStr(jsonData.begin(), jsonData.end());
                            auto j = json::parse(jsonStr);
                            auto& rect = j["rect"];
                            posX = rect[0].get<int>();
                            posY = rect[1].get<int>();
                        } catch (...) {}
                    }
                }

                results[i].clothesPos = info.clothesPos;
                results[i].classify = info.classify;
                results[i].pngData = std::move(pngData);
                results[i].posX = posX;
                results[i].posY = posY;
                results[i].clipMode = info.clipMode;

                activeCount--;
                cv.notify_one();
            });
        }

        // 等待所有下载线程完成
        for (auto& t : workers) {
            if (t.joinable()) t.join();
        }

        // 检查是否已过期
        if (cancelled.load() || currentGen != m_loadGeneration) {
            m_loading.store(false);
            return;
        }

        // 移除空结果
        results.erase(
            std::remove_if(results.begin(), results.end(),
                [](const PendingPartData& pd) { return pd.pngData.empty(); }),
            results.end()
        );

        // 按 clothesPos 从大到小排序（大的在底层先绘制）
        std::sort(results.begin(), results.end(), [](const PendingPartData& a, const PendingPartData& b) {
            return a.clothesPos > b.clothesPos;
        });

        // 将结果放入待处理队列
        {
            std::lock_guard<std::mutex> lock(m_pendingMutex);
            m_pendingParts = std::move(results);
            m_pendingReady = true;
            m_pendingGeneration = currentGen;
        }
    }).detach();
}

void AvatarRenderer::Update(SDL_Renderer* renderer) {
    std::lock_guard<std::mutex> lock(m_pendingMutex);
    if (!m_pendingReady) return;
    m_pendingReady = false;

    // 检查是否过期
    if (m_pendingGeneration != m_loadGeneration) {
        m_pendingParts.clear();
        return;
    }

    // 释放旧纹理
    for (auto& rp : m_renderParts) {
        if (rp.texture) SDL_DestroyTexture(rp.texture);
    }
    m_renderParts.clear();

    for (auto& pd : m_pendingParts) {
        if (pd.pngData.empty()) continue;

        // 在主线程创建纹理
        SDL_RWops* rw = SDL_RWFromMem(pd.pngData.data(), (int)pd.pngData.size());
        if (!rw) continue;

        SDL_Surface* surface = IMG_Load_RW(rw, 1);
        if (!surface) continue;

        SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surface);
        int w = surface->w;
        int h = surface->h;
        SDL_FreeSurface(surface);

        if (!tex) continue;
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);

        RenderPart rp;
        rp.clothesPos = pd.clothesPos;
        rp.classify = pd.classify;
        rp.texture = tex;
        rp.x = pd.posX;
        rp.y = pd.posY;
        rp.w = w;
        rp.h = h;
        rp.clipMode = pd.clipMode;
        m_renderParts.push_back(rp);
    }

    m_pendingParts.clear();
    m_loading.store(false);
    std::cout << "加载完成: " << m_renderParts.size() << " 个部件" << std::endl;
}

void AvatarRenderer::Render(SDL_Renderer* renderer, int centerX, int bottomY) {
    // 前端遮罩参数（来源于 a40000.fla，经 MODEL_SCALE_FROM_FLASH=1.67 缩放）
    // 用于旧人模 pos=53 躯干+腿合并部件的部分隐藏
    const float SCALE = 1.67f;

    // 旧人模缩放比例（参考前端 ClothesModel.oldModelScale = 0.7）
    // 新人模不缩放
    const float modelScale = (m_model == 0) ? 0.65f : 1.0f;

    // 判断是否为背景/前景部件（不参与人物缩放，保持原始大小）
    // 通过 classify 判断：classify=4 (旧背景) 或 classify=5 (新背景/前景)
    auto isBgOrFg = [](int classify) {
        return classify == CLASSIFY_OLD_BG || classify == CLASSIFY_NEW_BG;
    };

    // 缩放以 centerX（即 x=0 原点）为中心进行，不需要动态居中补偿
    // 这样无论穿什么衣服，角色位置始终稳定不偏移
    int centerCompX = 0;

    // 遮罩参数（应用缩放 + 居中补偿）
    // HIDE_LEG 时的遮罩：只显示上半身
    const int CLIP_UPPER_X = (int)(-48.45f * SCALE * modelScale) + centerCompX;
    const int CLIP_UPPER_Y = (int)(-259.05f * SCALE * modelScale);
    const int CLIP_UPPER_W = (int)(113.0f * SCALE * modelScale);
    const int CLIP_UPPER_H = (int)(130.0f * SCALE * modelScale);
    // HIDE_UP_BODY 时的遮罩：只显示下半身
    const int CLIP_LOWER_X = (int)(-78.45f * SCALE * modelScale) + centerCompX;
    const int CLIP_LOWER_Y = (int)(-131.0f * SCALE * modelScale);
    const int CLIP_LOWER_W = (int)(130.0f * SCALE * modelScale);
    const int CLIP_LOWER_H = (int)(170.0f * SCALE * modelScale);

    // 渲染所有部件（按已排好的顺序）
    for (const auto& rp : m_renderParts) {
        if (!rp.texture) continue;

        SDL_Rect dst;

        if (isBgOrFg(rp.classify)) {
            // 背景/前景部件：不缩放，使用原始坐标和尺寸
            dst.x = centerX + rp.x;
            dst.y = bottomY + rp.y;
            dst.w = rp.w;
            dst.h = rp.h;
        } else {
            // 非背景部件：应用缩放（以脚底中心为缩放原点），加居中补偿
            dst.x = centerX + (int)(rp.x * modelScale) + centerCompX;
            dst.y = bottomY + (int)(rp.y * modelScale);
            dst.w = (int)(rp.w * modelScale);
            dst.h = (int)(rp.h * modelScale);
        }

        // 对 pos=53 的旧人模躯干+腿部件应用裁剪遮罩
        if (rp.clipMode != CLIP_NONE) {
            SDL_Rect clipRect;
            if (rp.clipMode == CLIP_SHOW_UPPER) {
                // 只显示上半身
                clipRect.x = centerX + CLIP_UPPER_X;
                clipRect.y = bottomY + CLIP_UPPER_Y;
                clipRect.w = CLIP_UPPER_W;
                clipRect.h = CLIP_UPPER_H;
            } else { // CLIP_SHOW_LOWER
                // 只显示下半身
                clipRect.x = centerX + CLIP_LOWER_X;
                clipRect.y = bottomY + CLIP_LOWER_Y;
                clipRect.w = CLIP_LOWER_W;
                clipRect.h = CLIP_LOWER_H;
            }
            SDL_RenderSetClipRect(renderer, &clipRect);
            SDL_RenderCopy(renderer, rp.texture, nullptr, &dst);
            SDL_RenderSetClipRect(renderer, nullptr);  // 恢复无裁剪
        } else {
            SDL_RenderCopy(renderer, rp.texture, nullptr, &dst);
        }
    }
}
