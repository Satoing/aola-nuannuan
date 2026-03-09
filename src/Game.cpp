// Game.cpp - 换装游戏实现（参考奥拉星 UI 三栏布局）
#include "Game.h"
#include "ConfigLoader.h"
#include <iostream>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <vector>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <nlohmann/json.hpp>
#include <sqlite3.h>
#include <SDL_image.h>
#include <ctime>

#ifdef ANDROID_PLATFORM
#include "android_platform.h"
#include <android/log.h>
#include <jni.h>
#include <sys/stat.h>
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "Game", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "Game", __VA_ARGS__)
// Android 平台的文件系统辅助函数
static bool android_file_exists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}
static void android_create_dirs(const std::string& path) {
    AndroidPlatform::CreateDirectories(path);
}
#else
#include <filesystem>
namespace fs = std::filesystem;
#endif
using json = nlohmann::json;

// 辅助函数：在唤起文本输入前设置输入区域（Android 上必须设置才能弹出软键盘）
// 注意：Android 上用户可能通过系统返回键收起软键盘，此时 SDL 内部仍认为 TextInput 处于激活状态，
// 导致再次调用 SDL_StartTextInput() 时不会重新弹出键盘。因此先 Stop 再 Start 确保重置状态。
static void ActivateTextInput(int x, int y, int w, int h) {
    SDL_StopTextInput();
    SDL_Rect rect = { x, y, w, h };
    SDL_SetTextInputRect(&rect);
    SDL_StartTextInput();
}

static const char* BASE_URL = "https://aola.100bt.com/h5/";

// ==================== 粉红色基调配色方案（参考奥拉星UI） ====================
// 左侧分类栏（稍深粉底，深色文字）
static const SDL_Color C_LEFT_BG           = {238, 218, 225, 255};    // 稍深粉背景
static const SDL_Color C_LEFT_ITEM_ACTIVE   = {255, 245, 248, 255};   // 选中项：浅粉白
static const SDL_Color C_LEFT_ITEM_HOVER    = {248, 232, 238, 255};   // 悬停：浅粉
static const SDL_Color C_LEFT_ACCENT        = {210, 100, 130, 255};   // 粉红指示条
static const SDL_Color C_LEFT_TEXT          = {80, 55, 65, 255};      // 分类文字：深玫灰
static const SDL_Color C_LEFT_TEXT_ACTIVE   = {140, 45, 70, 255};     // 选中文字：玫红
static const SDL_Color C_LEFT_PINYIN        = {180, 145, 158, 255};   // 拼音：粉灰色

// 中间角色展示区
static const SDL_Color C_CENTER_BG         = {248, 235, 240, 255};   // 浅粉白

// 右侧面板
static const SDL_Color C_RIGHT_BG          = {252, 243, 247, 255};   // 右侧面板：浅粉白
static const SDL_Color C_CARD_BG           = {255, 250, 252, 255};   // 卡片：极浅粉白
static const SDL_Color C_CARD_HOVER        = {250, 230, 238, 255};   // 卡片悬停：浅粉
static const SDL_Color C_CARD_SELECTED     = {245, 215, 228, 255};   // 卡片选中：浅玫瑰
static const SDL_Color C_CARD_SEL_BORDER   = {210, 100, 130, 255};   // 选中边框：粉红
static const SDL_Color C_CARD_TEXT         = {85, 55, 70, 255};      // 卡片文字：深玫灰
static const SDL_Color C_SEARCH_BG         = {255, 250, 252, 255};   // 搜索框：极浅粉白
static const SDL_Color C_SEARCH_BORDER     = {230, 200, 212, 255};   // 搜索框边框：浅粉
static const SDL_Color C_SEARCH_TEXT       = {190, 155, 168, 255};   // 搜索提示文字

// 底部栏
static const SDL_Color C_BOTTOM_BG         = {252, 243, 247, 245};   // 底部栏：浅粉白
static const SDL_Color C_BTN_RESET         = {195, 165, 175, 255};   // 重置按钮：粉灰
static const SDL_Color C_BTN_SAVE          = {220, 90, 120, 255};    // 保存按钮：玫红
static const SDL_Color C_BTN_LOAD          = {185, 65, 100, 255};    // 加载按钮：深玫红
static const SDL_Color C_BTN_SCREENSHOT    = {165, 110, 125, 255};   // 截图按钮：暗玫瑰
static const SDL_Color C_BTN_SHARE         = {145, 95, 110, 255};    // 分享按钮：深玫瑰
static const SDL_Color C_BTN_MODEL         = {245, 225, 232, 255};   // 人模按钮：浅粉
static const SDL_Color C_BTN_MODEL_ACTIVE  = {185, 70, 105, 255};    // 人模按钮激活：深玫红

// 通用
static const SDL_Color C_WHITE             = {255, 255, 255, 255};   // 纯白
static const SDL_Color C_GRAY              = {170, 140, 155, 255};   // 粉灰
static const SDL_Color C_FASHION_VAL       = {210, 85, 120, 255};    // 时尚值：玫红
static const SDL_Color C_VAL_TAG           = {210, 90, 125, 200};    // 时尚值角标背景
static const SDL_Color C_DIVIDER           = {235, 215, 222, 255};   // 分割线：浅粉灰

// 前置声明
int MapPosToCat(int pos);

// ==================== 构造 / 析构 ====================
Game::Game()
    : m_window(nullptr), m_renderer(nullptr)
    , m_fontLarge(nullptr), m_font(nullptr), m_fontSmall(nullptr), m_fontTiny(nullptr)
    , m_resMgr(nullptr), m_avatar(nullptr)
    , m_width(0), m_height(0), m_running(false)
, m_currentCategory(CAT_SUIT), m_gridScrollOffset(0), m_catScrollOffset(0)
    , m_model(0), m_searchActive(false), m_dpiScale(1.0f)
    , m_designWidth(0), m_designHeight(0)
    , m_outfitDB(nullptr)
    , m_outfitDialogActive(false), m_outfitMsgTime(0), m_outfitDialogIsLoad(false)
    , m_outfitShowToken(false)
    , m_outfitSelectedIdx(-1), m_outfitListScroll(0), m_outfitRenameIdx(-1), m_outfitMenuIdx(-1)
    , m_imeCursor(0), m_imeSelLen(0)
    , m_modelSwitchAnimX(0.0f), m_modelSwitchTargetX(0.0f)
    , m_rightPanelCollapsed(false), m_rightPanelAnimProgress(0.0f)
    , m_touchActive(false), m_touchStartX(0), m_touchStartY(0)
    , m_touchLastY(0), m_touchIsDrag(false), m_touchScrollTarget(0)
    , m_scrollVelocity(0.0f), m_touchStartTime(0) {
}

Game::~Game() {
    Cleanup();
}

// ==================== 初始化 ====================
bool Game::Init(const std::string& title, int width, int height) {
    m_width = width;
    m_height = height;

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL_Init 失败: " << SDL_GetError() << std::endl;
        return false;
    }
    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
        std::cerr << "IMG_Init 失败: " << IMG_GetError() << std::endl;
        return false;
    }
    if (TTF_Init() < 0) {
        std::cerr << "TTF_Init 失败: " << TTF_GetError() << std::endl;
        return false;
    }

    // 设置纹理缩放质量为线性过滤，避免文本和图片在缩放时模糊/锯齿
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "best");

#ifdef ANDROID_PLATFORM
    // Android: 全屏模式，获取实际屏幕尺寸
    Uint32 windowFlags = SDL_WINDOW_SHOWN | SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_OPENGL;
    int realScreenW = 0, realScreenH = 0;
    {
        SDL_DisplayMode dm;
        SDL_GetCurrentDisplayMode(0, &dm);
        realScreenW = dm.w;
        realScreenH = dm.h;
    }
    if (width == 0 || height == 0) {
        width = realScreenW;
        height = realScreenH;
    }
    m_window = SDL_CreateWindow(title.c_str(),
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        width, height, windowFlags);
#else
    m_window = SDL_CreateWindow(title.c_str(),
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        width, height, SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE);
#endif
    if (!m_window) return false;

    m_renderer = SDL_CreateRenderer(m_window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!m_renderer) return false;

    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);

    // HiDPI 支持：根据实际渲染尺寸调整窗口和逻辑尺寸
    int renderW, renderH;
    SDL_GetRendererOutputSize(m_renderer, &renderW, &renderH);

#ifdef ANDROID_PLATFORM
    // Android: 使用固定逻辑宽度（与桌面版一致），高度按屏幕比例计算
    // 这样所有 UI 布局常量（LEFT_PANEL_W=150, RIGHT_PANEL_W=440 等）都能保持合理比例
    const int LOGICAL_WIDTH = 1280;
    float screenAspect = (float)renderH / (float)renderW;
    int logicalH = (int)(LOGICAL_WIDTH * screenAspect + 0.5f);
    m_width = LOGICAL_WIDTH;
    m_height = logicalH;
    m_dpiScale = (float)renderW / (float)LOGICAL_WIDTH;
    m_designWidth = LOGICAL_WIDTH;
    m_designHeight = logicalH;
    LOGI("Android 逻辑分辨率: %dx%d, 实际渲染: %dx%d, DPI缩放: %.2f", m_width, m_height, renderW, renderH, m_dpiScale);
#else
    m_dpiScale = (float)renderW / (float)width;
    m_designWidth = width;
    m_designHeight = height;

    // 根据实际渲染尺寸和 DPI 缩放计算正确的逻辑尺寸
    // 这样可以确保 viewport 无偏移，scale 在 X/Y 方向一致
    m_width = (int)(renderW / m_dpiScale + 0.5f);
    m_height = (int)(renderH / m_dpiScale + 0.5f);

    // 如果窗口尺寸和计算出的逻辑尺寸不同，调整窗口大小
    if (m_width != width || m_height != height) {
        SDL_SetWindowSize(m_window, m_width, m_height);
        // 重新获取渲染尺寸（窗口大小变化后渲染尺寸可能也变）
        SDL_GetRendererOutputSize(m_renderer, &renderW, &renderH);
        m_dpiScale = (float)renderW / (float)m_width;
    }
#endif

    // 设置逻辑分辨率（SDL 2.26+ 会自动映射鼠标坐标到逻辑坐标）
    SDL_RenderSetLogicalSize(m_renderer, m_width, m_height);

    std::cout << "窗口: " << m_width << "x" << m_height
              << " 渲染: " << renderW << "x" << renderH
              << " DPI缩放: " << m_dpiScale << std::endl;

    // 加载字体（按物理像素大小打开，确保 HiDPI 清晰）
    ReloadFonts();
    if (!m_fontLarge) { std::cerr << "无法加载字体!" << std::endl; return false; }

    // 设置窗口图标（Q萌软软梦）
    {
        SDL_Surface* iconSurf = IMG_Load("icon.png");
        if (iconSurf) {
            SDL_SetWindowIcon(m_window, iconSurf);
            SDL_FreeSurface(iconSurf);
        } else {
            std::cerr << "无法加载图标: " << IMG_GetError() << std::endl;
        }
    }

    m_resMgr = new ResourceManager(m_renderer);
    m_avatar = new AvatarRenderer(m_resMgr, BASE_URL);
    m_avatar->SetModel(m_model);

    BuildCategoryData();

    // 初始化装扮方案数据库（SQLite）
    InitOutfitDB();

    ApplyModelDefaults(m_model);
    m_avatar->SetClothes(m_defaultClothes);
    m_avatar->LoadParts();

    m_running = true;
    return true;
}

void Game::Cleanup() {
    // 关闭装扮方案数据库
    CloseOutfitDB();

    // 清理缩略图缓存
    for (auto& [id, tex] : m_thumbCache) {
        if (tex) SDL_DestroyTexture(tex);
    }
    m_thumbCache.clear();

    delete m_avatar; m_avatar = nullptr;
    delete m_resMgr; m_resMgr = nullptr;
    if (m_fontTiny)  { TTF_CloseFont(m_fontTiny);  m_fontTiny = nullptr; }
    if (m_fontSmall) { TTF_CloseFont(m_fontSmall); m_fontSmall = nullptr; }
    if (m_font)      { TTF_CloseFont(m_font);      m_font = nullptr; }
    if (m_fontLarge) { TTF_CloseFont(m_fontLarge); m_fontLarge = nullptr; }
    if (m_renderer)  { SDL_DestroyRenderer(m_renderer); m_renderer = nullptr; }
    if (m_window)    { SDL_DestroyWindow(m_window);   m_window = nullptr; }
    TTF_Quit(); IMG_Quit(); SDL_Quit();
}

void Game::Run() {
    while (m_running) {
        HandleEvents();
        Update();
        Render();
        SDL_Delay(16);
    }
}

// ==================== 分类逻辑 ====================
int Game::MapClothTypeToCat(int clothType) {
    return CAT_ACCESSORY;
}

int MapPosToCat(int pos) {
    if (pos == 26 || pos == 36 || pos == 53 || pos == 54 || pos == 55
        || pos == 63 || pos == 67 || pos == 33 || pos == 64) return CAT_SKIN;
    if (pos == 19 || pos == 28 || pos == 73) return CAT_HAIR;
    if (pos == 24 || pos == 25 || pos == 21 || pos == 74) return CAT_FACE;
    if (pos == 17 || pos == 76 || pos == 18 || pos == 75 || pos == 10) return CAT_HEAD_DECO;
    if (pos == 34 || pos == 39 || pos == 61 || pos == 16 || pos == 50
        || pos == 68 || pos == 13 || pos == 14) return CAT_TOP;
    if (pos == 41 || pos == 42 || pos == 43 || pos == 45 || pos == 48
        || pos == 49 || pos == 52 || pos == 57) return CAT_BOTTOM;
    if (pos == 40 || pos == 47 || pos == 51 || pos == 56) return CAT_SHOES;
    if (pos == 22 || pos == 30 || pos == 20 || pos == 29 || pos == 23
        || pos == 31 || pos == 71 || pos == 32 || pos == 72
        || pos == 35 || pos == 38 || pos == 44 || pos == 58
        || pos == 46 || pos == 70 || pos == 69 || pos == 15
        || pos == 60 || pos == 62 || pos == 66 || pos == 27) return CAT_ACCESSORY;
    if (pos == 4 || pos == 86 || pos == 12 || pos == 84 || pos == 11 || pos == 85) return CAT_WINGS;
    if (pos == 37 || pos == 77 || pos == 59 || pos == 83 || pos == 65) return CAT_HANDHELD;
    if (pos == 91 || pos == 88 || pos == 89) return CAT_BACKGROUND;
    if (pos <= 9 || pos == 78 || pos == 81 || pos == 82 || pos == 87) return CAT_WINGS;
    return CAT_ACCESSORY;
}

void Game::BuildCategoryData() {
    m_categoryItems.clear();
    for (auto& [id, cloth] : g_clothData) {
        int cat = CAT_ACCESSORY;
        if (!cloth.clothParts.empty()) {
            cat = MapPosToCat(cloth.clothParts[0].clothesPos);
        }
        ClothItem item;
        item.clothID = cloth.clothID;
        item.name = cloth.clothName;
        item.clothType = cloth.clothType;
        item.clothVal = cloth.clothVal;
        m_categoryItems[cat].push_back(item);
    }
    for (auto& [cat, items] : m_categoryItems) {
        std::sort(items.begin(), items.end(), [](const ClothItem& a, const ClothItem& b) {
            return a.clothID > b.clothID;
        });
    }

    // 构建套装列表
    m_suitItems.clear();
    for (auto& [id, suit] : g_suitData) {
        SuitItem si;
        si.suitId = suit.suitId;
        si.name = suit.suitName;
        si.clothesList = suit.clothesList;
        si.totalVal = 0;
        si.thumbClothID = 0;
        // 按优先级选择最具代表性的服装作为套装缩略图
        // 优先顺序: 上衣 > 下装 > 头发 > 翅膀 > 头饰 > 配饰 > 其他
        static const int thumbPriority[] = {
            CAT_TOP, CAT_BOTTOM, CAT_HAIR, CAT_WINGS, CAT_HEAD_DECO,
            CAT_ACCESSORY, CAT_HANDHELD, CAT_FACE, CAT_SHOES, CAT_SKIN, CAT_BACKGROUND
        };
        int bestPriority = 999;
        for (int cid : suit.clothesList) {
            auto it = g_clothData.find(cid);
            if (it != g_clothData.end()) {
                si.totalVal += it->second.clothVal;
                // 判断这件衣服属于哪个分类
                int cat = CAT_ACCESSORY;
                if (!it->second.clothParts.empty()) {
                    cat = MapPosToCat(it->second.clothParts[0].clothesPos);
                }
                // 查找该分类的优先级
                int priority = 999;
                for (int p = 0; p < (int)(sizeof(thumbPriority)/sizeof(thumbPriority[0])); p++) {
                    if (thumbPriority[p] == cat) { priority = p; break; }
                }
                if (priority < bestPriority) {
                    bestPriority = priority;
                    si.thumbClothID = cid;
                }
            }
        }
        // 只添加至少有一件有效服装的套装
        if (si.thumbClothID > 0) {
            m_suitItems.push_back(si);
        }
    }
    // 按套装ID降序排列（新套装在前）
    std::sort(m_suitItems.begin(), m_suitItems.end(), [](const SuitItem& a, const SuitItem& b) {
        return a.suitId > b.suitId;
    });

    for (int i = 0; i < CAT_COUNT; i++) {
        if (i == CAT_SUIT) {
            std::cout << GetCategoryName(i) << ": " << m_suitItems.size() << " 套" << std::endl;
        } else {
            auto it = m_categoryItems.find(i);
            int count = (it != m_categoryItems.end()) ? (int)it->second.size() : 0;
            std::cout << GetCategoryName(i) << ": " << count << " 件" << std::endl;
        }
    }
}

const std::vector<ClothItem>& Game::GetCurrentItems() const {
    static std::vector<ClothItem> empty;
    auto it = m_categoryItems.find(m_currentCategory);
    if (it != m_categoryItems.end()) return it->second;
    return empty;
}

std::vector<ClothItem> Game::GetFilteredItems() const {
    const auto& items = GetCurrentItems();
    if (m_searchText.empty()) return items;

    std::vector<ClothItem> filtered;
    for (const auto& item : items) {
        if (item.name.find(m_searchText) != std::string::npos) {
            filtered.push_back(item);
        }
    }
    return filtered;
}

std::vector<SuitItem> Game::GetFilteredSuits() const {
    if (m_searchText.empty()) return m_suitItems;

    std::vector<SuitItem> filtered;
    for (const auto& suit : m_suitItems) {
        if (suit.name.find(m_searchText) != std::string::npos) {
            filtered.push_back(suit);
        }
    }
    return filtered;
}

void Game::WearSuit(const SuitItem& suit) {
    // 如果当前已穿着该套装，则卸下（toggle 逻辑）
    if (IsSuitWorn(suit)) {
        UnwearSuit();
        std::cout << "卸下套装: " << suit.name << std::endl;
        return;
    }
    // 先重置为默认底色
    m_avatar->SetClothes(m_defaultClothes);
    // 逐件穿戴套装中的服装
    for (int clothID : suit.clothesList) {
        if (g_clothData.find(clothID) != g_clothData.end()) {
            m_avatar->AddCloth(clothID);
        }
    }
    m_avatar->LoadParts();
    std::cout << "穿戴套装: " << suit.name << " (" << suit.clothesList.size() << "件)" << std::endl;
}

bool Game::IsSuitWorn(const SuitItem& suit) const {
    for (int cid : suit.clothesList) {
        if (!m_avatar->IsWearing(cid)) return false;
    }
    return true;
}

void Game::UnwearSuit() {
    m_avatar->SetClothes(m_defaultClothes);
    m_avatar->LoadParts();
}

std::vector<ClothItem> Game::GetCurrentWornItems() const {
    std::vector<ClothItem> result;
    const auto& clothes = m_avatar->GetClothes();
    // 构建默认服装集合，用于过滤
    std::unordered_set<int> defaultSet(m_defaultClothes.begin(), m_defaultClothes.end());
    for (int clothID : clothes) {
        if (defaultSet.count(clothID)) continue; // 跳过默认底色服装
        auto it = g_clothData.find(clothID);
        if (it != g_clothData.end()) {
            ClothItem item;
            item.clothID = it->second.clothID;
            item.name = it->second.clothName;
            item.clothType = it->second.clothType;
            item.clothVal = it->second.clothVal;
            result.push_back(item);
        }
    }
    return result;
}

// ==================== 事件处理 ====================
void Game::HandleEvents() {
    // 如果没有活跃的文本输入（弹窗和搜索都不活跃），自动清除 IME 组合文本
    if (!m_outfitDialogActive && !m_searchActive) {
        m_imeComposition.clear();
        m_imeCursor = 0;
        m_imeSelLen = 0;
    }
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                m_running = false;
                break;

            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                    OnWindowResize(event.window.data1, event.window.data2);
                }
                break;

            case SDL_MOUSEWHEEL: {
                int rawMx, rawMy;
                SDL_GetMouseState(&rawMx, &rawMy);
                int mx, my;
                WindowToLogical(rawMx, rawMy, mx, my);
                int rightPanelX = m_width - GetCurrentRightPanelW();

                // 弹窗激活时，滚动方案列表
                if (m_outfitDialogActive) {
                    m_outfitListScroll -= event.wheel.y * 42;
                    break;
                }

                if (mx >= rightPanelX && my >= SEARCH_BAR_H && my < m_height - BOTTOM_BAR_H) {
                    // 右侧面板滚动
                    m_gridScrollOffset -= event.wheel.y * (GRID_CELL_SIZE + GRID_NAME_H + GRID_PADDING);
                    int itemCount = 0;
                    if (m_currentCategory == CAT_SUIT) {
                        itemCount = (int)GetFilteredSuits().size();
                    } else if (m_currentCategory == CAT_CURRENT) {
                        itemCount = (int)GetCurrentWornItems().size();
                    } else {
                        itemCount = (int)GetFilteredItems().size();
                    }
                    int rows = (itemCount + GRID_COLS - 1) / GRID_COLS;
                    int totalH = rows * (GRID_CELL_SIZE + GRID_NAME_H + GRID_PADDING);
                    int visibleH = m_height - SEARCH_BAR_H - BOTTOM_BAR_H;
                    int maxScroll = std::max(0, totalH - visibleH + GRID_PADDING);
                    m_gridScrollOffset = std::max(0, std::min(m_gridScrollOffset, maxScroll));
                } else if (mx < LEFT_PANEL_W) {
                    // 左侧分类滚动
                    m_catScrollOffset -= event.wheel.y * CAT_ITEM_H;
                    int totalH = CAT_COUNT * CAT_ITEM_H;
                    int maxScroll = std::max(0, totalH - m_height);
                    m_catScrollOffset = std::max(0, std::min(m_catScrollOffset, maxScroll));
                }
                break;
            }

            case SDL_MOUSEBUTTONDOWN: {
                if (event.button.button != SDL_BUTTON_LEFT) break;
#ifdef ANDROID_PLATFORM
                // Android 上忽略触摸模拟的鼠标事件（由 FINGER 事件统一处理滚动和点击）
                // 但弹窗模式下保留，因为弹窗按钮不需要滚动
                if (event.button.which == SDL_TOUCH_MOUSEID && !m_outfitDialogActive) break;
#endif
                int mx, my;
                WindowToLogical(event.button.x, event.button.y, mx, my);
                int rightPanelX = m_width - GetCurrentRightPanelW();
                int centerW = rightPanelX - LEFT_PANEL_W;

                // === 弹窗点击处理（优先拦截） ===
                if (m_outfitDialogActive) {
                    // 根据模式确定弹窗尺寸
                    int dlgW, dlgH;
                    if (m_outfitShowToken) {
                        dlgW = 420; dlgH = 260;
                    } else if (m_outfitDialogIsLoad) {
                        dlgW = 400; dlgH = 420;
                    } else {
                        dlgW = 360; dlgH = 200;
                    }
                    int dlgX = (m_width - dlgW) / 2;
                    int dlgY = (m_height - dlgH) / 2;

                    // 按钮区域参数
                    int btnW2 = 100, btnH2 = 32;
                    int btnY2 = m_outfitDialogIsLoad ? (dlgY + dlgH - btnH2 - 16) : (dlgY + dlgH - btnH2 - 18);
                    int btnGap = 20;
                    int totalBtnW = btnW2 * 2 + btnGap;
                    int btnStartX = dlgX + (dlgW - totalBtnW) / 2;

                    // 列表区域参数（仅加载模式使用）
                    int listMargin = 16;
                    int inputH = 36;
                    int inputY = dlgY + 50;
                    int listTop = inputY + inputH + 12;
                    int listBottom = dlgY + dlgH - 64;
                    int listX = dlgX + listMargin;
                    int listW = dlgW - listMargin * 2;
                    int itemH = 42, itemPad = 4;

                    // ======== 口令展示模式的点击处理 ========
                    if (m_outfitShowToken) {
                        // 复制口令按钮（居中，宽180）
                        int copyBtnW = 180, copyBtnH = 36;
                        int copyBtnX = dlgX + (dlgW - copyBtnW) / 2;
                        int copyBtnY = dlgY + 140;
                        // 关闭按钮
                        int closeBtnW = 100, closeBtnH = 32;
                        int closeBtnX = dlgX + (dlgW - closeBtnW) / 2;
                        int closeBtnY = dlgY + dlgH - closeBtnH - 18;

                        if (mx >= copyBtnX && mx < copyBtnX + copyBtnW
                            && my >= copyBtnY && my < copyBtnY + copyBtnH) {
                            // 复制口令到剪贴板
                            SDL_SetClipboardText(m_outfitSavedToken.c_str());
                            m_outfitMsg = "口令已复制到剪贴板！";
                            m_outfitMsgTime = SDL_GetTicks();
                        } else if (mx >= closeBtnX && mx < closeBtnX + closeBtnW
                                   && my >= closeBtnY && my < closeBtnY + closeBtnH) {
                            // 关闭弹窗
                            m_outfitDialogActive = false;
                            m_outfitShowToken = false;
                            if (!m_searchActive) SDL_StopTextInput();
                        } else if (mx < dlgX || mx > dlgX + dlgW || my < dlgY || my > dlgY + dlgH) {
                            // 点击弹窗外关闭
                            m_outfitDialogActive = false;
                            m_outfitShowToken = false;
                            if (!m_searchActive) SDL_StopTextInput();
                        }
                        break;
                    }

                    // 粘贴按钮点击（仅加载模式）
                    if (m_outfitDialogIsLoad) {
                        int pasteBtnSize = 36;
                        int pasteBtnGap = 6;
                        int pasteInputW = 360 - pasteBtnSize - pasteBtnGap;
                        int pasteInputX = dlgX + (dlgW - 360) / 2;
                        int pasteBtnX = pasteInputX + pasteInputW + pasteBtnGap;
                        int pasteBtnY = inputY + (inputH - pasteBtnSize) / 2;
                        if (mx >= pasteBtnX && mx < pasteBtnX + pasteBtnSize
                            && my >= pasteBtnY && my < pasteBtnY + pasteBtnSize) {
                            if (SDL_HasClipboardText()) {
                                char* clipText = SDL_GetClipboardText();
                                if (clipText) {
                                    m_outfitInput = clipText;
                                    SDL_free(clipText);
                                    m_outfitMsg = "已从剪贴板粘贴";
                                    m_outfitMsgTime = SDL_GetTicks();
                                }
                            } else {
                                m_outfitMsg = "剪贴板为空";
                                m_outfitMsgTime = SDL_GetTicks();
                            }
                            break;
                        }
                    }

                    // 输入框区域点击（加载/保存模式均需要）：点击输入框区域时重新激活软键盘
                    {
                        int dlgInputW, dlgInputH, dlgInputX, dlgInputY;
                        if (m_outfitDialogIsLoad) {
                            // 加载模式的输入框区域
                            dlgInputW = 360; dlgInputH = inputH;
                            dlgInputX = dlgX + (dlgW - 360) / 2;
                            dlgInputY = inputY;
                        } else {
                            // 保存模式的输入框区域
                            dlgInputW = 280; dlgInputH = 38;
                            dlgInputX = dlgX + (dlgW - 280) / 2;
                            dlgInputY = dlgY + 74;
                        }
                        if (mx >= dlgInputX && mx < dlgInputX + dlgInputW
                            && my >= dlgInputY && my < dlgInputY + dlgInputH) {
                            ActivateTextInput(dlgInputX, dlgInputY, dlgInputW, dlgInputH);
                            break;
                        }
                    }

                    // 取消按钮
                    if (mx >= btnStartX && mx < btnStartX + btnW2
                        && my >= btnY2 && my < btnY2 + btnH2) {
                        m_outfitDialogActive = false;
                        if (!m_searchActive) SDL_StopTextInput();
                    }
                    // 确认按钮
                    else if (mx >= btnStartX + btnW2 + btnGap
                             && mx < btnStartX + totalBtnW
                             && my >= btnY2 && my < btnY2 + btnH2) {
                        if (m_outfitDialogIsLoad) {
                            // 加载模式：优先使用输入框内容，否则使用列表选中项
                            std::string codeToLoad;
                            if (!m_outfitInput.empty()) {
                                // 检查是否为口令格式
                                if (m_outfitInput.substr(0, 3) == "#AO") {
                                    if (LoadOutfitFromToken(m_outfitInput)) {
                                        m_outfitMsg = "口令加载成功！";
                                    } else {
                                        m_outfitMsg = "口令无效，无法加载";
                                    }
                                    m_outfitMsgTime = SDL_GetTicks();
                                    m_outfitDialogActive = false;
                                    if (!m_searchActive) SDL_StopTextInput();
                                } else {
                                    codeToLoad = m_outfitInput;
                                }
                            } else if (m_outfitSelectedIdx >= 0 && m_outfitSelectedIdx < (int)m_outfitEntries.size()) {
                                codeToLoad = m_outfitEntries[m_outfitSelectedIdx].code;
                            }
                            if (!codeToLoad.empty()) {
                                if (LoadOutfit(codeToLoad)) {
                                    m_outfitMsg = "加载成功！编号: " + codeToLoad;
                                } else {
                                    m_outfitMsg = "编号 " + codeToLoad + " 不存在";
                                }
                                m_outfitMsgTime = SDL_GetTicks();
                                m_outfitDialogActive = false;
                                if (!m_searchActive) SDL_StopTextInput();
                            }
                        } else {
                            // 保存模式：保存当前装扮，进入口令展示模式
                            std::string code = SaveOutfit(m_outfitNameInput);
                            if (code.empty()) {
                                // 保存失败
                                m_outfitMsg = "保存失败，请重试";
                                m_outfitMsgTime = SDL_GetTicks();
                                m_outfitDialogActive = false;
                                if (!m_searchActive) SDL_StopTextInput();
                            } else {
                                m_outfitSavedCode = code;
                                m_outfitSavedToken = EncodeOutfitToken(m_model, m_avatar->GetClothes());
                                m_outfitShowToken = true;
                                m_outfitMsg = "保存成功！编号: " + code;
                                m_outfitMsgTime = SDL_GetTicks();
                            }
                        }
                    }
                    // 齿轮弹出菜单点击处理（优先于列表项点击，因为菜单可能超出列表区域）
                    else if (m_outfitDialogIsLoad && m_outfitMenuIdx >= 0 && m_outfitMenuIdx < (int)m_outfitEntries.size()) {
                        auto& menuEntry = m_outfitEntries[m_outfitMenuIdx];
                        int menuIy = listTop + m_outfitMenuIdx * (itemH + itemPad) - m_outfitListScroll;
                        int gearSize = 22;
                        int menuAnchorX = listX + listW - 8 - gearSize - 6 + gearSize;
                        int menuAnchorY = menuIy + (itemH + gearSize) / 2;

                        int menuItemW = 90, menuItemH = 28, menuPad = 2;
                        bool hasToken = !menuEntry.token.empty();
                        int menuItemCount = hasToken ? 3 : 2;
                        int menuW = menuItemW + menuPad * 2;
                        int menuH = menuItemCount * menuItemH + (menuItemCount - 1) * 2 + menuPad * 2;
                        int menuX = menuAnchorX - menuW - 4;
                        int menuY = menuAnchorY;
                        if (menuY + menuH > dlgY + dlgH - 50) {
                            menuY = menuAnchorY - menuH;
                        }

                        if (mx >= menuX && mx < menuX + menuW && my >= menuY && my < menuY + menuH) {
                            // 判断点击了哪个菜单项
                            int relY = my - menuY - menuPad;
                            int clickedItem = relY / (menuItemH + 2);
                            if (clickedItem >= 0 && clickedItem < menuItemCount) {
                                if (clickedItem == 0) {
                                    // 命名
                                    m_outfitRenameIdx = m_outfitMenuIdx;
                                    m_outfitRenameInput = menuEntry.name;
                                    m_outfitMenuIdx = -1;
                                } else if (clickedItem == 1) {
                                    // 删除
                                    DeleteOutfit(menuEntry.code);
                                    m_outfitMsg = "方案已删除！";
                                    m_outfitMsgTime = SDL_GetTicks();
                                    m_outfitMenuIdx = -1;
                                    m_outfitRenameIdx = -1;
                                    m_outfitSelectedIdx = -1;
                                    LoadOutfitList();
                                } else if (clickedItem == 2 && hasToken) {
                                    // 复制口令
                                    SDL_SetClipboardText(menuEntry.token.c_str());
                                    m_outfitMsg = "口令已复制到剪贴板！";
                                    m_outfitMsgTime = SDL_GetTicks();
                                    m_outfitMenuIdx = -1;
                                }
                            }
                        } else {
                            // 点击了菜单外，关闭菜单（不阻止后续处理其他区域）
                            m_outfitMenuIdx = -1;
                            // 继续判断是否点击了列表项
                            if (mx >= listX && mx < listX + listW
                                && my >= listTop && my < listBottom
                                && !m_outfitEntries.empty()) {
                                int clickedIdx = (my - listTop + m_outfitListScroll) / (itemH + itemPad);
                                if (clickedIdx >= 0 && clickedIdx < (int)m_outfitEntries.size()) {
                                    int iy = listTop + clickedIdx * (itemH + itemPad) - m_outfitListScroll;
                                    int gearSize2 = 22;
                                    int gearX2 = listX + listW - 8 - gearSize2 - 6;
                                    int gearY2 = iy + (itemH - gearSize2) / 2;
                                    // 检查是否处于重命名模式且点击了确认按钮
                                    bool isRenaming2 = (m_outfitRenameIdx == clickedIdx);
                                    int confirmBtnW2 = 50, confirmBtnH2 = 22;
                                    int confirmBtnX2 = listX + listW - 8 - confirmBtnW2 - 4;
                                    int confirmBtnY2 = iy + 2;
                                    if (isRenaming2
                                        && mx >= confirmBtnX2 && mx < confirmBtnX2 + confirmBtnW2
                                        && my >= confirmBtnY2 && my < confirmBtnY2 + confirmBtnH2) {
                                        // 确认重命名
                                        RenameOutfit(m_outfitEntries[clickedIdx].code, m_outfitRenameInput);
                                        m_outfitEntries[clickedIdx].name = m_outfitRenameInput;
                                        m_outfitMsg = "重命名成功！";
                                        m_outfitMsgTime = SDL_GetTicks();
                                        m_outfitRenameIdx = -1;
                                        m_outfitRenameInput.clear();
                                    } else if (mx >= gearX2 && mx < gearX2 + gearSize2
                                        && my >= gearY2 && my < gearY2 + gearSize2) {
                                        m_outfitMenuIdx = clickedIdx;
                                    } else {
                                        if (m_outfitRenameIdx >= 0) {
                                            m_outfitRenameIdx = -1;
                                            m_outfitRenameInput.clear();
                                        }
                                        m_outfitSelectedIdx = clickedIdx;
                                        m_outfitInput = m_outfitEntries[clickedIdx].code;
                                    }
                                }
                            }
                        }
                    }
                    // 列表项点击（仅加载模式，菜单未打开时）
                    else if (m_outfitDialogIsLoad
                             && mx >= listX && mx < listX + listW
                             && my >= listTop && my < listBottom
                             && !m_outfitEntries.empty()) {
                        int clickedIdx = (my - listTop + m_outfitListScroll) / (itemH + itemPad);
                        if (clickedIdx >= 0 && clickedIdx < (int)m_outfitEntries.size()) {
                            int iy = listTop + clickedIdx * (itemH + itemPad) - m_outfitListScroll;
                            // 齿轮按钮坐标（与绘制代码一致）
                            int gearSize = 22;
                            int gearX = listX + listW - 8 - gearSize - 6;
                            int gearY = iy + (itemH - gearSize) / 2;

                            bool isRenaming = (m_outfitRenameIdx == clickedIdx);

                            // 检查是否处于重命名模式且点击了确认按钮
                            int confirmBtnW3 = 50, confirmBtnH3 = 22;
                            int confirmBtnX3 = listX + listW - 8 - confirmBtnW3 - 4;
                            int confirmBtnY3 = iy + 2;

                            if (isRenaming
                                && mx >= confirmBtnX3 && mx < confirmBtnX3 + confirmBtnW3
                                && my >= confirmBtnY3 && my < confirmBtnY3 + confirmBtnH3) {
                                // 确认重命名
                                RenameOutfit(m_outfitEntries[clickedIdx].code, m_outfitRenameInput);
                                m_outfitEntries[clickedIdx].name = m_outfitRenameInput;
                                m_outfitMsg = "重命名成功！";
                                m_outfitMsgTime = SDL_GetTicks();
                                m_outfitRenameIdx = -1;
                                m_outfitRenameInput.clear();
                            }
                            // 检查是否点击了齿轮按钮
                            else if (!isRenaming && mx >= gearX && mx < gearX + gearSize
                                && my >= gearY && my < gearY + gearSize) {
                                m_outfitMenuIdx = clickedIdx;
                            } else if (isRenaming) {
                                // 重命名模式下，点击其他区域不做选中操作
                            } else {
                                if (m_outfitRenameIdx >= 0) {
                                    m_outfitRenameIdx = -1;
                                    m_outfitRenameInput.clear();
                                }
                                m_outfitSelectedIdx = clickedIdx;
                                m_outfitInput = m_outfitEntries[clickedIdx].code;
                            }
                        }
                    }
                    // 点击弹窗外区域：关闭弹窗
                    else if (mx < dlgX || mx > dlgX + dlgW || my < dlgY || my > dlgY + dlgH) {
                        m_outfitDialogActive = false;
                        if (!m_searchActive) SDL_StopTextInput();
                    }
                    break;  // 弹窗激活时不处理其他点击
                }

                // === 收起/展开切换按钮点击 ===
                {
                    int curRightW = GetCurrentRightPanelW();
                    int togglePanelX = m_width - curRightW;
                    int toggleBtnW = 24, toggleBtnH = 60;
                    int toggleBtnX = togglePanelX - toggleBtnW;
                    int toggleBtnY = m_height / 2 - toggleBtnH / 2;
                    if (mx >= toggleBtnX && mx < toggleBtnX + toggleBtnW &&
                        my >= toggleBtnY && my < toggleBtnY + toggleBtnH) {
                        m_rightPanelCollapsed = !m_rightPanelCollapsed;
                        break;
                    }
                }

                // === 左侧分类栏点击 ===
                if (mx < LEFT_PANEL_W) {
                    if (m_searchActive) { m_searchActive = false; SDL_StopTextInput(); }
                    int displayIdx = (my + m_catScrollOffset) / CAT_ITEM_H;
                    const int* displayOrder = GetCategoryDisplayOrder();
                    if (displayIdx >= 0 && displayIdx < CAT_COUNT) {
                        int catIdx = displayOrder[displayIdx];
                        if (catIdx != m_currentCategory) {
                            m_currentCategory = catIdx;
                            m_gridScrollOffset = 0;
                        }
                    }
                    break;
                }

                // === 底部栏按钮 ===
                if (mx >= rightPanelX && my >= m_height - BOTTOM_BAR_H) {
                    // 按钮布局（从右到左）
                    int btnH = 36;
                    int btnY2 = m_height - BOTTOM_BAR_H + (BOTTOM_BAR_H - btnH) / 2;
                    int loadBtnX = rightPanelX + GetCurrentRightPanelW() - 120;
                    int saveBtnX = loadBtnX - 110;
                    int resetX = saveBtnX - 110;

                    if (mx >= loadBtnX && mx < loadBtnX + 100 && my >= btnY2 && my < btnY2 + btnH) {
                        // 加载按钮
                        m_outfitDialogActive = true;
                        m_outfitDialogIsLoad = true;
                        m_outfitInput.clear();
                        m_outfitSelectedIdx = -1;
                        m_outfitListScroll = 0;
                        m_outfitRenameIdx = -1;
                        m_outfitRenameInput.clear();
                        m_outfitMenuIdx = -1;
                        LoadOutfitList();  // 加载已保存方案列表
                        if (!m_searchActive) {
                            ActivateTextInput(m_width / 4, m_height / 3, m_width / 2, 40);
                        }
                    } else if (mx >= saveBtnX && mx < saveBtnX + 100 && my >= btnY2 && my < btnY2 + btnH) {
                        // 保存按钮：打开命名弹窗
                        m_outfitDialogActive = true;
                        m_outfitDialogIsLoad = false;
                        m_outfitShowToken = false;
                        m_outfitNameInput.clear();
                        m_outfitInput.clear();
                        if (!m_searchActive) {
                            ActivateTextInput(m_width / 4, m_height / 3, m_width / 2, 40);
                        }
                    } else if (mx >= resetX && mx < resetX + 100 && my >= btnY2 && my < btnY2 + btnH) {
                        // 重置按钮
                        m_avatar->SetClothes(m_defaultClothes);
                        m_avatar->LoadParts();
                    }
                    break;
                }

                // === 右侧搜索栏点击 ===
                if (mx >= rightPanelX && my < SEARCH_BAR_H) {
                    int searchW = GetCurrentRightPanelW() - 90;
                    int searchBtnX = rightPanelX + GetCurrentRightPanelW() - 72;

                    // 搜索按钮
                    if (mx >= searchBtnX) {
                        // 点击搜索：关闭输入，执行搜索
                        m_searchActive = false;
                        SDL_StopTextInput();
                        m_gridScrollOffset = 0;
                    }
                    // 清除按钮 (X)
                    else if (!m_searchText.empty() && mx >= rightPanelX + searchW - 16) {
                        m_searchText.clear();
                        m_gridScrollOffset = 0;
                    }
                    // 搜索框区域
                    else if (mx < searchBtnX) {
                        if (!m_searchActive) {
                            m_searchActive = true;
                            ActivateTextInput(rightPanelX, 0, GetCurrentRightPanelW(), SEARCH_BAR_H);
                        }
                    }
                    break;
                }

                // === 左下角圆形截图按钮 ===
                {
                    int btnSize = 44;
                    int btnX = LEFT_PANEL_W + 16;
                    int btnY = m_height - btnSize - 16;
                    int cx = btnX + btnSize / 2;
                    int cy = btnY + btnSize / 2;
                    int r = btnSize / 2;
                    int dx = mx - cx;
                    int dy = my - cy;
                    if (dx * dx + dy * dy <= r * r) {
                        SaveScreenshot();
                        break;
                    }
                }

                // === 中间区域人模切换 ===
                if (mx >= LEFT_PANEL_W && mx < rightPanelX && my < 55) {
                    // 顶部人模按钮（毛玻璃整体布局）
                    int btnW = 75, btnPad = 3;
                    int totalW = btnW * 3 + btnPad * 2 + btnPad * 2;
                    int startBtnX = LEFT_PANEL_W + (centerW - totalW) / 2;
                    for (int i = 0; i < 3; i++) {
                        int bx = startBtnX + btnPad + i * (btnW + btnPad);
                        if (mx >= bx && mx < bx + btnW) {
                            m_model = i;
                            m_avatar->SetModel(m_model);
                            ApplyModelDefaults(m_model);
                            m_avatar->SetClothes(m_defaultClothes);
                            m_avatar->LoadParts();
                            break;
                        }
                    }
                    break;
                }

                // === 右侧服装网格点击 ===
                if (mx >= rightPanelX && my >= SEARCH_BAR_H && my < m_height - BOTTOM_BAR_H) {
                    int gridX = mx - rightPanelX - GRID_PADDING;
                    int gridY = my - SEARCH_BAR_H + m_gridScrollOffset - GRID_PADDING;
                    int cellTotalW = GRID_CELL_SIZE + GRID_PADDING;
                    int cellTotalH = GRID_CELL_SIZE + GRID_NAME_H + GRID_PADDING;
                    int col = gridX / cellTotalW;
                    int row = gridY / cellTotalH;

                    if (col >= 0 && col < GRID_COLS) {
                        int idx = row * GRID_COLS + col;

                        if (m_currentCategory == CAT_SUIT) {
                            // 套装模式：一键穿戴/卸下
                            auto suits = GetFilteredSuits();
                            if (idx >= 0 && idx < (int)suits.size()) {
                                WearSuit(suits[idx]);
                            }
                        } else if (m_currentCategory == CAT_CURRENT) {
                            // 当前穿戴模式：点击可卸下单件
                            auto items = GetCurrentWornItems();
                            if (idx >= 0 && idx < (int)items.size()) {
                                m_avatar->RemoveCloth(items[idx].clothID);
                                m_avatar->LoadParts();
                            }
                        } else {
                            // 普通服装模式
                            auto items = GetFilteredItems();
                            if (idx >= 0 && idx < (int)items.size()) {
                                int clothID = items[idx].clothID;
                                if (m_avatar->IsWearing(clothID)) {
                                    m_avatar->RemoveCloth(clothID);
                                } else {
                                    m_avatar->AddCloth(clothID);
                                }
                                m_avatar->LoadParts();
                            }
                        }
                    }
                }
                break;
            }

            case SDL_TEXTINPUT: {
                // 输入法确认输入后，清除组合文本
                m_imeComposition.clear();
                m_imeCursor = 0;
                m_imeSelLen = 0;
                if (m_outfitDialogActive) {
                    if (m_outfitShowToken) {
                        // 口令展示模式：忽略文本输入
                    } else if (m_outfitRenameIdx >= 0) {
                        // 重命名模式：接受任意字符
                        m_outfitRenameInput += event.text.text;
                    } else if (m_outfitDialogIsLoad) {
                        // 加载模式：接受数字和口令字符（#AO开头的口令）
                        m_outfitInput += event.text.text;
                    } else {
                        // 保存模式：接受任意字符作为方案名称
                        m_outfitNameInput += event.text.text;
                    }
                } else if (m_searchActive) {
                    m_searchText += event.text.text;
                    m_gridScrollOffset = 0;
                }
                break;
            }

            case SDL_TEXTEDITING: {
                // 输入法正在组合文本（如拼音输入中）
                m_imeComposition = event.edit.text;
                m_imeCursor = event.edit.start;
                m_imeSelLen = event.edit.length;
                break;
            }

            // ======== 触摸事件处理（Android 触摸滚动与点击区分） ========
            case SDL_FINGERDOWN: {
                // 将归一化坐标转换为逻辑坐标
                float fx = event.tfinger.x * m_width;
                float fy = event.tfinger.y * m_height;
                m_touchActive = true;
                m_touchStartX = fx;
                m_touchStartY = fy;
                m_touchLastY = fy;
                m_touchIsDrag = false;
                m_touchStartTime = SDL_GetTicks();
                m_scrollVelocity = 0.0f;  // 停止惯性滚动

                // 判断触摸在哪个区域
                int rightPanelX = m_width - GetCurrentRightPanelW();
                if (m_outfitDialogActive) {
                    m_touchScrollTarget = 3;  // 弹窗列表
                } else if (fx >= rightPanelX && fy >= SEARCH_BAR_H && fy < m_height - BOTTOM_BAR_H) {
                    m_touchScrollTarget = 1;  // 右侧服装网格
                } else if (fx < LEFT_PANEL_W) {
                    m_touchScrollTarget = 2;  // 左侧分类
                } else {
                    m_touchScrollTarget = 0;  // 中间区域或其他
                }
                break;
            }

            case SDL_FINGERMOTION: {
                if (!m_touchActive) break;
                float fx = event.tfinger.x * m_width;
                float fy = event.tfinger.y * m_height;
                float dy = fy - m_touchLastY;

                // 检查是否超过拖拽阈值
                float totalDx = fx - m_touchStartX;
                float totalDy = fy - m_touchStartY;
                if (!m_touchIsDrag && (totalDx * totalDx + totalDy * totalDy > TOUCH_DRAG_THRESHOLD * TOUCH_DRAG_THRESHOLD)) {
                    m_touchIsDrag = true;
                }

                if (m_touchIsDrag) {
                    if (m_touchScrollTarget == 1) {
                        // 右侧网格滚动
                        m_gridScrollOffset -= (int)dy;
                        int itemCount = 0;
                        if (m_currentCategory == CAT_SUIT) {
                            itemCount = (int)GetFilteredSuits().size();
                        } else if (m_currentCategory == CAT_CURRENT) {
                            itemCount = (int)GetCurrentWornItems().size();
                        } else {
                            itemCount = (int)GetFilteredItems().size();
                        }
                        int rows = (itemCount + GRID_COLS - 1) / GRID_COLS;
                        int totalH = rows * (GRID_CELL_SIZE + GRID_NAME_H + GRID_PADDING);
                        int visibleH = m_height - SEARCH_BAR_H - BOTTOM_BAR_H;
                        int maxScroll = std::max(0, totalH - visibleH + GRID_PADDING);
                        m_gridScrollOffset = std::max(0, std::min(m_gridScrollOffset, maxScroll));
                    } else if (m_touchScrollTarget == 2) {
                        // 左侧分类滚动
                        m_catScrollOffset -= (int)dy;
                        int totalH = CAT_COUNT * CAT_ITEM_H;
                        int maxScroll = std::max(0, totalH - m_height);
                        m_catScrollOffset = std::max(0, std::min(m_catScrollOffset, maxScroll));
                    } else if (m_touchScrollTarget == 3) {
                        // 弹窗方案列表滚动
                        m_outfitListScroll -= (int)dy;
                        if (m_outfitListScroll < 0) m_outfitListScroll = 0;
                    }
                    // 记录速度用于惯性
                    m_scrollVelocity = -dy;
                }
                m_touchLastY = fy;
                break;
            }

            case SDL_FINGERUP: {
                if (!m_touchActive) break;
                float fx = event.tfinger.x * m_width;
                float fy = event.tfinger.y * m_height;
                m_touchActive = false;

                if (!m_touchIsDrag) {
                    // 未发生拖拽 → 视为点击，模拟 MOUSEBUTTONDOWN 处理
                    int mx = (int)fx;
                    int my = (int)fy;
                    int rightPanelX = m_width - GetCurrentRightPanelW();
                    int centerW = rightPanelX - LEFT_PANEL_W;

                    // === 弹窗点击由 MOUSEBUTTONDOWN 统一处理（不在此重复） ===
                    if (m_outfitDialogActive) break;

                    // === 左侧分类点击 ===
                    if (mx < LEFT_PANEL_W) {
                        if (m_searchActive) { m_searchActive = false; SDL_StopTextInput(); }
                        int displayIdx = (my + m_catScrollOffset) / CAT_ITEM_H;
                        const int* displayOrder = GetCategoryDisplayOrder();
                        if (displayIdx >= 0 && displayIdx < CAT_COUNT) {
                            int catIdx = displayOrder[displayIdx];
                            if (catIdx != m_currentCategory) {
                                m_currentCategory = catIdx;
                                m_gridScrollOffset = 0;
                            }
                        }
                        break;
                    }

                    // === 搜索栏点击 ===
                    if (mx >= rightPanelX && my < SEARCH_BAR_H) {
                        m_searchActive = true;
                        ActivateTextInput(rightPanelX, 0, GetCurrentRightPanelW(), SEARCH_BAR_H);
                        break;
                    }

                    // === 底部栏按钮点击 ===
                    if (mx >= rightPanelX && my >= m_height - BOTTOM_BAR_H) {
                        int btnH = 36;
                        int btnY2 = m_height - BOTTOM_BAR_H + (BOTTOM_BAR_H - btnH) / 2;
                        int loadBtnX = rightPanelX + GetCurrentRightPanelW() - 120;
                        int saveBtnX = loadBtnX - 110;
                        int resetX = saveBtnX - 110;

                        if (mx >= loadBtnX && mx < loadBtnX + 100 && my >= btnY2 && my < btnY2 + btnH) {
                            m_outfitDialogActive = true;
                            m_outfitDialogIsLoad = true;
                            m_outfitInput.clear();
                            m_outfitSelectedIdx = -1;
                            m_outfitListScroll = 0;
                            m_outfitRenameIdx = -1;
                            m_outfitRenameInput.clear();
                            m_outfitMenuIdx = -1;
                            LoadOutfitList();
                            if (!m_searchActive) { ActivateTextInput(m_width / 4, m_height / 3, m_width / 2, 40); }
                        } else if (mx >= saveBtnX && mx < saveBtnX + 100 && my >= btnY2 && my < btnY2 + btnH) {
                            m_outfitDialogActive = true;
                            m_outfitDialogIsLoad = false;
                            m_outfitShowToken = false;
                            m_outfitNameInput.clear();
                            m_outfitInput.clear();
                            if (!m_searchActive) { ActivateTextInput(m_width / 4, m_height / 3, m_width / 2, 40); }
                        } else if (mx >= resetX && mx < resetX + 100 && my >= btnY2 && my < btnY2 + btnH) {
                            m_avatar->SetClothes(m_defaultClothes);
                            m_avatar->LoadParts();
                        }
                        break;
                    }

                    // === 中间区域人模切换 ===
                    if (mx >= LEFT_PANEL_W && mx < rightPanelX && my < 55) {
                        int btnW = 75, btnPad = 3;
                        int totalW = btnW * 3 + btnPad * 2 + btnPad * 2;
                        int startBtnX = LEFT_PANEL_W + (centerW - totalW) / 2;
                        for (int i = 0; i < 3; i++) {
                            int bx = startBtnX + btnPad + i * (btnW + btnPad);
                            if (mx >= bx && mx < bx + btnW) {
                                m_model = i;
                                m_avatar->SetModel(m_model);
                                ApplyModelDefaults(m_model);
                                m_avatar->SetClothes(m_defaultClothes);
                                m_avatar->LoadParts();
                                break;
                            }
                        }
                        break;
                    }

                    // === 中间区域截图按钮 ===
                    if (mx >= LEFT_PANEL_W && mx < rightPanelX) {
                        int btnSize = 42;
                        int btnX = rightPanelX - btnSize - 16;
                        int btnY = m_height - BOTTOM_BAR_H - btnSize - 16;
                        int cx = btnX + btnSize / 2;
                        int cy = btnY + btnSize / 2;
                        int r = btnSize / 2;
                        int dx = mx - cx;
                        int ddy = my - cy;
                        if (dx * dx + ddy * ddy <= r * r) {
                            SaveScreenshot();
                            break;
                        }
                    }

                    // === 右侧面板展开/收起按钮 ===
                    {
                        int curRightW = GetCurrentRightPanelW();
                        int togglePanelX = m_width - curRightW;
                        int toggleBtnW = 24, toggleBtnH = 60;
                        int toggleBtnX = togglePanelX - toggleBtnW;
                        int toggleBtnY = m_height / 2 - toggleBtnH / 2;
                        if (mx >= toggleBtnX && mx < toggleBtnX + toggleBtnW &&
                            my >= toggleBtnY && my < toggleBtnY + toggleBtnH) {
                            m_rightPanelCollapsed = !m_rightPanelCollapsed;
                            break;
                        }
                    }

                    // === 右侧服装网格点击 ===
                    if (mx >= rightPanelX && my >= SEARCH_BAR_H && my < m_height - BOTTOM_BAR_H) {
                        int gridX = mx - rightPanelX - GRID_PADDING;
                        int gridY = my - SEARCH_BAR_H + m_gridScrollOffset - GRID_PADDING;
                        int cellTotalW = GRID_CELL_SIZE + GRID_PADDING;
                        int cellTotalH = GRID_CELL_SIZE + GRID_NAME_H + GRID_PADDING;
                        int col = gridX / cellTotalW;
                        int row = gridY / cellTotalH;

                        if (col >= 0 && col < GRID_COLS) {
                            int idx = row * GRID_COLS + col;

                            if (m_currentCategory == CAT_SUIT) {
                                auto suits = GetFilteredSuits();
                                if (idx >= 0 && idx < (int)suits.size()) {
                                    WearSuit(suits[idx]);
                                }
                            } else if (m_currentCategory == CAT_CURRENT) {
                                // 当前穿戴模式：点击可卸下单件
                                auto items = GetCurrentWornItems();
                                if (idx >= 0 && idx < (int)items.size()) {
                                    m_avatar->RemoveCloth(items[idx].clothID);
                                    m_avatar->LoadParts();
                                }
                            } else {
                                auto items = GetFilteredItems();
                                if (idx >= 0 && idx < (int)items.size()) {
                                    int clothID = items[idx].clothID;
                                    if (m_avatar->IsWearing(clothID)) {
                                        m_avatar->RemoveCloth(clothID);
                                    } else {
                                        m_avatar->AddCloth(clothID);
                                    }
                                    m_avatar->LoadParts();
                                }
                            }
                        }
                    }
                } else {
                    // 拖拽结束，启动惯性滚动（如果速度足够快）
                    if (std::fabs(m_scrollVelocity) < 1.0f) {
                        m_scrollVelocity = 0.0f;
                    }
                }
                break;
            }

            case SDL_KEYDOWN: {
                if (m_outfitDialogActive) {
                    if (m_outfitRenameIdx >= 0) {
                        // 重命名模式下的键盘处理
                        if (event.key.keysym.sym == SDLK_RETURN) {
                            // 确认重命名
                            if (m_outfitRenameIdx < (int)m_outfitEntries.size()) {
                                RenameOutfit(m_outfitEntries[m_outfitRenameIdx].code, m_outfitRenameInput);
                                m_outfitEntries[m_outfitRenameIdx].name = m_outfitRenameInput;
                                m_outfitMsg = "重命名成功！";
                                m_outfitMsgTime = SDL_GetTicks();
                            }
                            m_outfitRenameIdx = -1;
                            m_outfitRenameInput.clear();
                        } else if (event.key.keysym.sym == SDLK_ESCAPE) {
                            // 取消重命名
                            m_outfitRenameIdx = -1;
                            m_outfitRenameInput.clear();
                        } else if (event.key.keysym.sym == SDLK_BACKSPACE) {
                            // UTF-8安全删除
                            if (!m_outfitRenameInput.empty()) {
                                while (!m_outfitRenameInput.empty() && (m_outfitRenameInput.back() & 0xC0) == 0x80) {
                                    m_outfitRenameInput.pop_back();
                                }
                                if (!m_outfitRenameInput.empty()) m_outfitRenameInput.pop_back();
                            }
                        } else if (event.key.keysym.sym == SDLK_v
                                   && ((event.key.keysym.mod & KMOD_CTRL) || (event.key.keysym.mod & KMOD_GUI))) {
                            // Ctrl+V / Cmd+V 粘贴
                            if (SDL_HasClipboardText()) {
                                char* clipText = SDL_GetClipboardText();
                                if (clipText) {
                                    m_outfitRenameInput += clipText;
                                    SDL_free(clipText);
                                }
                            }
                        }
                        break;
                    }
                    if (event.key.keysym.sym == SDLK_RETURN) {
                        if (m_outfitShowToken) {
                            // 口令展示模式：回车关闭弹窗
                            m_outfitDialogActive = false;
                            m_outfitShowToken = false;
                            if (!m_searchActive) SDL_StopTextInput();
                        } else if (m_outfitDialogIsLoad) {
                            // 加载模式：回车确认加载
                            std::string codeToLoad;
                            if (!m_outfitInput.empty()) {
                                // 检查是否为口令格式
                                if (m_outfitInput.substr(0, 3) == "#AO") {
                                    if (LoadOutfitFromToken(m_outfitInput)) {
                                        m_outfitMsg = "口令加载成功！";
                                    } else {
                                        m_outfitMsg = "口令无效，无法加载";
                                    }
                                    m_outfitMsgTime = SDL_GetTicks();
                                    m_outfitDialogActive = false;
                                    if (!m_searchActive) SDL_StopTextInput();
                                } else {
                                    codeToLoad = m_outfitInput;
                                }
                            } else if (m_outfitSelectedIdx >= 0 && m_outfitSelectedIdx < (int)m_outfitEntries.size()) {
                                codeToLoad = m_outfitEntries[m_outfitSelectedIdx].code;
                            }
                            if (!codeToLoad.empty()) {
                                if (LoadOutfit(codeToLoad)) {
                                    m_outfitMsg = "加载成功！编号: " + codeToLoad;
                                } else {
                                    m_outfitMsg = "编号 " + codeToLoad + " 不存在";
                                }
                                m_outfitMsgTime = SDL_GetTicks();
                                m_outfitDialogActive = false;
                                if (!m_searchActive) SDL_StopTextInput();
                            }
                        } else {
                            // 保存模式：回车确认保存，进入口令展示模式
                            std::string code = SaveOutfit(m_outfitNameInput);
                            m_outfitSavedCode = code;
                            m_outfitSavedToken = EncodeOutfitToken(m_model, m_avatar->GetClothes());
                            m_outfitShowToken = true;
                            m_outfitMsg = "保存成功！编号: " + code;
                            m_outfitMsgTime = SDL_GetTicks();
                        }
                    } else if (event.key.keysym.sym == SDLK_ESCAPE) {
                        m_outfitDialogActive = false;
                        m_outfitShowToken = false;
                        if (!m_searchActive) SDL_StopTextInput();
                    } else if (event.key.keysym.sym == SDLK_BACKSPACE) {
                        if (m_outfitShowToken) {
                            // 口令展示模式：忽略退格
                        } else if (m_outfitDialogIsLoad) {
                            // 加载模式：UTF-8安全删除
                            if (!m_outfitInput.empty()) {
                                while (!m_outfitInput.empty() && (m_outfitInput.back() & 0xC0) == 0x80) {
                                    m_outfitInput.pop_back();
                                }
                                if (!m_outfitInput.empty()) m_outfitInput.pop_back();
                            }
                        } else {
                            // 保存模式：UTF-8安全删除
                            if (!m_outfitNameInput.empty()) {
                                while (!m_outfitNameInput.empty() && (m_outfitNameInput.back() & 0xC0) == 0x80) {
                                    m_outfitNameInput.pop_back();
                                }
                                if (!m_outfitNameInput.empty()) m_outfitNameInput.pop_back();
                            }
                        }
                    } else if (event.key.keysym.sym == SDLK_v
                               && ((event.key.keysym.mod & KMOD_CTRL) || (event.key.keysym.mod & KMOD_GUI))) {
                        // Ctrl+V / Cmd+V 粘贴
                        if (SDL_HasClipboardText()) {
                            char* clipText = SDL_GetClipboardText();
                            if (clipText) {
                                if (m_outfitDialogIsLoad) {
                                    m_outfitInput = clipText;
                                } else if (!m_outfitShowToken) {
                                    m_outfitNameInput += clipText;
                                }
                                SDL_free(clipText);
                            }
                        }
                    }
                    break;
                }
                if (m_searchActive) {
                    if (event.key.keysym.sym == SDLK_BACKSPACE && !m_searchText.empty()) {
                        // UTF-8 安全删除最后一个字符
                        while (!m_searchText.empty() && (m_searchText.back() & 0xC0) == 0x80) {
                            m_searchText.pop_back();
                        }
                        if (!m_searchText.empty()) m_searchText.pop_back();
                        m_gridScrollOffset = 0;
                    } else if (event.key.keysym.sym == SDLK_ESCAPE) {
                        m_searchActive = false;
                        SDL_StopTextInput();
                    } else if (event.key.keysym.sym == SDLK_RETURN) {
                        m_searchActive = false;
                        SDL_StopTextInput();
                    } else if (event.key.keysym.sym == SDLK_v
                               && ((event.key.keysym.mod & KMOD_CTRL) || (event.key.keysym.mod & KMOD_GUI))) {
                        // Ctrl+V / Cmd+V 粘贴
                        if (SDL_HasClipboardText()) {
                            char* clipText = SDL_GetClipboardText();
                            if (clipText) {
                                m_searchText += clipText;
                                m_gridScrollOffset = 0;
                                SDL_free(clipText);
                            }
                        }
                    }
                } else {
                    switch (event.key.keysym.sym) {
                        case SDLK_ESCAPE: m_running = false; break;
                        case SDLK_r:
                            m_avatar->SetClothes(m_defaultClothes);
                            m_avatar->LoadParts();
                            break;
                        case SDLK_f:
                            m_searchActive = true;
                            ActivateTextInput(m_width - GetCurrentRightPanelW(), 0, GetCurrentRightPanelW(), SEARCH_BAR_H);
                            break;
                        case SDLK_TAB:
                            m_rightPanelCollapsed = !m_rightPanelCollapsed;
                            break;
                    }
                }
                break;
            }
        }
    }
}

void Game::Update() {
    // 在主线程中处理待创建的缩略图纹理
    {
        std::lock_guard<std::mutex> lock(m_thumbPendingMutex);
        int processed = 0;
        while (!m_thumbPending.empty() && processed < 5) { // 每帧最多处理 5 个
            auto item = std::move(m_thumbPending.front());
            m_thumbPending.pop();
            processed++;

            SDL_Texture* tex = nullptr;
            if (!item.data.empty()) {
                SDL_RWops* rw = SDL_RWFromMem(item.data.data(), (int)item.data.size());
                if (rw) {
                    SDL_Surface* surface = IMG_Load_RW(rw, 1);
                    if (surface) {
                        tex = SDL_CreateTextureFromSurface(m_renderer, surface);
                        SDL_FreeSurface(surface);
                        if (tex) {
                            SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
                        }
                    }
                }
            }

            std::lock_guard<std::mutex> thumbLock(m_thumbMutex);
            m_thumbCache[item.clothID] = tex;
            m_thumbLoading.erase(item.clothID);
        }
    }

    // 处理角色服装的异步加载结果
    if (m_avatar) {
        m_avatar->Update(m_renderer);
    }

    // 右侧面板收起/展开动画
    {
        float target = m_rightPanelCollapsed ? 1.0f : 0.0f;
        float diff = target - m_rightPanelAnimProgress;
        if (std::fabs(diff) > 0.001f) {
            m_rightPanelAnimProgress += diff * 0.12f; // 缓动系数
        } else {
            m_rightPanelAnimProgress = target;
        }
    }

    // 触摸惯性滚动（松手后继续滑动一段距离）
    if (!m_touchActive && std::fabs(m_scrollVelocity) > 0.5f) {
        if (m_touchScrollTarget == 1) {
            m_gridScrollOffset += (int)m_scrollVelocity;
            int itemCount = 0;
            if (m_currentCategory == CAT_SUIT) {
                itemCount = (int)GetFilteredSuits().size();
            } else if (m_currentCategory == CAT_CURRENT) {
                itemCount = (int)GetCurrentWornItems().size();
            } else {
                itemCount = (int)GetFilteredItems().size();
            }
            int rows = (itemCount + GRID_COLS - 1) / GRID_COLS;
            int totalH = rows * (GRID_CELL_SIZE + GRID_NAME_H + GRID_PADDING);
            int visibleH = m_height - SEARCH_BAR_H - BOTTOM_BAR_H;
            int maxScroll = std::max(0, totalH - visibleH + GRID_PADDING);
            m_gridScrollOffset = std::max(0, std::min(m_gridScrollOffset, maxScroll));
        } else if (m_touchScrollTarget == 2) {
            m_catScrollOffset += (int)m_scrollVelocity;
            int totalH = CAT_COUNT * CAT_ITEM_H;
            int maxScroll = std::max(0, totalH - m_height);
            m_catScrollOffset = std::max(0, std::min(m_catScrollOffset, maxScroll));
        } else if (m_touchScrollTarget == 3) {
            m_outfitListScroll += (int)m_scrollVelocity;
            if (m_outfitListScroll < 0) m_outfitListScroll = 0;
        }
        m_scrollVelocity *= 0.92f;  // 衰减
        if (std::fabs(m_scrollVelocity) < 0.5f) {
            m_scrollVelocity = 0.0f;
        }
    }
}

// ==================== 主渲染 ====================
void Game::Render() {
    // 浅粉底色
    SDL_SetRenderDrawColor(m_renderer, 248, 235, 240, 255);
    SDL_RenderClear(m_renderer);

    DrawCenterPanel();
    DrawRightPanel();
    DrawBottomBar();
    DrawLeftPanel();
    DrawCollapseToggle();  // 收起/展开切换按钮

    // 绘制弹窗和提示信息
    DrawOutfitDialog();

    SDL_RenderPresent(m_renderer);
}

// ==================== 左侧分类栏 ====================
void Game::DrawLeftPanel() {
    // 背景（浅灰白色）
    SDL_Rect bg = {0, 0, LEFT_PANEL_W, m_height};
    FillRect(bg, C_LEFT_BG);

    // 右侧边线
    SDL_SetRenderDrawColor(m_renderer, C_DIVIDER.r, C_DIVIDER.g, C_DIVIDER.b, 255);
    SDL_RenderDrawLine(m_renderer, LEFT_PANEL_W - 1, 0, LEFT_PANEL_W - 1, m_height);

    // 裁剪
    SDL_RenderSetClipRect(m_renderer, &bg);

    int rawMx, rawMy;
    SDL_GetMouseState(&rawMx, &rawMy);
    int mx, my;
    WindowToLogical(rawMx, rawMy, mx, my);

    const int* displayOrder = GetCategoryDisplayOrder();
    for (int idx = 0; idx < CAT_COUNT; idx++) {
        int i = displayOrder[idx]; // 实际分类ID
        int itemY = idx * CAT_ITEM_H - m_catScrollOffset;
        if (itemY + CAT_ITEM_H < 0 || itemY > m_height) continue;

        SDL_Rect itemRect = {0, itemY, LEFT_PANEL_W, CAT_ITEM_H};
        bool active = (i == m_currentCategory);
        bool hovered = (mx >= 0 && mx < LEFT_PANEL_W && my >= itemY && my < itemY + CAT_ITEM_H);

        // 背景：选中项为白色，悬停为极浅灰
        if (active) {
            FillRect(itemRect, C_LEFT_ITEM_ACTIVE);
            // 左侧蓝色指示条
            SDL_Rect accentBar = {0, itemY, ACCENT_BAR_W, CAT_ITEM_H};
            FillRect(accentBar, C_LEFT_ACCENT);
        } else if (hovered) {
            FillRect(itemRect, C_LEFT_ITEM_HOVER);
        }

        // 底部分割线
        SDL_SetRenderDrawColor(m_renderer, C_DIVIDER.r, C_DIVIDER.g, C_DIVIDER.b, 100);
        SDL_RenderDrawLine(m_renderer, 12, itemY + CAT_ITEM_H - 1,
                          LEFT_PANEL_W - 12, itemY + CAT_ITEM_H - 1);

        // 分类名（大字，深色）
        CategoryInfo info = GetCategoryInfo(i);
        SDL_Color textColor = active ? C_LEFT_TEXT_ACTIVE : C_LEFT_TEXT;
        DrawText(info.name, ACCENT_BAR_W + 12, itemY + 12, m_fontLarge, textColor);

        // 数量（在分类名右侧，同一行）
        int count = 0;
        if (i == CAT_SUIT) {
            count = (int)m_suitItems.size();
        } else if (i == CAT_CURRENT) {
            count = (int)GetCurrentWornItems().size();
        } else {
            auto it = m_categoryItems.find(i);
            count = (it != m_categoryItems.end()) ? (int)it->second.size() : 0;
        }
        std::string countStr = std::to_string(count);
        DrawText(countStr, LEFT_PANEL_W - 42, itemY + 14, m_font, C_LEFT_PINYIN);

        // 拼音（小字，灰色）
        DrawText(info.pinyin, ACCENT_BAR_W + 12, itemY + 40, m_fontTiny, C_LEFT_PINYIN);
    }

    SDL_RenderSetClipRect(m_renderer, nullptr);
}

// ==================== 中间角色展示 ====================
void Game::DrawCenterPanel() {
    int centerX = LEFT_PANEL_W;
    int curRightW = GetCurrentRightPanelW();
    int centerW = m_width - LEFT_PANEL_W - curRightW;

    // 扁平纯色背景
    SDL_Rect centerBg = {centerX, 0, centerW, m_height};
    FillRect(centerBg, C_CENTER_BG);

    // 先渲染角色（底层）— 居中于当前中间区域
    int avatarCenterX = centerX + centerW / 2;
    int avatarBottomY = m_height - 100;
    m_avatar->Render(m_renderer, avatarCenterX, avatarBottomY);

    // 人模切换按钮（毛玻璃整体 + 渐变动画）
    const char* modelNames[] = {"旧人模", "新男模", "新女模"};
    int btnW = 75, btnH = 32, btnPad = 3;
    int totalW = btnW * 3 + btnPad * 2 + btnPad * 2; // 三个按钮 + 内边距
    int startBtnX = centerX + (centerW - totalW) / 2;
    int btnY = 10;

    // 毛玻璃整体背景（圆角矩形，半透明粉色）
    SDL_Rect glassRect = {startBtnX, btnY, totalW, btnH + btnPad * 2};
    DrawRoundedRect(glassRect, (SDL_Color){235, 200, 215, 200}, 12);

    // 计算选中滑块的目标位置和动画插值
    float targetX = (float)(startBtnX + btnPad + m_model * (btnW + btnPad));
    m_modelSwitchTargetX = targetX;
    // 首次初始化（避免启动时从0滑过来）
    if (m_modelSwitchAnimX < (float)startBtnX) m_modelSwitchAnimX = targetX;
    // 平滑插值动画
    float diff = m_modelSwitchTargetX - m_modelSwitchAnimX;
    if (std::fabs(diff) > 0.5f) {
        m_modelSwitchAnimX += diff * 0.15f; // 缓动系数
    } else {
        m_modelSwitchAnimX = m_modelSwitchTargetX;
    }

    // 绘制选中滑块（深色，带圆角）
    SDL_Rect sliderRect = {(int)m_modelSwitchAnimX, btnY + btnPad, btnW, btnH};
    DrawRoundedRect(sliderRect, C_BTN_MODEL_ACTIVE, 10);

    // 绘制三个按钮文字
    for (int i = 0; i < 3; i++) {
        SDL_Rect textRect = {startBtnX + btnPad + i * (btnW + btnPad), btnY + btnPad, btnW, btnH};
        bool active = (m_model == i);
        // 根据滑块位置计算文字颜色渐变
        float sliderCenter = m_modelSwitchAnimX + btnW / 2.0f;
        float textCenter = textRect.x + btnW / 2.0f;
        float dist = std::fabs(sliderCenter - textCenter);
        float t = 1.0f - std::min(dist / (float)(btnW + btnPad), 1.0f); // 0~1, 越近越大
        t = t * t; // 二次缓动使过渡更平滑
        // 插值颜色：粉灰(120,85,100) -> 白(255,255,255)
        Uint8 cr = (Uint8)(120 + t * 135);
        Uint8 cg = (Uint8)(85 + t * 170);
        Uint8 cb = (Uint8)(100 + t * 155);
        DrawTextCentered(modelNames[i], textRect, m_fontSmall, (SDL_Color){cr, cg, cb, 255});
    }

    // 加载中指示器 - 转菊花
    if (m_avatar->IsLoading()) {
        Uint32 ticks = SDL_GetTicks();
        const int numSpokes = 12;           // 菊花辐条数
        const int innerR = 8;               // 内半径
        const int outerR = 18;              // 外半径
        const int spokeWidth = 3;           // 辐条粗细
        int cx = centerX + centerW / 2;     // 菊花中心X
        int cy = m_height / 2;              // 菊花中心Y

        // 绘制半透明底色背景
        int bgPadding = 16;
        int bgW = (outerR + bgPadding) * 2;
        int bgH = (outerR + bgPadding) * 2 + 28;  // 额外空间给文字
        SDL_Rect bgRect = {cx - bgW / 2, cy - (outerR + bgPadding), bgW, bgH};
        DrawRoundedRect(bgRect, (SDL_Color){60, 30, 45, 180}, 12);
        DrawRoundedRectOutline(bgRect, (SDL_Color){140, 60, 85, 100}, 12, 1);

        // 当前旋转步进（每80ms转一格）
        int step = (ticks / 80) % numSpokes;

        for (int i = 0; i < numSpokes; i++) {
            // 计算每条辐条的透明度（距离当前step越远越淡）
            int dist = (i - step + numSpokes) % numSpokes;
            int alpha = 255 - dist * (200 / numSpokes);
            if (alpha < 55) alpha = 55;

            double angle = i * (2.0 * M_PI / numSpokes) - M_PI / 2.0;
            double cosA = cos(angle);
            double sinA = sin(angle);

            int x1 = cx + (int)(innerR * cosA);
            int y1 = cy + (int)(innerR * sinA);
            int x2 = cx + (int)(outerR * cosA);
            int y2 = cy + (int)(outerR * sinA);

            SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(m_renderer, 230, 130, 160, (Uint8)alpha);

            // 绘制有粗细的辐条（多条平行线模拟粗线）
            for (int w = -(spokeWidth / 2); w <= spokeWidth / 2; w++) {
                int offX = (int)(w * sinA);  // 垂直于辐条方向的偏移
                int offY = (int)(-w * cosA);
                SDL_RenderDrawLine(m_renderer, x1 + offX, y1 + offY, x2 + offX, y2 + offY);
            }
        }

        // 菊花下方显示 "加载中" 文字
        DrawTextCentered("加载中",
            {cx - 30, cy + outerR + 8, 60, 20},
            m_fontSmall, (SDL_Color){230, 170, 190, 255});
    }

    // ---- 左下角圆形截图按钮 ----
    {
        int btnSize = 44;
        int btnX = centerX + 16;
        int btnY = m_height - btnSize - 16;
        int cx = btnX + btnSize / 2;
        int cy = btnY + btnSize / 2;
        int r = btnSize / 2;

        // 辅助lambda：Bresenham中点圆算法画圆轮廓（无毛刺）
        auto drawCircleBresenham = [&](int ccx, int ccy, int cr) {
            int x = cr, y = 0, d = 1 - cr;
            while (x >= y) {
                SDL_RenderDrawPoint(m_renderer, ccx + x, ccy + y);
                SDL_RenderDrawPoint(m_renderer, ccx - x, ccy + y);
                SDL_RenderDrawPoint(m_renderer, ccx + x, ccy - y);
                SDL_RenderDrawPoint(m_renderer, ccx - x, ccy - y);
                SDL_RenderDrawPoint(m_renderer, ccx + y, ccy + x);
                SDL_RenderDrawPoint(m_renderer, ccx - y, ccy + x);
                SDL_RenderDrawPoint(m_renderer, ccx + y, ccy - x);
                SDL_RenderDrawPoint(m_renderer, ccx - y, ccy - x);
                y++;
                if (d <= 0) {
                    d += 2 * y + 1;
                } else {
                    x--;
                    d += 2 * (y - x) + 1;
                }
            }
        };

        // 辅助lambda：Bresenham中点圆算法画填充圆（无毛刺）
        auto drawFilledCircleBresenham = [&](int ccx, int ccy, int cr) {
            int x = cr, y = 0, d = 1 - cr;
            while (x >= y) {
                SDL_RenderDrawLine(m_renderer, ccx - x, ccy + y, ccx + x, ccy + y);
                SDL_RenderDrawLine(m_renderer, ccx - x, ccy - y, ccx + x, ccy - y);
                SDL_RenderDrawLine(m_renderer, ccx - y, ccy + x, ccx + y, ccy + x);
                SDL_RenderDrawLine(m_renderer, ccx - y, ccy - x, ccx + y, ccy - x);
                y++;
                if (d <= 0) {
                    d += 2 * y + 1;
                } else {
                    x--;
                    d += 2 * (y - x) + 1;
                }
            }
        };

        // 绘制圆形按钮背景（填充圆）
        SDL_SetRenderDrawColor(m_renderer, C_BTN_SCREENSHOT.r, C_BTN_SCREENSHOT.g, C_BTN_SCREENSHOT.b, 220);
        drawFilledCircleBresenham(cx, cy, r);

        // 圆形边框（Bresenham无毛刺）
        SDL_SetRenderDrawColor(m_renderer, 255, 255, 255, 80);
        drawCircleBresenham(cx, cy, r);

        // 绘制相机图标（居中）
        {
            int iconSize = 20;
            int ix = cx - iconSize / 2;
            int iy = cy - iconSize / 2;
            int is = iconSize;
            SDL_SetRenderDrawColor(m_renderer, 255, 255, 255, 255);
            // 相机机身
            SDL_Rect body = {ix + 1, iy + is/4, is - 2, is * 2/3};
            SDL_RenderDrawRect(m_renderer, &body);
            // 相机机身填充（半透明）
            SDL_SetRenderDrawColor(m_renderer, 255, 255, 255, 40);
            for (int fy = iy + is/4 + 1; fy < iy + is/4 + is * 2/3; fy++) {
                SDL_RenderDrawLine(m_renderer, ix + 2, fy, ix + is - 3, fy);
            }
            SDL_SetRenderDrawColor(m_renderer, 255, 255, 255, 255);
            // 取景器凸起
            SDL_RenderDrawLine(m_renderer, ix + is/3, iy + is/4, ix + is/3, iy + is/6);
            SDL_RenderDrawLine(m_renderer, ix + is/3, iy + is/6, ix + is*2/3, iy + is/6);
            SDL_RenderDrawLine(m_renderer, ix + is*2/3, iy + is/6, ix + is*2/3, iy + is/4);
            // 镜头圆（Bresenham无毛刺）
            int lensR = is / 5;
            int lcx = ix + is / 2, lcy = iy + is/4 + is/3;
            drawCircleBresenham(lcx, lcy, lensR);
            // 镜头中心点
            SDL_RenderDrawPoint(m_renderer, lcx, lcy);
        }
    }

    // 右侧边线
    int rightPanelX = m_width - curRightW;
    SDL_SetRenderDrawColor(m_renderer, C_DIVIDER.r, C_DIVIDER.g, C_DIVIDER.b, 255);
    SDL_RenderDrawLine(m_renderer, rightPanelX, 0, rightPanelX, m_height);
}

// ==================== 右侧服装面板 ====================
int Game::GetCurrentRightPanelW() const {
    return (int)(RIGHT_PANEL_W * (1.0f - m_rightPanelAnimProgress));
}

void Game::DrawCollapseToggle() {
    int curRightW = GetCurrentRightPanelW();
    int panelX = m_width - curRightW;

    // 切换按钮：半圆形标签，紧贴右侧面板左边缘
    int btnW = 24, btnH = 60;
    int btnX = panelX - btnW;
    int btnY = m_height / 2 - btnH / 2;

    // 获取鼠标位置判断悬停
    int rawMx, rawMy;
    SDL_GetMouseState(&rawMx, &rawMy);
    int mx, my;
    WindowToLogical(rawMx, rawMy, mx, my);
    bool hovered = (mx >= btnX && mx < btnX + btnW && my >= btnY && my < btnY + btnH);

    // 按钮背景（带圆角的半圆形标签）
    SDL_Color btnColor = hovered
        ? (SDL_Color){210, 150, 175, 230}
        : (SDL_Color){230, 190, 210, 200};
    SDL_Rect btnRect = {btnX, btnY, btnW, btnH};
    DrawRoundedRect(btnRect, btnColor, 8);

    // 箭头指示（收起时显示 <，展开时显示 >）
    const char* arrow = m_rightPanelCollapsed ? "<" : ">";
    DrawTextCentered(arrow, btnRect, m_font, (SDL_Color){255, 255, 255, 255});
}

void Game::DrawRightPanel() {
    int curRightW = GetCurrentRightPanelW();
    if (curRightW <= 0) return; // 完全收起时不绘制

    int panelX = m_width - curRightW;

    // 设置裁剪区域，防止内容溢出
    SDL_Rect clipRect = {panelX, 0, curRightW, m_height};
    SDL_RenderSetClipRect(m_renderer, &clipRect);

    // 背景（总是以完整宽度绘制，靠右对齐）
    SDL_Rect bg = {panelX, 0, curRightW, m_height};
    FillRect(bg, C_RIGHT_BG);

    DrawSearchBar();
    DrawClothGrid();

    SDL_RenderSetClipRect(m_renderer, nullptr); // 取消裁剪
}

void Game::DrawSearchBar() {
    int curRightW = GetCurrentRightPanelW();
    int panelX = m_width - curRightW;
    int barY = 8;
    int barH = SEARCH_BAR_H - 16;
    int searchW = curRightW - 90;

    // 搜索框背景
    SDL_Rect searchRect = {panelX + 12, barY, searchW, barH};
    FillRect(searchRect, C_SEARCH_BG);
    SDL_SetRenderDrawColor(m_renderer, C_SEARCH_BORDER.r, C_SEARCH_BORDER.g, C_SEARCH_BORDER.b, 255);
    SDL_RenderDrawRect(m_renderer, &searchRect);

    // 搜索框文字
    if (m_searchText.empty() && m_imeComposition.empty()) {
        const char* placeholder = (m_currentCategory == CAT_SUIT) ? "输入套装名称" : "输入服装名称";
        DrawText(placeholder, searchRect.x + 10, searchRect.y + 8, m_font, C_SEARCH_TEXT);
    } else {
        DrawText(m_searchText, searchRect.x + 10, searchRect.y + 8, m_font, (SDL_Color){120, 50, 75, 255});
    }

    // 搜索框光标和IME组合文本
    if (m_searchActive) {
        // 设置输入法候选窗口位置
        SDL_Rect textInputRect = searchRect;
        SDL_SetTextInputRect(&textInputRect);

        int cursorX = searchRect.x + 10;
        if (!m_searchText.empty()) {
            int tw, th;
            TTF_SizeUTF8(m_font, m_searchText.c_str(), &tw, &th);
            cursorX += (int)(tw / m_dpiScale + 0.5f);
        }

        // 绘制IME组合文本（如拼音）
        if (!m_imeComposition.empty()) {
            // 组合文本用半透明背景高亮显示
            int compW, compH;
            TTF_SizeUTF8(m_font, m_imeComposition.c_str(), &compW, &compH);
            int compDisplayW = (int)(compW / m_dpiScale + 0.5f);
            SDL_Rect compBg = {cursorX, searchRect.y + 4, compDisplayW + 4, barH - 8};
            FillRect(compBg, (SDL_Color){255, 230, 240, 200});
            DrawText(m_imeComposition, cursorX + 2, searchRect.y + 8, m_font, (SDL_Color){180, 80, 120, 255});
            // 组合文本后的光标
            cursorX += compDisplayW + 4;
        }

        // 闪烁光标
        Uint32 ticks = SDL_GetTicks();
        if ((ticks / 500) % 2 == 0) {
            SDL_SetRenderDrawColor(m_renderer, 210, 100, 130, 200);
            SDL_RenderDrawLine(m_renderer, cursorX, searchRect.y + 6,
                             cursorX, searchRect.y + barH - 6);
        }
    }

    // X 关闭按钮
    if (!m_searchText.empty()) {
        SDL_Rect closeRect = {searchRect.x + searchW - 28, barY + 4, 24, 24};
        DrawTextCentered("X", closeRect, m_font, C_GRAY);
    }

    // 搜索按钮（蓝色）
    SDL_Rect searchBtn = {panelX + curRightW - 72, barY, 60, barH};
    FillRect(searchBtn, C_LEFT_ACCENT);
    DrawTextCentered("搜 索", searchBtn, m_font, C_WHITE);
}

void Game::DrawClothGrid() {
    int curRightW = GetCurrentRightPanelW();
    int panelX = m_width - curRightW;
    int gridTop = SEARCH_BAR_H;
    int gridHeight = m_height - SEARCH_BAR_H - BOTTOM_BAR_H;

    // 裁剪区域
    SDL_Rect clipRect = {panelX, gridTop, curRightW, gridHeight};
    SDL_RenderSetClipRect(m_renderer, &clipRect);

    int rawMx, rawMy;
    SDL_GetMouseState(&rawMx, &rawMy);
    int mx, my;
    WindowToLogical(rawMx, rawMy, mx, my);

    // cellTotal 包含卡片高度 + 名称区域 + 间距
    int cellTotalW = GRID_CELL_SIZE + GRID_PADDING;     // 水平方向
    int cellTotalH = GRID_CELL_SIZE + GRID_NAME_H + GRID_PADDING;  // 垂直方向（含名称）
    int itemCount = 0;

    // 角标颜色
    SDL_Color tagBg = {210, 130, 155, 210};        // 角标背景：玫粉色
    SDL_Color nameBg = {210, 130, 155, 210};        // 名称底栏背景：与角标同色
    (void)nameBg;

    if (m_currentCategory == CAT_SUIT) {
        // ========== 套装网格模式 ==========
        auto suits = GetFilteredSuits();
        itemCount = (int)suits.size();

        for (int i = 0; i < itemCount; i++) {
            int row = i / GRID_COLS;
            int col = i % GRID_COLS;

            int cellX = panelX + GRID_PADDING + col * cellTotalW;
            int cellY = gridTop + GRID_PADDING + row * cellTotalH - m_gridScrollOffset;

            // 跳过不可见
            if (cellY + GRID_CELL_SIZE + GRID_NAME_H < gridTop || cellY > gridTop + gridHeight) continue;

            // 整体卡片区域（含名称栏）
            SDL_Rect fullCard = {cellX, cellY, GRID_CELL_SIZE, GRID_CELL_SIZE + GRID_NAME_H};
            SDL_Rect cellRect = {cellX, cellY, GRID_CELL_SIZE, GRID_CELL_SIZE};
            bool hovered = (mx >= fullCard.x && mx < fullCard.x + fullCard.w &&
                           my >= fullCard.y && my < fullCard.y + fullCard.h);

            // 检查当前是否穿着这套套装
            bool selected = true;
            for (int cid : suits[i].clothesList) {
                if (!m_avatar->IsWearing(cid)) { selected = false; break; }
            }

            // 卡片背景（圆角矩形，包含名称区域为整体）
            if (selected) {
                DrawRoundedRect(fullCard, (SDL_Color){240, 205, 220, 255}, CARD_RADIUS);
            } else if (hovered) {
                DrawRoundedRect(fullCard, (SDL_Color){250, 230, 238, 255}, CARD_RADIUS);
            } else {
                DrawRoundedRect(fullCard, C_CARD_BG, CARD_RADIUS);
            }

            // 缩略图
            if (suits[i].thumbClothID > 0) {
                SDL_Texture* thumb = GetThumbTexture(suits[i].thumbClothID);
                if (thumb) {
                    int tw, th;
                    SDL_QueryTexture(thumb, nullptr, nullptr, &tw, &th);
                    int maxSize = GRID_CELL_SIZE - 12;
                    float scale = std::min((float)maxSize / tw, (float)maxSize / th);
                    int dw = (int)(tw * scale);
                    int dh = (int)(th * scale);
                    SDL_Rect thumbDst = {
                        cellX + (GRID_CELL_SIZE - dw) / 2,
                        cellY + (GRID_CELL_SIZE - dh) / 2,
                        dw, dh
                    };
                    SDL_RenderCopy(m_renderer, thumb, nullptr, &thumbDst);
                }
            }

            // 套装件数角标（左上角，圆角）
            {
                std::string pieceStr = std::to_string(suits[i].clothesList.size());
                int tagW = 32, tagH = 26;
                SDL_Rect valTag = {cellX + 4, cellY + 4, tagW, tagH};
                DrawRoundedRect(valTag, tagBg, 12);
                DrawTextCentered(pieceStr, valTag, m_font, C_WHITE);
            }

            // 套装名称（卡片外部下方，居中显示）
            std::string displayName = suits[i].name;
            // UTF-8 截断：按字符数限制
            {
                int charCount = 0, bytePos = 0;
                while (bytePos < (int)displayName.size() && charCount < 5) {
                    unsigned char c = displayName[bytePos];
                    if (c < 0x80) bytePos += 1;
                    else if (c < 0xE0) bytePos += 2;
                    else if (c < 0xF0) bytePos += 3;
                    else bytePos += 4;
                    charCount++;
                }
                if (bytePos < (int)displayName.size()) {
                    displayName = displayName.substr(0, bytePos) + "...";
                }
            }
            // 底部名称
            SDL_Rect nameArea = {cellX, cellY + GRID_CELL_SIZE, GRID_CELL_SIZE, GRID_NAME_H};
            if (selected) {
                // 选中态：深色底栏 + 白色文字
                DrawRoundedRect(nameArea, (SDL_Color){210, 130, 155, 230}, CARD_RADIUS);
                // 用矩形覆盖上半部分圆角，与图片区域衔接
                SDL_Rect topCover = {cellX, cellY + GRID_CELL_SIZE, GRID_CELL_SIZE, CARD_RADIUS};
                SDL_SetRenderDrawColor(m_renderer, 210, 130, 155, 230);
                SDL_RenderFillRect(m_renderer, &topCover);
                DrawTextCentered(displayName, nameArea, m_fontSmall, C_WHITE);
            } else {
                // 未选中态：深色文字
                DrawTextCentered(displayName, nameArea, m_fontSmall, (SDL_Color){85, 55, 70, 255});
            }
        }
    } else if (m_currentCategory == CAT_CURRENT) {
        // ========== 当前穿戴网格模式 ==========
        auto items = GetCurrentWornItems();
        itemCount = (int)items.size();

        if (itemCount == 0) {
            // 无当前穿戴时显示提示
            DrawTextCentered("当前没有额外穿戴的服装",
                {panelX, gridTop + gridHeight / 2 - 20, curRightW, 40},
                m_font, C_GRAY);
        }

        for (int i = 0; i < itemCount; i++) {
            int row = i / GRID_COLS;
            int col = i % GRID_COLS;

            int cellX = panelX + GRID_PADDING + col * cellTotalW;
            int cellY = gridTop + GRID_PADDING + row * cellTotalH - m_gridScrollOffset;

            if (cellY + GRID_CELL_SIZE + GRID_NAME_H < gridTop || cellY > gridTop + gridHeight) continue;

            SDL_Rect fullCard = {cellX, cellY, GRID_CELL_SIZE, GRID_CELL_SIZE + GRID_NAME_H};
            SDL_Rect cellRect = {cellX, cellY, GRID_CELL_SIZE, GRID_CELL_SIZE};
            bool hovered = (mx >= fullCard.x && mx < fullCard.x + fullCard.w &&
                           my >= fullCard.y && my < fullCard.y + fullCard.h);

            // 当前穿戴的服装都是选中态
            if (hovered) {
                DrawRoundedRect(fullCard, (SDL_Color){250, 215, 225, 255}, CARD_RADIUS);
            } else {
                DrawRoundedRect(fullCard, (SDL_Color){240, 205, 220, 255}, CARD_RADIUS);
            }

            // 缩略图
            SDL_Texture* thumb = GetThumbTexture(items[i].clothID);
            if (thumb) {
                int tw, th;
                SDL_QueryTexture(thumb, nullptr, nullptr, &tw, &th);
                int maxSize = GRID_CELL_SIZE - 12;
                float scale = std::min((float)maxSize / tw, (float)maxSize / th);
                int dw = (int)(tw * scale);
                int dh = (int)(th * scale);
                SDL_Rect thumbDst = {
                    cellX + (GRID_CELL_SIZE - dw) / 2,
                    cellY + (GRID_CELL_SIZE - dh) / 2,
                    dw, dh
                };
                SDL_RenderCopy(m_renderer, thumb, nullptr, &thumbDst);
            }

            // 时尚值角标（左上角，圆角）
            if (items[i].clothVal > 0) {
                std::string valStr = std::to_string(items[i].clothVal);
                int tagW = 32, tagH = 26;
                SDL_Rect valTag = {cellX + 4, cellY + 4, tagW, tagH};
                DrawRoundedRect(valTag, tagBg, 12);
                DrawTextCentered(valStr, valTag, m_font, C_WHITE);
            }

            // 名称（卡片下方）
            std::string displayName = items[i].name;
            {
                int charCount = 0, bytePos = 0;
                while (bytePos < (int)displayName.size() && charCount < 5) {
                    unsigned char c = displayName[bytePos];
                    if (c < 0x80) bytePos += 1;
                    else if (c < 0xE0) bytePos += 2;
                    else if (c < 0xF0) bytePos += 3;
                    else bytePos += 4;
                    charCount++;
                }
                if (bytePos < (int)displayName.size()) {
                    displayName = displayName.substr(0, bytePos) + "...";
                }
            }
            SDL_Rect nameArea = {cellX, cellY + GRID_CELL_SIZE, GRID_CELL_SIZE, GRID_NAME_H};
            DrawRoundedRect(nameArea, (SDL_Color){210, 130, 155, 230}, CARD_RADIUS);
            SDL_Rect topCover = {cellX, cellY + GRID_CELL_SIZE, GRID_CELL_SIZE, CARD_RADIUS};
            SDL_SetRenderDrawColor(m_renderer, 210, 130, 155, 230);
            SDL_RenderFillRect(m_renderer, &topCover);
            DrawTextCentered(displayName, nameArea, m_fontSmall, C_WHITE);
        }
    } else {
        // ========== 普通服装网格模式 ==========
        auto items = GetFilteredItems();
        itemCount = (int)items.size();

        for (int i = 0; i < itemCount; i++) {
            int row = i / GRID_COLS;
            int col = i % GRID_COLS;

            int cellX = panelX + GRID_PADDING + col * cellTotalW;
            int cellY = gridTop + GRID_PADDING + row * cellTotalH - m_gridScrollOffset;

            if (cellY + GRID_CELL_SIZE + GRID_NAME_H < gridTop || cellY > gridTop + gridHeight) continue;

            // 整体卡片区域（含名称栏）
            SDL_Rect fullCard = {cellX, cellY, GRID_CELL_SIZE, GRID_CELL_SIZE + GRID_NAME_H};
            SDL_Rect cellRect = {cellX, cellY, GRID_CELL_SIZE, GRID_CELL_SIZE};
            bool hovered = (mx >= fullCard.x && mx < fullCard.x + fullCard.w &&
                           my >= fullCard.y && my < fullCard.y + fullCard.h);
            bool selected = m_avatar->IsWearing(items[i].clothID);

            // 卡片背景（圆角矩形，包含名称区域为整体）
            if (selected) {
                DrawRoundedRect(fullCard, (SDL_Color){240, 205, 220, 255}, CARD_RADIUS);
            } else if (hovered) {
                DrawRoundedRect(fullCard, (SDL_Color){250, 230, 238, 255}, CARD_RADIUS);
            } else {
                DrawRoundedRect(fullCard, C_CARD_BG, CARD_RADIUS);
            }

            // 缩略图
            SDL_Texture* thumb = GetThumbTexture(items[i].clothID);
            if (thumb) {
                int tw, th;
                SDL_QueryTexture(thumb, nullptr, nullptr, &tw, &th);
                int maxSize = GRID_CELL_SIZE - 12;
                float scale = std::min((float)maxSize / tw, (float)maxSize / th);
                int dw = (int)(tw * scale);
                int dh = (int)(th * scale);
                SDL_Rect thumbDst = {
                    cellX + (GRID_CELL_SIZE - dw) / 2,
                    cellY + (GRID_CELL_SIZE - dh) / 2,
                    dw, dh
                };
                SDL_RenderCopy(m_renderer, thumb, nullptr, &thumbDst);
            }

            // 时尚值角标（左上角，圆角）
            if (items[i].clothVal > 0) {
                std::string valStr = std::to_string(items[i].clothVal);
                int tagW = 32, tagH = 26;
                SDL_Rect valTag = {cellX + 4, cellY + 4, tagW, tagH};
                DrawRoundedRect(valTag, tagBg, 12);
                DrawTextCentered(valStr, valTag, m_font, C_WHITE);
            }

            // 名称（卡片外部下方，居中显示）
            std::string displayName = items[i].name;
            {
                int charCount = 0, bytePos = 0;
                while (bytePos < (int)displayName.size() && charCount < 5) {
                    unsigned char c = displayName[bytePos];
                    if (c < 0x80) bytePos += 1;
                    else if (c < 0xE0) bytePos += 2;
                    else if (c < 0xF0) bytePos += 3;
                    else bytePos += 4;
                    charCount++;
                }
                if (bytePos < (int)displayName.size()) {
                    displayName = displayName.substr(0, bytePos) + "...";
                }
            }
            // 底部名称
            SDL_Rect nameArea = {cellX, cellY + GRID_CELL_SIZE, GRID_CELL_SIZE, GRID_NAME_H};
            if (selected) {
                // 选中态：深色底栏 + 白色文字
                DrawRoundedRect(nameArea, (SDL_Color){210, 130, 155, 230}, CARD_RADIUS);
                // 用矩形覆盖上半部分圆角，与图片区域衔接
                SDL_Rect topCover = {cellX, cellY + GRID_CELL_SIZE, GRID_CELL_SIZE, CARD_RADIUS};
                SDL_SetRenderDrawColor(m_renderer, 210, 130, 155, 230);
                SDL_RenderFillRect(m_renderer, &topCover);
                DrawTextCentered(displayName, nameArea, m_fontSmall, C_WHITE);
            } else {
                // 未选中态：深色文字
                DrawTextCentered(displayName, nameArea, m_fontSmall, (SDL_Color){85, 55, 70, 255});
            }
        }
    }

    SDL_RenderSetClipRect(m_renderer, nullptr);

    // 滚动条（灰蓝色）
    int rows = (itemCount + GRID_COLS - 1) / GRID_COLS;
    int totalH = rows * cellTotalH;
    if (totalH > gridHeight) {
        int scrollBarH = std::max(20, gridHeight * gridHeight / totalH);
        int maxScroll = totalH - gridHeight;
        int scrollBarY = gridTop + (gridHeight - scrollBarH) * m_gridScrollOffset / maxScroll;
        SDL_Rect scrollBar = {panelX + curRightW - 5, scrollBarY, 4, scrollBarH};
        SDL_SetRenderDrawColor(m_renderer, 215, 180, 195, 150);
        SDL_RenderFillRect(m_renderer, &scrollBar);
    }
}

// ==================== 底部状态栏 ====================
void Game::DrawBottomBar() {
    int curRightW = GetCurrentRightPanelW();
    int panelX = m_width - curRightW;
    int barY = m_height - BOTTOM_BAR_H;

    if (curRightW <= 0) return; // 收起时不绘制

    // 使用完整面板宽度布局，整体跟随panelX滑动（收起时滑出屏幕右侧）
    int fullW = RIGHT_PANEL_W;

    // 背景
    SDL_Rect bg = {panelX, barY, fullW, BOTTOM_BAR_H};
    FillRect(bg, C_BOTTOM_BG);

    // 顶部分割线
    SDL_SetRenderDrawColor(m_renderer, C_DIVIDER.r, C_DIVIDER.g, C_DIVIDER.b, 255);
    SDL_RenderDrawLine(m_renderer, panelX, barY, panelX + fullW, barY);

    // 当前时尚值
    DrawText("当前时尚值", panelX + 15, barY + 8, m_fontSmall, C_GRAY);

    int fashionVal = m_avatar->GetTotalFashionValue();
    std::string valStr = std::to_string(fashionVal);
    DrawText(valStr, panelX + 15, barY + 28, m_fontLarge, C_FASHION_VAL);

    // 按钮布局（从右到左：加载、保存、重置，使用完整宽度定位）
    int btnH = 36;
    int btnY = barY + (BOTTOM_BAR_H - btnH) / 2;
    int loadBtnX = panelX + fullW - 120;
    int saveBtnX = loadBtnX - 110;
    int resetBtnX = saveBtnX - 110;

    SDL_Rect resetBtn = {resetBtnX, btnY, 100, btnH};
    FillRect(resetBtn, C_BTN_RESET);
    DrawTextCentered("重置", resetBtn, m_font, C_WHITE);

    SDL_Rect saveBtn = {saveBtnX, btnY, 100, btnH};
    FillRect(saveBtn, C_BTN_SAVE);
    DrawTextCentered("保存", saveBtn, m_font, C_WHITE);

    // 加载按钮（深蓝色）
    SDL_Rect loadBtn = {loadBtnX, btnY, 100, btnH};
    FillRect(loadBtn, C_BTN_LOAD);
    DrawTextCentered("加载", loadBtn, m_font, C_WHITE);
}

// ==================== 绘制辅助 ====================
void Game::FillRect(const SDL_Rect& rect, SDL_Color color) {
    SDL_SetRenderDrawColor(m_renderer, color.r, color.g, color.b, color.a);
    SDL_RenderFillRect(m_renderer, &rect);
}

SDL_Texture* Game::RenderText(const std::string& text, TTF_Font* font, SDL_Color color) {
    if (text.empty() || !font) return nullptr;
    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text.c_str(), color);
    if (!surface) return nullptr;
    SDL_Texture* texture = SDL_CreateTextureFromSurface(m_renderer, surface);
    SDL_FreeSurface(surface);
    return texture;
}

void Game::DrawText(const std::string& text, int x, int y, TTF_Font* font, SDL_Color color) {
    SDL_Texture* tex = RenderText(text, font, color);
    if (!tex) return;
    int w, h;
    SDL_QueryTexture(tex, nullptr, nullptr, &w, &h);
    // 字体按物理像素大小打开，绘制到逻辑坐标时需要缩放回来
    int logicalW = (int)(w / m_dpiScale + 0.5f);
    int logicalH = (int)(h / m_dpiScale + 0.5f);
    SDL_Rect dst = {x, y, logicalW, logicalH};
    SDL_RenderCopy(m_renderer, tex, nullptr, &dst);
    SDL_DestroyTexture(tex);
}

void Game::DrawTextCentered(const std::string& text, const SDL_Rect& rect, TTF_Font* font, SDL_Color color) {
    SDL_Texture* tex = RenderText(text, font, color);
    if (!tex) return;
    int w, h;
    SDL_QueryTexture(tex, nullptr, nullptr, &w, &h);
    // 字体按物理像素大小打开，绘制到逻辑坐标时需要缩放回来
    int logicalW = (int)(w / m_dpiScale + 0.5f);
    int logicalH = (int)(h / m_dpiScale + 0.5f);
    SDL_Rect dst = {rect.x + (rect.w - logicalW) / 2, rect.y + (rect.h - logicalH) / 2, logicalW, logicalH};
    SDL_RenderCopy(m_renderer, tex, nullptr, &dst);
    SDL_DestroyTexture(tex);
}

void Game::DrawRoundedRect(const SDL_Rect& rect, SDL_Color color, int radius) {
    if (radius <= 0) { FillRect(rect, color); return; }
    if (radius > rect.h / 2) radius = rect.h / 2;
    if (radius > rect.w / 2) radius = rect.w / 2;
    SDL_SetRenderDrawColor(m_renderer, color.r, color.g, color.b, color.a);
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);

    // 使用 Bresenham 圆算法逐行扫描绘制，无锯齿且无缝隙
    // 每行只画一条完整水平线，从左边缘到右边缘
    int r = radius;
    // 圆心坐标（四个角的圆心）
    int ltCx = rect.x + r,          ltCy = rect.y + r;          // 左上
    int rtCx = rect.x + rect.w - r - 1, rtCy = rect.y + r;     // 右上
    int lbCx = rect.x + r,          lbCy = rect.y + rect.h - r - 1; // 左下
    int rbCx = rect.x + rect.w - r - 1, rbCy = rect.y + rect.h - r - 1; // 右下

    // 中间矩形区域（圆角之间的部分）
    SDL_Rect mid = {rect.x, rect.y + r, rect.w, rect.h - 2 * r};
    if (mid.h > 0) SDL_RenderFillRect(m_renderer, &mid);

    // 用 Bresenham 画上下圆角区域，每个y值画一条完整水平线
    int x = 0, y = r, d = 3 - 2 * r;
    // 记录每行是否已画过，避免重复
    std::vector<bool> drawnTop(r + 1, false), drawnBot(r + 1, false);

    auto drawScanlines = [&](int cx_offset, int cy_offset) {
        // 上半部分：cy_offset = x 或 y
        // 画从左上圆心左侧到右上圆心右侧的完整水平线

        // 上方两条线（对称的 x 和 y）
        if (cx_offset <= r) {
            // 线 1: y偏移 = cx_offset
            if (!drawnTop[cx_offset]) {
                int lineY = ltCy - cx_offset;
                int left = ltCx - cy_offset;
                int right = rtCx + cy_offset;
                if (lineY >= rect.y)
                    SDL_RenderDrawLine(m_renderer, left, lineY, right, lineY);
                drawnTop[cx_offset] = true;
            }
            // 线 2: y偏移 = cy_offset
            if (!drawnTop[cy_offset]) {
                int lineY = ltCy - cy_offset;
                int left = ltCx - cx_offset;
                int right = rtCx + cx_offset;
                if (lineY >= rect.y)
                    SDL_RenderDrawLine(m_renderer, left, lineY, right, lineY);
                drawnTop[cy_offset] = true;
            }
        }
        // 下方两条线
        if (cx_offset <= r) {
            if (!drawnBot[cx_offset]) {
                int lineY = lbCy + cx_offset;
                int left = lbCx - cy_offset;
                int right = rbCx + cy_offset;
                if (lineY < rect.y + rect.h)
                    SDL_RenderDrawLine(m_renderer, left, lineY, right, lineY);
                drawnBot[cx_offset] = true;
            }
            if (!drawnBot[cy_offset]) {
                int lineY = lbCy + cy_offset;
                int left = lbCx - cx_offset;
                int right = rbCx + cx_offset;
                if (lineY < rect.y + rect.h)
                    SDL_RenderDrawLine(m_renderer, left, lineY, right, lineY);
                drawnBot[cy_offset] = true;
            }
        }
    };

    while (y >= x) {
        drawScanlines(x, y);
        if (d < 0) {
            d += 4 * x + 6;
        } else {
            d += 4 * (x - y) + 10;
            y--;
        }
        x++;
    }
}

// 绘制圆角矩形边框
void Game::DrawRoundedRectOutline(const SDL_Rect& rect, SDL_Color color, int radius, int thickness) {
    SDL_SetRenderDrawColor(m_renderer, color.r, color.g, color.b, color.a);
    for (int t = 0; t < thickness; t++) {
        SDL_Rect r = {rect.x + t, rect.y + t, rect.w - 2*t, rect.h - 2*t};
        int rad = std::max(0, radius - t);
        // 上边
        SDL_RenderDrawLine(m_renderer, r.x + rad, r.y, r.x + r.w - rad - 1, r.y);
        // 下边
        SDL_RenderDrawLine(m_renderer, r.x + rad, r.y + r.h - 1, r.x + r.w - rad - 1, r.y + r.h - 1);
        // 左边
        SDL_RenderDrawLine(m_renderer, r.x, r.y + rad, r.x, r.y + r.h - rad - 1);
        // 右边
        SDL_RenderDrawLine(m_renderer, r.x + r.w - 1, r.y + rad, r.x + r.w - 1, r.y + r.h - rad - 1);
        // 四个圆角
        int x0 = 0, y0 = rad, d = 3 - 2 * rad;
        while (y0 >= x0) {
            // 左上
            SDL_RenderDrawPoint(m_renderer, r.x + rad - x0, r.y + rad - y0);
            SDL_RenderDrawPoint(m_renderer, r.x + rad - y0, r.y + rad - x0);
            // 右上
            SDL_RenderDrawPoint(m_renderer, r.x + r.w - 1 - rad + x0, r.y + rad - y0);
            SDL_RenderDrawPoint(m_renderer, r.x + r.w - 1 - rad + y0, r.y + rad - x0);
            // 左下
            SDL_RenderDrawPoint(m_renderer, r.x + rad - x0, r.y + r.h - 1 - rad + y0);
            SDL_RenderDrawPoint(m_renderer, r.x + rad - y0, r.y + r.h - 1 - rad + x0);
            // 右下
            SDL_RenderDrawPoint(m_renderer, r.x + r.w - 1 - rad + x0, r.y + r.h - 1 - rad + y0);
            SDL_RenderDrawPoint(m_renderer, r.x + r.w - 1 - rad + y0, r.y + r.h - 1 - rad + x0);
            if (d < 0) { d += 4 * x0 + 6; }
            else { d += 4 * (x0 - y0) + 10; y0--; }
            x0++;
        }
    }
}

// ==================== 缩略图功能 ====================
std::string Game::GetThumbURL(int clothID) {
    // 前端逻辑: "clothes/icon/a0" + numToString(id, 5) + ".png"
    // 即 clothID 补零到 5 位，前面加 "a0"
    std::ostringstream oss;
    oss << "https://aola.100bt.com/h5/clothes/icon/a0"
        << std::setfill('0') << std::setw(5) << clothID
        << ".png";
    return oss.str();
}

SDL_Texture* Game::GetThumbTexture(int clothID) {
    // 检查缓存
    {
        std::lock_guard<std::mutex> lock(m_thumbMutex);
        auto it = m_thumbCache.find(clothID);
        if (it != m_thumbCache.end()) {
            return it->second;  // 可能为 nullptr（加载失败）
        }
    }

    // 未缓存，启动异步加载
    LoadThumbAsync(clothID);
    return nullptr;
}

void Game::ApplyModelDefaults(int model) {
    // 参考前端 ClothesDefaultConfig: 按人模类型设置默认服装和挂载点映射
    // extraDefaultSkins 挂载点常量（与前端 SkinPosInfo 一致）
    const int POS_TOU_FA_QIAN = 19;    // 头发前（刘海）
    const int POS_TOU_FA_HOU = 73;     // 头发后
    const int POS_YAN_JING_ZUI_BA = 24; // 眼睛嘴巴

    if (model == 1) {
        // 新男模 (defaultSkinForNewModelBoy)
        // 底色 + 默认头发6534 + 默认眼睛6536
        m_defaultClothes = {6541, 6542, 6543, 6544, 6545, 6546, 6547, 6563, 6534, 6536};
        m_avatar->SetDefaultSkinMap({
            {26, 6541},                   // DI_SE__TOU_BU
            {36, 6542},                   // DI_SE__SHOU_BI_QIAN
            {67, 6543},                   // DI_SE__SHOU_BI_HOU
            {54, 6544},                   // DI_SE_QU_GAN
            {55, 6545},                   // DI_SE_TUI
            {16, 6546},                   // SHANG_YI_XIU_ZI_ZUI_QIAN
            {34, 6546},                   // YI_FU__XIU_ZI_QIAN
            {39, 6546},                   // YI_FU__QU_GAN
            {50, 6546},                   // SHOU_YAO_KUANG_SHANG_YI_QU_GAN
            {61, 6546},                   // YI_FU__XIU_ZI_HOU
            {68, 6546},                   // YI_FU__PI_FU
            {42, 6547},                   // KU_ZI_QIAN
            {48, 6547},                   // SHANG_YI_HOU_KU_ZI
            {22, 6563},                   // ER_DUO_QIAN
            {30, 6563},                   // ER_DUO_HOU
        });
        // 额外默认部件：头发前=6534, 头发后=6534, 眼睛嘴巴=6536
        m_avatar->SetExtraDefaults(
            {POS_TOU_FA_QIAN, POS_TOU_FA_HOU, POS_YAN_JING_ZUI_BA},
            {6534, 6534, 6536}
        );
    } else if (model == 2) {
        // 新女模 (defaultSkinForNewModelGirl)
        // 底色 + 默认头发6548 + 默认眼睛6550
        m_defaultClothes = {6556, 6557, 6558, 6559, 6560, 6561, 6562, 6564, 6548, 6550};
        m_avatar->SetDefaultSkinMap({
            {26, 6556},                   // DI_SE__TOU_BU
            {36, 6557},                   // DI_SE__SHOU_BI_QIAN
            {67, 6558},                   // DI_SE__SHOU_BI_HOU
            {54, 6559},                   // DI_SE_QU_GAN
            {55, 6560},                   // DI_SE_TUI
            {16, 6561},                   // SHANG_YI_XIU_ZI_ZUI_QIAN
            {34, 6561},                   // YI_FU__XIU_ZI_QIAN
            {39, 6561},                   // YI_FU__QU_GAN
            {50, 6561},                   // SHOU_YAO_KUANG_SHANG_YI_QU_GAN
            {61, 6561},                   // YI_FU__XIU_ZI_HOU
            {68, 6561},                   // YI_FU__PI_FU
            {52, 6562},                   // NEI_KU_WA_ZI
            {22, 6564},                   // ER_DUO_QIAN
            {30, 6564},                   // ER_DUO_HOU
        });
        // 额外默认部件：头发前=6548, 头发后=6548, 眼睛嘴巴=6550
        m_avatar->SetExtraDefaults(
            {POS_TOU_FA_QIAN, POS_TOU_FA_HOU, POS_YAN_JING_ZUI_BA},
            {6548, 6548, 6550}
        );
    } else {
        // 旧人模 (defaultSkins)
        // 底色 + 默认头发72 + 默认眼睛73
        m_defaultClothes = {6, 7, 741, 742, 72, 73};
        m_avatar->SetDefaultSkinMap({
            {26, 6},                      // DI_SE__TOU_BU
            {36, 7}, {67, 7}, {53, 7},   // DI_SE__SHOU_BI_QIAN / HOU / QU_GAN_HE_TUI
            {39, 741}, {34, 741}, {61, 741}, // YI_FU
            {49, 742},                    // KU_ZI
        });
        // 额外默认部件：头发前=72, 头发后=72, 眼睛嘴巴=73
        m_avatar->SetExtraDefaults(
            {POS_TOU_FA_QIAN, POS_TOU_FA_HOU, POS_YAN_JING_ZUI_BA},
            {72, 72, 73}
        );
    }
}

void Game::LoadThumbAsync(int clothID) {
    {
        std::lock_guard<std::mutex> lock(m_thumbMutex);
        // 已在加载中或已缓存
        if (m_thumbLoading.count(clothID) || m_thumbCache.count(clothID)) return;
        m_thumbLoading.insert(clothID);
    }

    std::string url = GetThumbURL(clothID);

    // 后台线程仅下载数据，不创建纹理
    std::thread([this, clothID, url]() {
        auto data = ResourceManager::HttpGet(url);

        // 将下载数据放入待处理队列，由主线程创建纹理
        std::lock_guard<std::mutex> lock(m_thumbPendingMutex);
        m_thumbPending.push({clothID, std::move(data)});
    }).detach();
}

// ==================== 鼠标坐标转换（窗口坐标 → 逻辑坐标） ====================
void Game::WindowToLogical(int windowX, int windowY, int& logicalX, int& logicalY) {
    // SDL 2.26+ 在使用 SDL_RenderSetLogicalSize 时，
    // SDL_GetMouseState 和 event.button 自动返回逻辑坐标
    logicalX = windowX;
    logicalY = windowY;
}

// ==================== 窗口缩放处理 ====================
void Game::OnWindowResize(int newW, int newH) {
    // 重新计算 DPI 缩放和逻辑尺寸
    int renderW, renderH;
    SDL_GetRendererOutputSize(m_renderer, &renderW, &renderH);

#ifdef ANDROID_PLATFORM
    // Android: 保持固定逻辑宽度 1280，高度按屏幕比例计算
    const int LOGICAL_WIDTH = 1280;
    float screenAspect = (float)renderH / (float)renderW;
    float newDpiScale = (float)renderW / (float)LOGICAL_WIDTH;
    if (std::abs(newDpiScale - m_dpiScale) > 0.01f) {
        m_dpiScale = newDpiScale;
        ReloadFonts();
    }
    m_width = LOGICAL_WIDTH;
    m_height = (int)(LOGICAL_WIDTH * screenAspect + 0.5f);
#else
    float newDpiScale = (float)renderW / (float)newW;

    // 如果 DPI 缩放变了（比如窗口在不同 DPI 屏幕间移动），重载字体
    if (std::abs(newDpiScale - m_dpiScale) > 0.01f) {
        m_dpiScale = newDpiScale;
        ReloadFonts();
    }

    // 根据实际渲染尺寸和 DPI 计算逻辑尺寸，确保 viewport 无偏移
    m_width = (int)(renderW / m_dpiScale + 0.5f);
    m_height = (int)(renderH / m_dpiScale + 0.5f);

    // 如果窗口尺寸和逻辑尺寸不同，调整窗口
    if (m_width != newW || m_height != newH) {
        SDL_SetWindowSize(m_window, m_width, m_height);
        SDL_GetRendererOutputSize(m_renderer, &renderW, &renderH);
    }
#endif

    SDL_RenderSetLogicalSize(m_renderer, m_width, m_height);

    std::cout << "窗口调整: " << m_width << "x" << m_height
              << " 渲染: " << renderW << "x" << renderH << std::endl;
}

void Game::ReloadFonts() {
    // 先关闭旧字体
    if (m_fontTiny)  { TTF_CloseFont(m_fontTiny);  m_fontTiny = nullptr; }
    if (m_fontSmall) { TTF_CloseFont(m_fontSmall); m_fontSmall = nullptr; }
    if (m_font)      { TTF_CloseFont(m_font);      m_font = nullptr; }
    if (m_fontLarge) { TTF_CloseFont(m_fontLarge); m_fontLarge = nullptr; }

    // 按物理像素大小打开字体，确保 HiDPI 下文字清晰
    // 逻辑字号 * dpiScale = 物理字号
    int sizeLarge = (int)(20 * m_dpiScale + 0.5f);
    int sizeNormal = (int)(15 * m_dpiScale + 0.5f);
    int sizeSmall = (int)(12 * m_dpiScale + 0.5f);
    int sizeTiny = (int)(10 * m_dpiScale + 0.5f);

#ifdef ANDROID_PLATFORM
    // Android: 优先使用系统中文字体（确保有完整的中文字形）
    // 系统字体路径列表（按优先级排序）
    const char* sysFontPaths[] = {
        "/system/fonts/NotoSansSC-Regular.otf",
        "/system/fonts/NotoSansCJK-Regular.ttc",
        "/system/fonts/DroidSansFallback.ttf",
        "/system/fonts/NotoSansHans-Regular.otf",
        "/system/fonts/DroidSansChinese.ttf",
        nullptr
    };
    for (int i = 0; sysFontPaths[i]; i++) {
        m_fontLarge = TTF_OpenFont(sysFontPaths[i], sizeLarge);
        if (m_fontLarge) {
            m_font = TTF_OpenFont(sysFontPaths[i], sizeNormal);
            m_fontSmall = TTF_OpenFont(sysFontPaths[i], sizeSmall);
            m_fontTiny = TTF_OpenFont(sysFontPaths[i], sizeTiny);
            LOGI("使用系统字体: %s (字号: %d/%d/%d/%d)", sysFontPaths[i], sizeLarge, sizeNormal, sizeSmall, sizeTiny);
            break;
        }
    }
    // 备用方案：从 assets 加载字体
    if (!m_fontLarge) {
        const char* assetFontPaths[] = {
            "fonts/NotoSansSC-Regular.ttf",
            "NotoSansSC-Regular.ttf",
            "fonts/DroidSansFallback.ttf",
            nullptr
        };
        for (int i = 0; assetFontPaths[i]; i++) {
            SDL_RWops* rw = SDL_RWFromFile(assetFontPaths[i], "rb");
            if (rw) {
                SDL_RWclose(rw);
                m_fontLarge = TTF_OpenFont(assetFontPaths[i], sizeLarge);
                if (m_fontLarge) {
                    m_font = TTF_OpenFont(assetFontPaths[i], sizeNormal);
                    m_fontSmall = TTF_OpenFont(assetFontPaths[i], sizeSmall);
                    m_fontTiny = TTF_OpenFont(assetFontPaths[i], sizeTiny);
                    LOGI("使用assets字体: %s (字号: %d/%d/%d/%d)", assetFontPaths[i], sizeLarge, sizeNormal, sizeSmall, sizeTiny);
                    break;
                }
            }
        }
    }
    if (!m_fontLarge) {
        // 最后备用：Roboto（不支持中文，但至少能显示英文）
        m_fontLarge = TTF_OpenFont("/system/fonts/Roboto-Regular.ttf", sizeLarge);
        if (m_fontLarge) {
            m_font = TTF_OpenFont("/system/fonts/Roboto-Regular.ttf", sizeNormal);
            m_fontSmall = TTF_OpenFont("/system/fonts/Roboto-Regular.ttf", sizeSmall);
            m_fontTiny = TTF_OpenFont("/system/fonts/Roboto-Regular.ttf", sizeTiny);
        }
        LOGI("使用 Roboto 备用字体（可能不支持中文）");
    }
#else
    // 桌面平台字体路径
#ifdef _WIN32
    // Windows: 系用系统字体或自带字体
    const char* fontPaths[] = {
        "fonts/msyh.ttc",                    // 自带微软雅黑
        "fonts/simhei.ttf",                  // 自带黑体
        "C:\\Windows\\Fonts\\msyh.ttc",      // 微软雅黑
        "C:\\Windows\\Fonts\\msyhbd.ttc",    // 微软雅黑粗体
        "C:\\Windows\\Fonts\\simhei.ttf",    // 黑体
        "C:\\Windows\\Fonts\\simsun.ttc",    // 宋体
        "C:\\Windows\\Fonts\\arial.ttf",     // Arial（备用）
        nullptr
    };
#else
    // macOS / Linux
    const char* fontPaths[] = {
        "/System/Library/Fonts/STHeiti Medium.ttc",
        "/System/Library/Fonts/STHeiti Light.ttc",
        "/Library/Fonts/Arial Unicode.ttf",
        "/System/Library/Fonts/Supplemental/Arial Unicode.ttf",
        "/System/Library/Fonts/PingFang.ttc",
        "/System/Library/Fonts/Hiragino Sans GB.ttc",
        "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/truetype/droid/DroidSansFallback.ttf",
        nullptr
    };
#endif
    for (int i = 0; fontPaths[i]; i++) {
        m_fontLarge = TTF_OpenFont(fontPaths[i], sizeLarge);
        if (m_fontLarge) {
            m_font = TTF_OpenFont(fontPaths[i], sizeNormal);
            m_fontSmall = TTF_OpenFont(fontPaths[i], sizeSmall);
            m_fontTiny = TTF_OpenFont(fontPaths[i], sizeTiny);
            std::cout << "使用字体: " << fontPaths[i]
                      << " (物理字号: " << sizeLarge << "/" << sizeNormal
                      << "/" << sizeSmall << "/" << sizeTiny << ")" << std::endl;
            break;
        }
    }
    if (!m_fontLarge) {
        // 备用方案：使用平台默认字体
#ifdef _WIN32
        m_fontLarge = TTF_OpenFont("C:\\Windows\\Fonts\\arial.ttf", sizeLarge);
        m_font = TTF_OpenFont("C:\\Windows\\Fonts\\arial.ttf", sizeNormal);
        m_fontSmall = TTF_OpenFont("C:\\Windows\\Fonts\\arial.ttf", sizeSmall);
        m_fontTiny = TTF_OpenFont("C:\\Windows\\Fonts\\arial.ttf", sizeTiny);
#else
        m_fontLarge = TTF_OpenFont("/System/Library/Fonts/Helvetica.ttc", sizeLarge);
        m_font = TTF_OpenFont("/System/Library/Fonts/Helvetica.ttc", sizeNormal);
        m_fontSmall = TTF_OpenFont("/System/Library/Fonts/Helvetica.ttc", sizeSmall);
        m_fontTiny = TTF_OpenFont("/System/Library/Fonts/Helvetica.ttc", sizeTiny);
#endif
    }
#endif

    // 设置所有字体为正常样式（不加粗）
    if (m_fontLarge) TTF_SetFontStyle(m_fontLarge, TTF_STYLE_NORMAL);
    if (m_font)      TTF_SetFontStyle(m_font, TTF_STYLE_NORMAL);
    if (m_fontSmall) TTF_SetFontStyle(m_fontSmall, TTF_STYLE_NORMAL);
    if (m_fontTiny)  TTF_SetFontStyle(m_fontTiny, TTF_STYLE_NORMAL);
}

// ==================== 装扮方案保存/加载 ====================

std::string Game::GenerateOutfitCode(int id) {
    // 生成6位数字编号，不足补零
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(6) << id;
    return oss.str();
}

// ==================== 口令加密/解密 ====================

// Base64编码表
static const char* B64_CHARS = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static std::string Base64Encode(const std::vector<uint8_t>& data) {
    std::string result;
    int val = 0, valb = -6;
    for (uint8_t c : data) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            result.push_back(B64_CHARS[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) result.push_back(B64_CHARS[((val << 8) >> (valb + 8)) & 0x3F]);
    while (result.size() % 4) result.push_back('=');
    return result;
}

static std::vector<uint8_t> Base64Decode(const std::string& encoded) {
    std::vector<int> T(256, -1);
    for (int i = 0; i < 64; i++) T[(int)B64_CHARS[i]] = i;
    std::vector<uint8_t> result;
    int val = 0, valb = -8;
    for (char c : encoded) {
        if (T[(int)(unsigned char)c] == -1) break;
        val = (val << 6) + T[(int)(unsigned char)c];
        valb += 6;
        if (valb >= 0) {
            result.push_back((uint8_t)((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return result;
}

// 简单XOR加密（使用固定密钥）
static const uint8_t XOR_KEY[] = {0xA3, 0x5B, 0x7E, 0x19, 0xC4, 0x62, 0xD8, 0x3F};

static void XorCrypt(std::vector<uint8_t>& data) {
    for (size_t i = 0; i < data.size(); i++) {
        data[i] ^= XOR_KEY[i % sizeof(XOR_KEY)];
    }
}

std::string Game::EncodeOutfitToken(int model, const std::vector<int>& clothes) {
    // 格式: model|id1,id2,id3...
    std::string payload = std::to_string(model) + "|";
    for (size_t i = 0; i < clothes.size(); i++) {
        if (i > 0) payload += ",";
        payload += std::to_string(clothes[i]);
    }

    // 转为字节数组并XOR加密
    std::vector<uint8_t> data(payload.begin(), payload.end());
    XorCrypt(data);

    // Base64编码，加上口令前缀
    return "#AO" + Base64Encode(data);
}

bool Game::DecodeOutfitToken(const std::string& token, int& outModel, std::vector<int>& outClothes) {
    // 检查前缀
    if (token.size() < 4 || token.substr(0, 3) != "#AO") return false;

    std::string b64Part = token.substr(3);
    std::vector<uint8_t> data = Base64Decode(b64Part);
    if (data.empty()) return false;

    // XOR解密
    XorCrypt(data);

    std::string payload(data.begin(), data.end());

    // 解析: model|id1,id2,id3...
    size_t sep = payload.find('|');
    if (sep == std::string::npos) return false;

    try {
        outModel = std::stoi(payload.substr(0, sep));
    } catch (...) {
        return false;
    }

    outClothes.clear();
    std::string clothesPart = payload.substr(sep + 1);
    std::istringstream iss(clothesPart);
    std::string tok;
    while (std::getline(iss, tok, ',')) {
        if (!tok.empty()) {
            try {
                outClothes.push_back(std::stoi(tok));
            } catch (...) {}
        }
    }

    return !outClothes.empty();
}

bool Game::LoadOutfitFromToken(const std::string& token) {
    int model;
    std::vector<int> clothes;
    if (!DecodeOutfitToken(token, model, clothes)) return false;

    // 切换人模
    if (model != m_model) {
        m_model = model;
        m_avatar->SetModel(m_model);
        ApplyModelDefaults(m_model);
    }

    m_avatar->SetClothes(clothes);
    m_avatar->LoadParts();

    std::cout << "从口令加载装扮! 人模: " << model
              << " 服装数: " << clothes.size() << std::endl;
    return true;
}

// ==================== SQLite 数据库操作 ====================

void Game::InitOutfitDB() {
#ifdef ANDROID_PLATFORM
    std::string dbPath = AndroidPlatform::GetInternalStoragePath() + OUTFIT_DB;
#else
    std::string dbPath = OUTFIT_DB;
#endif

    int rc = sqlite3_open(dbPath.c_str(), &m_outfitDB);
    if (rc != SQLITE_OK) {
        std::cerr << "无法打开数据库: " << sqlite3_errmsg(m_outfitDB) << std::endl;
#ifdef ANDROID_PLATFORM
        __android_log_print(ANDROID_LOG_ERROR, "AolaNuanNuan",
            "sqlite3_open 失败: rc=%d, path=%s, err=%s", rc, dbPath.c_str(), sqlite3_errmsg(m_outfitDB));
#endif
        m_outfitDB = nullptr;
        return;
    }
#ifdef ANDROID_PLATFORM
    __android_log_print(ANDROID_LOG_INFO, "AolaNuanNuan",
        "数据库打开成功: %s", dbPath.c_str());
#endif

    // 创建方案表（如果不存在）
    const char* createSQL = R"(
        CREATE TABLE IF NOT EXISTS outfits (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            code TEXT UNIQUE NOT NULL,
            name TEXT DEFAULT '',
            model INTEGER DEFAULT 0,
            fashion_value INTEGER DEFAULT 0,
            clothes TEXT NOT NULL,
            token TEXT DEFAULT '',
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP
        );
    )";
    char* errMsg = nullptr;
    rc = sqlite3_exec(m_outfitDB, createSQL, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "建表失败: " << errMsg << std::endl;
        sqlite3_free(errMsg);
    }

    // 迁移旧表：添加 token 列（如果不存在）
    const char* alterSQL = "ALTER TABLE outfits ADD COLUMN token TEXT DEFAULT ''";
    sqlite3_exec(m_outfitDB, alterSQL, nullptr, nullptr, nullptr); // 忽略已存在错误

    // 为已有方案补充生成 token（如果 token 为空）
    {
        const char* selSQL = "SELECT code, model, clothes FROM outfits WHERE token = '' OR token IS NULL";
        sqlite3_stmt* selStmt = nullptr;
        if (sqlite3_prepare_v2(m_outfitDB, selSQL, -1, &selStmt, nullptr) == SQLITE_OK) {
            while (sqlite3_step(selStmt) == SQLITE_ROW) {
                std::string oCode = reinterpret_cast<const char*>(sqlite3_column_text(selStmt, 0));
                int oModel = sqlite3_column_int(selStmt, 1);
                const char* oClothesText = reinterpret_cast<const char*>(sqlite3_column_text(selStmt, 2));
                std::string oClothesStr = oClothesText ? oClothesText : "";
                // 解析服装列表
                std::vector<int> oClothes;
                std::istringstream oIss(oClothesStr);
                std::string oTok;
                while (std::getline(oIss, oTok, ',')) {
                    if (!oTok.empty()) { try { oClothes.push_back(std::stoi(oTok)); } catch (...) {} }
                }
                std::string oToken = EncodeOutfitToken(oModel, oClothes);
                const char* updSQL = "UPDATE outfits SET token = ? WHERE code = ?";
                sqlite3_stmt* updStmt = nullptr;
                if (sqlite3_prepare_v2(m_outfitDB, updSQL, -1, &updStmt, nullptr) == SQLITE_OK) {
                    sqlite3_bind_text(updStmt, 1, oToken.c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_bind_text(updStmt, 2, oCode.c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_step(updStmt);
                    sqlite3_finalize(updStmt);
                }
            }
            sqlite3_finalize(selStmt);
        }
    }

    // 尝试从旧的 JSON 文件迁移数据
    MigrateFromJSON();

    std::cout << "装扮方案数据库已初始化: " << dbPath << std::endl;
#ifdef ANDROID_PLATFORM
    __android_log_print(ANDROID_LOG_INFO, "AolaNuanNuan",
        "装扮方案数据库已初始化: %s", dbPath.c_str());
#endif
}

void Game::CloseOutfitDB() {
    if (m_outfitDB) {
        sqlite3_close(m_outfitDB);
        m_outfitDB = nullptr;
    }
}

void Game::MigrateFromJSON() {
    if (!m_outfitDB) return;

    // 检查数据库是否已有数据，有则不迁移
    const char* countSQL = "SELECT COUNT(*) FROM outfits";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_outfitDB, countSQL, -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_int(stmt, 0) > 0) {
            sqlite3_finalize(stmt);
            return;  // 已有数据，不重复迁移
        }
        sqlite3_finalize(stmt);
    }

    // 读取旧的 index.json
#ifdef ANDROID_PLATFORM
    std::string outfitDir = AndroidPlatform::GetInternalStoragePath() + "outfits/";
    std::string indexPath = outfitDir + "index.json";
    bool indexExists = android_file_exists(indexPath);
#else
    std::string outfitDir = "outfits/";
    std::string indexPath = outfitDir + "index.json";
    bool indexExists = fs::exists(indexPath);
#endif

    if (!indexExists) return;

    try {
        std::ifstream ifs(indexPath);
        json idx = json::parse(ifs);
        if (!idx.is_object() || !idx.contains("outfits")) return;

        int migrated = 0;
        for (const auto& entry : idx["outfits"]) {
            std::string code = entry.value("code", "");
            if (code.empty()) continue;

            std::string name = entry.value("name", "");
            int model = entry.value("model", 0);
            int fashionValue = entry.value("fashion_value", 0);

            // 读取对应的方案文件获取服装列表
            std::string outfitPath = outfitDir + code + ".json";
            std::string clothesStr;
            try {
                std::ifstream oifs(outfitPath);
                json outfit = json::parse(oifs);
                if (outfit.contains("clothes") && outfit["clothes"].is_array()) {
                    for (size_t i = 0; i < outfit["clothes"].size(); i++) {
                        if (i > 0) clothesStr += ",";
                        clothesStr += std::to_string(outfit["clothes"][i].get<int>());
                    }
                }
            } catch (...) {
                continue;  // 方案文件读取失败则跳过
            }

            // 插入数据库
            const char* insertSQL = "INSERT OR IGNORE INTO outfits (code, name, model, fashion_value, clothes) VALUES (?, ?, ?, ?, ?)";
            sqlite3_stmt* insertStmt = nullptr;
            if (sqlite3_prepare_v2(m_outfitDB, insertSQL, -1, &insertStmt, nullptr) == SQLITE_OK) {
                sqlite3_bind_text(insertStmt, 1, code.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(insertStmt, 2, name.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_int(insertStmt, 3, model);
                sqlite3_bind_int(insertStmt, 4, fashionValue);
                sqlite3_bind_text(insertStmt, 5, clothesStr.c_str(), -1, SQLITE_TRANSIENT);
                if (sqlite3_step(insertStmt) == SQLITE_DONE) {
                    migrated++;
                }
                sqlite3_finalize(insertStmt);
            }
        }
        if (migrated > 0) {
            std::cout << "从JSON迁移了 " << migrated << " 个方案到SQLite数据库" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "JSON数据迁移失败: " << e.what() << std::endl;
    }
}

void Game::LoadOutfitList() {
    m_outfitEntries.clear();
    if (!m_outfitDB) return;

    const char* sql = "SELECT code, name, model, fashion_value, COALESCE(token, '') FROM outfits ORDER BY id DESC";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_outfitDB, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "查询方案列表失败: " << sqlite3_errmsg(m_outfitDB) << std::endl;
        return;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        OutfitEntry oe;
        oe.code = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        const char* nameText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        oe.name = nameText ? nameText : "";
        oe.model = sqlite3_column_int(stmt, 2);
        oe.fashionValue = sqlite3_column_int(stmt, 3);
        const char* tokenText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        oe.token = tokenText ? tokenText : "";
        m_outfitEntries.push_back(oe);
    }
    sqlite3_finalize(stmt);
}

std::string Game::SaveOutfit(const std::string& name) {
    if (!m_outfitDB) {
#ifdef ANDROID_PLATFORM
        __android_log_print(ANDROID_LOG_ERROR, "AolaNuanNuan",
            "SaveOutfit: m_outfitDB 为空，无法保存");
#endif
        return "";
    }

    // 获取下一个可用ID
    int nextID = 1;
    const char* maxSQL = "SELECT COALESCE(MAX(id), 0) + 1 FROM outfits";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_outfitDB, maxSQL, -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            nextID = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    std::string code = GenerateOutfitCode(nextID);
    int fashionValue = m_avatar->GetTotalFashionValue();

    // 构建服装ID列表（逗号分隔字符串）
    std::string clothesStr;
    const auto& wearing = m_avatar->GetClothes();
    for (size_t i = 0; i < wearing.size(); i++) {
        if (i > 0) clothesStr += ",";
        clothesStr += std::to_string(wearing[i]);
    }

    // 生成分享口令
    std::string token = EncodeOutfitToken(m_model, wearing);

    // 插入数据库
    const char* insertSQL = "INSERT INTO outfits (code, name, model, fashion_value, clothes, token) VALUES (?, ?, ?, ?, ?, ?)";
    if (sqlite3_prepare_v2(m_outfitDB, insertSQL, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, code.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 3, m_model);
        sqlite3_bind_int(stmt, 4, fashionValue);
        sqlite3_bind_text(stmt, 5, clothesStr.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 6, token.c_str(), -1, SQLITE_TRANSIENT);

        int rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            std::cerr << "保存方案失败: " << sqlite3_errmsg(m_outfitDB) << std::endl;
#ifdef ANDROID_PLATFORM
            __android_log_print(ANDROID_LOG_ERROR, "AolaNuanNuan",
                "保存方案失败: rc=%d, err=%s", rc, sqlite3_errmsg(m_outfitDB));
#endif
            sqlite3_finalize(stmt);
            return "";
        }
        sqlite3_finalize(stmt);
    }

    std::cout << "装扮已保存! 编号: " << code << " 口令: " << token << " 服装数: " << wearing.size() << std::endl;
#ifdef ANDROID_PLATFORM
    __android_log_print(ANDROID_LOG_INFO, "AolaNuanNuan",
        "装扮已保存! 编号: %s, 口令: %s, 服装数: %zu", code.c_str(), token.c_str(), wearing.size());
#endif
    return code;
}

bool Game::LoadOutfit(const std::string& code) {
    if (!m_outfitDB) return false;

    // 规范化编号：补零到6位
    std::string normalizedCode = code;
    while (normalizedCode.size() < 6) {
        normalizedCode = "0" + normalizedCode;
    }

    const char* sql = "SELECT model, clothes FROM outfits WHERE code = ?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_outfitDB, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "查询方案失败: " << sqlite3_errmsg(m_outfitDB) << std::endl;
        return false;
    }
    sqlite3_bind_text(stmt, 1, normalizedCode.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        std::cerr << "方案不存在: " << normalizedCode << std::endl;
        sqlite3_finalize(stmt);
        return false;
    }

    int model = sqlite3_column_int(stmt, 0);
    const char* clothesText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    std::string clothesStr = clothesText ? clothesText : "";
    sqlite3_finalize(stmt);

    // 切换人模
    if (model != m_model) {
        m_model = model;
        m_avatar->SetModel(m_model);
        ApplyModelDefaults(m_model);
    }

    // 解析服装ID列表（逗号分隔）
    std::vector<int> clothes;
    std::istringstream iss(clothesStr);
    std::string token;
    while (std::getline(iss, token, ',')) {
        if (!token.empty()) {
            try {
                clothes.push_back(std::stoi(token));
            } catch (...) {}
        }
    }

    m_avatar->SetClothes(clothes);
    m_avatar->LoadParts();

    std::cout << "装扮已加载! 编号: " << normalizedCode
              << " 人模: " << model
              << " 服装数: " << clothes.size() << std::endl;
    return true;
}

void Game::RenameOutfit(const std::string& code, const std::string& newName) {
    if (!m_outfitDB) return;

    const char* sql = "UPDATE outfits SET name = ? WHERE code = ?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_outfitDB, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, newName.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, code.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    std::cout << "方案重命名: " << code << " -> " << newName << std::endl;
}

void Game::DeleteOutfit(const std::string& code) {
    if (!m_outfitDB) return;

    const char* sql = "DELETE FROM outfits WHERE code = ?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_outfitDB, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, code.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    std::cout << "方案已删除: " << code << std::endl;
}

void Game::DrawOutfitDialog() {
    Uint32 now = SDL_GetTicks();

    // 绘制提示消息（保存/加载结果提示，3秒后消失）
    if (!m_outfitMsg.empty() && now - m_outfitMsgTime < 3000) {
        int msgW = 300, msgH = 44;
        int msgX = (m_width - msgW) / 2;
        int msgY = m_height - BOTTOM_BAR_H - msgH - 10;

        // 半透明背景
        SDL_Rect msgBg = {msgX, msgY, msgW, msgH};
        FillRect(msgBg, (SDL_Color){62, 60, 66, 220});
        DrawTextCentered(m_outfitMsg, msgBg, m_font, (SDL_Color){245, 240, 235, 255});
    } else if (!m_outfitMsg.empty() && now - m_outfitMsgTime >= 3000) {
        m_outfitMsg.clear();
    }

    // 如果弹窗未激活，不绘制
    if (!m_outfitDialogActive) return;

    // 全屏半透明遮罩
    SDL_Rect overlay = {0, 0, m_width, m_height};
    FillRect(overlay, (SDL_Color){0, 0, 0, 120});

    const char* modelNames[] = {"旧人模", "新男模", "新女模"};

    if (m_outfitShowToken) {
        // ======== 口令展示模式弹窗 ========
        int dlgW = 420, dlgH = 260;
        int dlgX = (m_width - dlgW) / 2;
        int dlgY = (m_height - dlgH) / 2;
        SDL_Rect dlgRect = {dlgX, dlgY, dlgW, dlgH};
        DrawRoundedRect(dlgRect, (SDL_Color){255, 255, 255, 255}, 12);
        DrawRoundedRectOutline(dlgRect, (SDL_Color){230, 200, 212, 255}, 12, 2);

        // 标题
        SDL_Rect titleRect = {dlgX, dlgY + 14, dlgW, 30};
        DrawTextCentered("保存成功！", titleRect, m_fontLarge, (SDL_Color){60, 160, 80, 255});

        // 编号显示
        SDL_Rect codeRect = {dlgX, dlgY + 48, dlgW, 22};
        DrawTextCentered("方案编号: #" + m_outfitSavedCode, codeRect, m_font, (SDL_Color){120, 50, 75, 255});

        // 口令标签
        SDL_Rect tokenLabelRect = {dlgX, dlgY + 76, dlgW, 20};
        DrawTextCentered("分享口令（可发给好友使用）", tokenLabelRect, m_fontSmall, (SDL_Color){170, 140, 155, 255});

        // 口令显示框
        int tokenBoxW = 380, tokenBoxH = 36;
        int tokenBoxX = dlgX + (dlgW - tokenBoxW) / 2;
        int tokenBoxY = dlgY + 100;
        SDL_Rect tokenBoxRect = {tokenBoxX, tokenBoxY, tokenBoxW, tokenBoxH};
        DrawRoundedRect(tokenBoxRect, (SDL_Color){248, 240, 244, 255}, 8);
        DrawRoundedRectOutline(tokenBoxRect, (SDL_Color){210, 180, 195, 255}, 8, 1);

        // 口令文本（截断显示）
        std::string displayToken = m_outfitSavedToken;
        int tw, th;
        TTF_SizeUTF8(m_fontSmall, displayToken.c_str(), &tw, &th);
        if ((int)(tw / m_dpiScale + 0.5f) > tokenBoxW - 16) {
            // 截断并添加省略号
            while (displayToken.size() > 10) {
                displayToken.pop_back();
                TTF_SizeUTF8(m_fontSmall, (displayToken + "...").c_str(), &tw, &th);
                if ((int)(tw / m_dpiScale + 0.5f) <= tokenBoxW - 16) {
                    displayToken += "...";
                    break;
                }
            }
        }
        DrawTextCentered(displayToken, tokenBoxRect, m_fontSmall, (SDL_Color){90, 50, 70, 255});

        // 复制口令按钮
        int copyBtnW = 180, copyBtnH = 36;
        int copyBtnX = dlgX + (dlgW - copyBtnW) / 2;
        int copyBtnY = dlgY + 140;
        SDL_Rect copyBtnRect = {copyBtnX, copyBtnY, copyBtnW, copyBtnH};
        DrawRoundedRect(copyBtnRect, C_BTN_SAVE, 8);
        DrawTextCentered("复制口令", copyBtnRect, m_font, C_WHITE);

        // 关闭按钮
        int closeBtnW = 100, closeBtnH = 32;
        int closeBtnX = dlgX + (dlgW - closeBtnW) / 2;
        int closeBtnY = dlgY + dlgH - closeBtnH - 18;
        SDL_Rect closeBtnRect = {closeBtnX, closeBtnY, closeBtnW, closeBtnH};
        DrawRoundedRect(closeBtnRect, C_BTN_RESET, 8);
        DrawTextCentered("关闭", closeBtnRect, m_font, C_WHITE);

    } else if (!m_outfitDialogIsLoad) {
        // ======== 保存模式弹窗 ========
        int dlgW = 360, dlgH = 200;
        int dlgX = (m_width - dlgW) / 2;
        int dlgY = (m_height - dlgH) / 2;
        SDL_Rect dlgRect = {dlgX, dlgY, dlgW, dlgH};
        DrawRoundedRect(dlgRect, (SDL_Color){255, 255, 255, 255}, 12);
        DrawRoundedRectOutline(dlgRect, (SDL_Color){230, 200, 212, 255}, 12, 2);

        // 标题
        SDL_Rect titleRect = {dlgX, dlgY + 14, dlgW, 30};
        DrawTextCentered("保存装扮方案", titleRect, m_fontLarge, (SDL_Color){120, 50, 75, 255});

        // 提示文字
        SDL_Rect hintRect = {dlgX, dlgY + 48, dlgW, 20};
        DrawTextCentered("为方案起个名字（可选）", hintRect, m_fontSmall, (SDL_Color){170, 140, 155, 255});

        // 名称输入框
        int inputW = 280, inputH = 38;
        int inputX = dlgX + (dlgW - inputW) / 2;
        int inputY = dlgY + 74;
        SDL_Rect inputRect = {inputX, inputY, inputW, inputH};
        DrawRoundedRect(inputRect, (SDL_Color){255, 248, 250, 255}, 8);
        DrawRoundedRectOutline(inputRect, (SDL_Color){220, 185, 198, 255}, 8, 1);

        if (m_outfitNameInput.empty() && m_imeComposition.empty()) {
            DrawTextCentered("例如：日常搭配、战斗套装...", inputRect, m_fontSmall, (SDL_Color){195, 165, 178, 255});
        } else {
            // 显示已输入文本 + 组合文本
            std::string displayText = m_outfitNameInput;
            if (!m_imeComposition.empty()) {
                displayText += m_imeComposition;
            }
            DrawTextCentered(displayText, inputRect, m_font, (SDL_Color){120, 50, 75, 255});
        }

        // 设置输入法候选窗口位置
        SDL_SetTextInputRect(&inputRect);

        // 闪烁光标
        if ((now / 500) % 2 == 0) {
            int cursorX = inputX + inputW / 2;
            std::string displayForCursor = m_outfitNameInput + m_imeComposition;
            if (!displayForCursor.empty()) {
                int tw, th;
                TTF_SizeUTF8(m_font, displayForCursor.c_str(), &tw, &th);
                cursorX = inputX + (inputW + (int)(tw / m_dpiScale + 0.5f)) / 2 + 2;
            }
            SDL_SetRenderDrawColor(m_renderer, 210, 100, 130, 200);
            SDL_RenderDrawLine(m_renderer, cursorX, inputY + 8, cursorX, inputY + inputH - 8);
        }

        // 按钮行
        int btnW = 100, btnH2 = 32;
        int btnY2 = dlgY + dlgH - btnH2 - 18;
        int btnGap = 20;
        int totalBtnW = btnW * 2 + btnGap;
        int btnStartX = dlgX + (dlgW - totalBtnW) / 2;

        SDL_Rect cancelBtn = {btnStartX, btnY2, btnW, btnH2};
        DrawRoundedRect(cancelBtn, C_BTN_RESET, 8);
        DrawTextCentered("取消", cancelBtn, m_font, C_WHITE);

        SDL_Rect confirmBtn = {btnStartX + btnW + btnGap, btnY2, btnW, btnH2};
        DrawRoundedRect(confirmBtn, C_BTN_SAVE, 8);
        DrawTextCentered("确认保存", confirmBtn, m_font, C_WHITE);

    } else {
        // ======== 加载模式弹窗 ========
        int dlgW = 400, dlgH = 420;
        int dlgX = (m_width - dlgW) / 2;
        int dlgY = (m_height - dlgH) / 2;
        SDL_Rect dlgRect = {dlgX, dlgY, dlgW, dlgH};
        DrawRoundedRect(dlgRect, (SDL_Color){255, 255, 255, 255}, 12);
        DrawRoundedRectOutline(dlgRect, (SDL_Color){230, 200, 212, 255}, 12, 2);

        // 标题
        SDL_Rect titleRect = {dlgX, dlgY + 12, dlgW, 30};
        DrawTextCentered("加载装扮方案", titleRect, m_fontLarge, (SDL_Color){120, 50, 75, 255});

        // 输入框（手动输入编号或口令）+ 粘贴按钮
        int pasteBtnSize = 36;  // 粘贴按钮尺寸（正方形）
        int pasteBtnGap = 6;    // 输入框与按钮间距
        int inputW = 360 - pasteBtnSize - pasteBtnGap, inputH = 36;
        int inputX = dlgX + (dlgW - 360) / 2;
        int inputY = dlgY + 50;
        SDL_Rect inputRect = {inputX, inputY, inputW, inputH};
        DrawRoundedRect(inputRect, (SDL_Color){255, 248, 250, 255}, 8);
        DrawRoundedRectOutline(inputRect, (SDL_Color){220, 185, 198, 255}, 8, 1);

        // 粘贴按钮（正方形，中间三个横杠图标）
        int pasteBtnX = inputX + inputW + pasteBtnGap;
        int pasteBtnY = inputY + (inputH - pasteBtnSize) / 2;
        SDL_Rect pasteBtnRect = {pasteBtnX, pasteBtnY, pasteBtnSize, pasteBtnSize};
        {
            // 判断是否有剪贴板内容
            bool hasClip = SDL_HasClipboardText() == SDL_TRUE;
            SDL_Color pasteBtnBg = hasClip ? (SDL_Color){230, 190, 210, 255} : (SDL_Color){220, 210, 215, 255};
            DrawRoundedRect(pasteBtnRect, pasteBtnBg, 6);
            DrawRoundedRectOutline(pasteBtnRect, (SDL_Color){200, 170, 185, 255}, 6, 1);
            // 绘制三个横杠图标
            SDL_Color lineColor = hasClip ? (SDL_Color){100, 50, 75, 255} : (SDL_Color){170, 155, 165, 255};
            SDL_SetRenderDrawColor(m_renderer, lineColor.r, lineColor.g, lineColor.b, lineColor.a);
            int lineX1 = pasteBtnX + 9;
            int lineX2 = pasteBtnX + pasteBtnSize - 9;
            for (int li = 0; li < 3; li++) {
                int ly = pasteBtnY + 10 + li * 7;
                SDL_RenderDrawLine(m_renderer, lineX1, ly, lineX2, ly);
                SDL_RenderDrawLine(m_renderer, lineX1, ly + 1, lineX2, ly + 1);  // 加粗
            }
        }

        if (m_outfitInput.empty() && m_imeComposition.empty()) {
            DrawTextCentered("输入编号/口令，或点击下方选择", inputRect, m_fontSmall, (SDL_Color){195, 165, 178, 255});
        } else {
            // 显示时限制长度：如果文字超出输入框，只显示末尾部分并加省略号
            int maxTextW = inputW - 20; // 左右各留10px边距
            std::string displayInput = m_outfitInput + m_imeComposition;
            int tw, th;
            TTF_SizeUTF8(m_fontSmall, displayInput.c_str(), &tw, &th);
            if ((int)(tw / m_dpiScale + 0.5f) > maxTextW) {
                // 从末尾往前截取，找到能放下的最长子串
                std::string ellipsis = "...";
                int ellW, ellH;
                TTF_SizeUTF8(m_fontSmall, ellipsis.c_str(), &ellW, &ellH);
                int availW = maxTextW - (int)(ellW / m_dpiScale + 0.5f);
                // 从后往前逐字符（UTF-8安全）查找截断点
                size_t pos = displayInput.size();
                while (pos > 0) {
                    // 回退一个UTF-8字符
                    pos--;
                    while (pos > 0 && (displayInput[pos] & 0xC0) == 0x80) pos--;
                    std::string sub = displayInput.substr(pos);
                    TTF_SizeUTF8(m_fontSmall, sub.c_str(), &tw, &th);
                    if ((int)(tw / m_dpiScale + 0.5f) > availW) {
                        // 这个子串太长了，用上一次的截断点
                        // 前进一个UTF-8字符
                        pos++;
                        while (pos < displayInput.size() && (displayInput[pos] & 0xC0) == 0x80) pos++;
                        break;
                    }
                }
                displayInput = ellipsis + displayInput.substr(pos);
            }
            DrawTextCentered(displayInput, inputRect, m_fontSmall, (SDL_Color){120, 50, 75, 255});
        }

        // 设置输入法候选窗口位置
        if (m_outfitRenameIdx < 0) {
            SDL_SetTextInputRect(&inputRect);
        }

        // 闪烁光标
        if ((now / 500) % 2 == 0) {
            int cursorX = inputX + inputW / 2;
            std::string cursorText = m_outfitInput + m_imeComposition;
            if (!cursorText.empty()) {
                // 使用和显示相同的截断逻辑计算光标位置
                int maxTextW = inputW - 20;
                int tw, th;
                TTF_SizeUTF8(m_fontSmall, cursorText.c_str(), &tw, &th);
                if ((int)(tw / m_dpiScale + 0.5f) > maxTextW) {
                    // 文本被截断时，光标固定在右侧
                    cursorX = inputX + inputW / 2 + maxTextW / 2 + 2;
                } else {
                    cursorX = inputX + (inputW + (int)(tw / m_dpiScale + 0.5f)) / 2 + 2;
                }
            }
            SDL_SetRenderDrawColor(m_renderer, 210, 100, 130, 200);
            SDL_RenderDrawLine(m_renderer, cursorX, inputY + 8, cursorX, inputY + inputH - 8);
        }

        // ======== 已保存方案列表 ========
        int listMargin = 16;
        int listTop = inputY + inputH + 12;
        int listBottom = dlgY + dlgH - 64;
        int listH = listBottom - listTop;
        int listX = dlgX + listMargin;
        int listW = dlgW - listMargin * 2;

        SDL_Rect listBg = {listX, listTop, listW, listH};
        DrawRoundedRect(listBg, (SDL_Color){250, 245, 248, 255}, 8);
        DrawRoundedRectOutline(listBg, (SDL_Color){235, 215, 225, 255}, 8, 1);

        if (m_outfitEntries.empty()) {
            SDL_Rect emptyRect = {listX, listTop, listW, listH};
            DrawTextCentered("暂无已保存的方案", emptyRect, m_font, (SDL_Color){195, 165, 178, 255});
        } else {
            SDL_Rect clipRect = {listX, listTop, listW, listH};
            SDL_RenderSetClipRect(m_renderer, &clipRect);

            int itemH = 42;
            int itemPad = 4;
            int totalContentH = (int)m_outfitEntries.size() * (itemH + itemPad);

            int maxScroll = std::max(0, totalContentH - listH);
            if (m_outfitListScroll < 0) m_outfitListScroll = 0;
            if (m_outfitListScroll > maxScroll) m_outfitListScroll = maxScroll;

            for (int i = 0; i < (int)m_outfitEntries.size(); i++) {
                int iy = listTop + i * (itemH + itemPad) - m_outfitListScroll;
                if (iy + itemH < listTop || iy > listBottom) continue;

                const auto& entry = m_outfitEntries[i];
                SDL_Rect itemRect = {listX + 4, iy, listW - 8, itemH};
                bool selected = (i == m_outfitSelectedIdx);

                if (selected) {
                    DrawRoundedRect(itemRect, (SDL_Color){235, 190, 210, 255}, 8);
                } else {
                    DrawRoundedRect(itemRect, (SDL_Color){255, 252, 254, 255}, 8);
                }

                // 编号
                SDL_Color codeColor = selected ? (SDL_Color){120, 40, 70, 255} : (SDL_Color){100, 60, 80, 255};
                DrawText("#" + entry.code, itemRect.x + 12, iy + 2, m_fontLarge, codeColor);

                bool isRenaming = (i == m_outfitRenameIdx);

                if (isRenaming) {
                    // 重命名模式：显示输入框
                    int renameInputX = itemRect.x + 100;
                    int renameInputW = itemRect.w - 100 - 60;
                    int renameInputH = 24;
                    int renameInputY = iy + 2;
                    SDL_Rect renameRect = {renameInputX, renameInputY, renameInputW, renameInputH};
                    DrawRoundedRect(renameRect, (SDL_Color){255, 248, 250, 255}, 6);
                    DrawRoundedRectOutline(renameRect, (SDL_Color){210, 100, 130, 200}, 6, 1);

                    // 设置输入法候选窗口位置
                    SDL_SetTextInputRect(&renameRect);

                    std::string renameDisplay = m_outfitRenameInput + m_imeComposition;
                    if (renameDisplay.empty()) {
                        DrawTextCentered("输入新名称...", renameRect, m_fontSmall, (SDL_Color){195, 165, 178, 255});
                    } else {
                        // 左对齐显示
                        DrawText(renameDisplay, renameInputX + 6, renameInputY + 2, m_fontSmall, (SDL_Color){120, 50, 75, 255});
                    }

                    // 闪烁光标
                    if ((now / 500) % 2 == 0) {
                        int cursorX = renameInputX + 6;
                        if (!renameDisplay.empty()) {
                            int tw, th;
                            TTF_SizeUTF8(m_fontSmall, renameDisplay.c_str(), &tw, &th);
                            cursorX += (int)(tw / m_dpiScale + 0.5f);
                        }
                        SDL_SetRenderDrawColor(m_renderer, 210, 100, 130, 200);
                        SDL_RenderDrawLine(m_renderer, cursorX, renameInputY + 4, cursorX, renameInputY + renameInputH - 4);
                    }

                    // 确认按钮（√）
                    int confirmBtnW = 50, confirmBtnH = 22;
                    int confirmBtnX = itemRect.x + itemRect.w - confirmBtnW - 4;
                    int confirmBtnY = iy + 2;
                    SDL_Rect confirmBtnRect = {confirmBtnX, confirmBtnY, confirmBtnW, confirmBtnH};
                    DrawRoundedRect(confirmBtnRect, C_BTN_SAVE, 6);
                    DrawTextCentered("确认", confirmBtnRect, m_fontSmall, C_WHITE);
                } else {
                    // 方案名称（如果有）
                    if (!entry.name.empty()) {
                        DrawText(entry.name, itemRect.x + 110, iy + 4, m_font, (SDL_Color){90, 60, 75, 255});
                    }

                    // 右侧齿轮按钮（圆形，垂直居中）
                    int gearSize = 22;
                    int gearX = itemRect.x + itemRect.w - gearSize - 6;
                    int gearY = iy + (itemH - gearSize) / 2;
                    SDL_Rect gearRect = {gearX, gearY, gearSize, gearSize};
                    // 齿轮按钮背景（圆形）
                    bool isMenuOpen = (m_outfitMenuIdx == i);
                    SDL_Color gearBg = isMenuOpen ? (SDL_Color){180, 155, 170, 255} : (SDL_Color){220, 200, 212, 255};
                    DrawRoundedRect(gearRect, gearBg, gearSize / 2);

                    // 绘制齿轮图标（用简单的点和线模拟）
                    int cx = gearX + gearSize / 2;
                    int cy = gearY + gearSize / 2;
                    SDL_Color gearColor = {100, 65, 85, 255};
                    SDL_SetRenderDrawColor(m_renderer, gearColor.r, gearColor.g, gearColor.b, gearColor.a);
                    // 中心圆点
                    for (int dx = -2; dx <= 2; dx++) {
                        for (int dy = -2; dy <= 2; dy++) {
                            if (dx*dx + dy*dy <= 5) {
                                SDL_RenderDrawPoint(m_renderer, cx + dx, cy + dy);
                            }
                        }
                    }
                    // 齿轮齿（6个方向的短线）
                    const int teethR1 = 5, teethR2 = 8;
                    for (int t = 0; t < 6; t++) {
                        float angle = t * 3.14159f / 3.0f;
                        int x1 = cx + (int)(teethR1 * cosf(angle));
                        int y1 = cy + (int)(teethR1 * sinf(angle));
                        int x2 = cx + (int)(teethR2 * cosf(angle));
                        int y2 = cy + (int)(teethR2 * sinf(angle));
                        SDL_RenderDrawLine(m_renderer, x1, y1, x2, y2);
                        // 加粗
                        SDL_RenderDrawLine(m_renderer, x1+1, y1, x2+1, y2);
                        SDL_RenderDrawLine(m_renderer, x1, y1+1, x2, y2+1);
                    }
                }

                // 人模名称
                const char* mName = (entry.model >= 0 && entry.model <= 2) ? modelNames[entry.model] : "未知";
                DrawText(mName, itemRect.x + 12, iy + 24, m_fontSmall, (SDL_Color){170, 130, 150, 255});

                // 时尚值
                std::string valStr = "时尚值:" + std::to_string(entry.fashionValue);
                DrawText(valStr, itemRect.x + 80, iy + 24, m_fontSmall, (SDL_Color){170, 130, 150, 255});

                // 底部分隔线
                if (!selected && i < (int)m_outfitEntries.size() - 1) {
                    SDL_SetRenderDrawColor(m_renderer, 240, 225, 232, 255);
                    SDL_RenderDrawLine(m_renderer, itemRect.x + 8, iy + itemH + 1,
                                       itemRect.x + itemRect.w - 8, iy + itemH + 1);
                }
            }

            SDL_RenderSetClipRect(m_renderer, nullptr);

            // 绘制齿轮弹出菜单（在裁剪区域外绘制，确保不被裁剪）
            if (m_outfitMenuIdx >= 0 && m_outfitMenuIdx < (int)m_outfitEntries.size()) {
                auto& menuEntry = m_outfitEntries[m_outfitMenuIdx];
                int menuIy = listTop + m_outfitMenuIdx * (itemH + itemPad) - m_outfitListScroll;
                int gearSize = 22;
                int menuAnchorX = listX + listW - 8 - gearSize - 6 + gearSize;
                int menuAnchorY = menuIy + (itemH + gearSize) / 2;

                // 菜单项
                int menuItemW = 90, menuItemH = 28, menuPad = 2;
                bool hasToken = !menuEntry.token.empty();
                int menuItemCount = hasToken ? 3 : 2;  // 命名、删除 [、复制口令]
                int menuW = menuItemW + menuPad * 2;
                int menuH = menuItemCount * menuItemH + (menuItemCount - 1) * 2 + menuPad * 2;

                // 菜单位置：齿轮按钮左侧
                int menuX = menuAnchorX - menuW - 4;
                int menuY = menuAnchorY;

                // 确保菜单不超出弹窗底部
                if (menuY + menuH > dlgY + dlgH - 50) {
                    menuY = menuAnchorY - menuH;
                }

                // 菜单阴影
                SDL_Rect shadowRect = {menuX + 2, menuY + 2, menuW, menuH};
                DrawRoundedRect(shadowRect, (SDL_Color){0, 0, 0, 30}, 8);

                // 菜单背景
                SDL_Rect menuBg = {menuX, menuY, menuW, menuH};
                DrawRoundedRect(menuBg, (SDL_Color){255, 252, 254, 255}, 8);
                DrawRoundedRectOutline(menuBg, (SDL_Color){220, 200, 215, 255}, 8, 1);

                // 菜单项绘制
                const char* menuLabels[] = {"命名", "删除", "复制口令"};
                SDL_Color menuColors[] = {
                    {100, 65, 85, 255},    // 命名 - 粉紫色
                    {200, 80, 80, 255},    // 删除 - 红色
                    {70, 100, 140, 255}    // 复制口令 - 蓝色
                };
                for (int mi = 0; mi < menuItemCount; mi++) {
                    int itemY = menuY + menuPad + mi * (menuItemH + 2);
                    SDL_Rect miRect = {menuX + menuPad, itemY, menuItemW, menuItemH};

                    // 鼠标悬停高亮（通过当前鼠标位置判断）
                    int curMx, curMy;
                    SDL_GetMouseState(&curMx, &curMy);
                    WindowToLogical(curMx, curMy, curMx, curMy);
                    if (curMx >= miRect.x && curMx < miRect.x + miRect.w
                        && curMy >= miRect.y && curMy < miRect.y + miRect.h) {
                        DrawRoundedRect(miRect, (SDL_Color){240, 228, 235, 255}, 6);
                    }

                    DrawTextCentered(menuLabels[mi], miRect, m_fontSmall, menuColors[mi]);

                    // 分隔线
                    if (mi < menuItemCount - 1) {
                        int lineY = itemY + menuItemH + 1;
                        SDL_SetRenderDrawColor(m_renderer, 235, 220, 228, 200);
                        SDL_RenderDrawLine(m_renderer, menuX + 8, lineY, menuX + menuW - 8, lineY);
                    }
                }
            }

            if (totalContentH > listH) {
                float scrollRatio = (float)m_outfitListScroll / (float)maxScroll;
                int barH = std::max(20, listH * listH / totalContentH);
                int barY = listTop + (int)(scrollRatio * (listH - barH));
                SDL_Rect scrollBar = {listX + listW - 6, barY, 4, barH};
                DrawRoundedRect(scrollBar, (SDL_Color){210, 185, 198, 150}, 2);
            }
        }

        // 按钮行
        int btnW = 100, btnH2 = 32;
        int btnY2 = dlgY + dlgH - btnH2 - 16;
        int btnGap = 20;
        int totalBtnW = btnW * 2 + btnGap;
        int btnStartX = dlgX + (dlgW - totalBtnW) / 2;

        SDL_Rect cancelBtn = {btnStartX, btnY2, btnW, btnH2};
        DrawRoundedRect(cancelBtn, C_BTN_RESET, 8);
        DrawTextCentered("取消", cancelBtn, m_font, C_WHITE);

        SDL_Rect confirmBtn = {btnStartX + btnW + btnGap, btnY2, btnW, btnH2};
        bool hasSelection = (m_outfitSelectedIdx >= 0 && m_outfitSelectedIdx < (int)m_outfitEntries.size()) || !m_outfitInput.empty();
        SDL_Color confirmColor = hasSelection ? C_BTN_SAVE : (SDL_Color){200, 180, 190, 255};
        DrawRoundedRect(confirmBtn, confirmColor, 8);
        DrawTextCentered("确认加载", confirmBtn, m_font, C_WHITE);
    }
}

// ==================== 截图保存与分享 ====================

std::string Game::CaptureAvatarToFile() {
    // 创建一个离屏渲染目标，只包含角色部分
    int curRightW = GetCurrentRightPanelW();
    int centerW = m_width - LEFT_PANEL_W - curRightW;
    int captureW = std::min(centerW, 900);
    int captureH = std::min(m_height, 1000);

    // 创建离屏纹理（Android OpenGL ES 上优先使用 ARGB8888）
    Uint32 pixelFmt = SDL_PIXELFORMAT_ARGB8888;
    SDL_Texture* target = SDL_CreateTexture(m_renderer, pixelFmt,
                                             SDL_TEXTUREACCESS_TARGET, captureW, captureH);
    if (!target) {
        // 尝试回退到 RGBA8888
        pixelFmt = SDL_PIXELFORMAT_RGBA8888;
        target = SDL_CreateTexture(m_renderer, pixelFmt,
                                    SDL_TEXTUREACCESS_TARGET, captureW, captureH);
    }
    if (!target) {
        std::cerr << "创建截图纹理失败: " << SDL_GetError() << std::endl;
        return "";
    }

    // 设置渲染目标为离屏纹理
    if (SDL_SetRenderTarget(m_renderer, target) != 0) {
        std::cerr << "设置渲染目标失败: " << SDL_GetError() << std::endl;
        SDL_DestroyTexture(target);
        return "";
    }

    // 清除为浅粉白背景
    SDL_SetRenderDrawColor(m_renderer, C_CENTER_BG.r, C_CENTER_BG.g, C_CENTER_BG.b, 255);
    SDL_RenderClear(m_renderer);

    // 渲染角色到离屏纹理中央（往上移留出底部空间）
    int avatarCenterX = captureW / 2;
    int avatarBottomY = captureH - 100;
    m_avatar->Render(m_renderer, avatarCenterX, avatarBottomY);

    // 读取像素数据
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, captureW, captureH, 32, pixelFmt);
    if (!surface) {
        SDL_SetRenderTarget(m_renderer, nullptr);
        SDL_DestroyTexture(target);
        return "";
    }
    SDL_RenderReadPixels(m_renderer, nullptr, pixelFmt, surface->pixels, surface->pitch);

    // 恢复渲染目标
    SDL_SetRenderTarget(m_renderer, nullptr);
    SDL_DestroyTexture(target);

    // 生成文件名
    time_t now = time(nullptr);
    struct tm* lt = localtime(&now);
    char timeStr[64];
    strftime(timeStr, sizeof(timeStr), "%Y%m%d_%H%M%S", lt);

    std::string filename;

#ifdef ANDROID_PLATFORM
    // Android: 保存到内部存储
    std::string dir = AndroidPlatform::GetInternalStoragePath() + "screenshots/";
    AndroidPlatform::CreateDirectories(dir);
    filename = dir + "AolaNuanNuan_" + std::string(timeStr) + ".png";
#else
    // 桌面端: 保存到当前目录下的 screenshots 子目录
    std::string dir = "screenshots";
    if (!fs::exists(dir)) fs::create_directories(dir);
    filename = dir + "/AolaNuanNuan_" + std::string(timeStr) + ".png";
#endif

    // 保存 PNG
    int ret = IMG_SavePNG(surface, filename.c_str());
    SDL_FreeSurface(surface);

    if (ret != 0) {
        std::cerr << "保存截图失败: " << IMG_GetError() << std::endl;
#ifdef ANDROID_PLATFORM
        __android_log_print(ANDROID_LOG_ERROR, "AolaNuanNuan",
            "IMG_SavePNG 失败: %s -> %s", filename.c_str(), IMG_GetError());
#endif
        return "";
    }
#ifdef ANDROID_PLATFORM
    __android_log_print(ANDROID_LOG_INFO, "AolaNuanNuan",
        "截图已保存到: %s", filename.c_str());
#endif

    std::cout << "截图已保存: " << filename << std::endl;
    return filename;
}

void Game::SaveScreenshot() {
    std::string path = CaptureAvatarToFile();
    if (path.empty()) {
        m_outfitMsg = "截图保存失败";
        m_outfitMsgTime = SDL_GetTicks();
        return;
    }

#ifdef ANDROID_PLATFORM
    // Android: 通过 JNI 保存到系统相册
    JNIEnv* env = (JNIEnv*)SDL_AndroidGetJNIEnv();
    if (env) {
        jclass clazz = env->FindClass("com/aola/nuannuan/AolaNuanNuanActivity");
        if (clazz) {
            jmethodID mid = env->GetStaticMethodID(clazz, "saveToGallery", "(Ljava/lang/String;)Z");
            if (mid) {
                jstring jPath = env->NewStringUTF(path.c_str());
                jboolean success = env->CallStaticBooleanMethod(clazz, mid, jPath);
                env->DeleteLocalRef(jPath);
                if (success) {
                    m_outfitMsg = "已保存到相册 📷";
                } else {
                    m_outfitMsg = "保存到相册失败";
                }
            } else {
                env->ExceptionClear();
                m_outfitMsg = "截图已保存到: " + path;
            }
            env->DeleteLocalRef(clazz);
        } else {
            env->ExceptionClear();
            m_outfitMsg = "截图已保存到: " + path;
        }
    }
#else
    m_outfitMsg = "截图已保存 📷";
    // 打开文件管理器高亮显示
    OpenFileInExplorer(path);
#endif

    m_outfitMsgTime = SDL_GetTicks();
}

void Game::ShareScreenshot() {
    std::string path = CaptureAvatarToFile();
    if (path.empty()) {
        m_outfitMsg = "截图失败，无法分享";
        m_outfitMsgTime = SDL_GetTicks();
        return;
    }

#ifdef ANDROID_PLATFORM
    // Android: 通过 JNI 调用系统分享
    JNIEnv* env = (JNIEnv*)SDL_AndroidGetJNIEnv();
    if (env) {
        jclass clazz = env->FindClass("com/aola/nuannuan/AolaNuanNuanActivity");
        if (clazz) {
            jmethodID mid = env->GetStaticMethodID(clazz, "shareImage", "(Ljava/lang/String;)V");
            if (mid) {
                jstring jPath = env->NewStringUTF(path.c_str());
                env->CallStaticVoidMethod(clazz, mid, jPath);
                env->DeleteLocalRef(jPath);
                m_outfitMsg = "正在打开分享...";
            } else {
                env->ExceptionClear();
                m_outfitMsg = "分享功能暂不可用";
            }
            env->DeleteLocalRef(clazz);
        } else {
            env->ExceptionClear();
            m_outfitMsg = "分享功能暂不可用";
        }
    }
#elif __APPLE__
    // macOS: 打开文件管理器，提示用户手动分享
    OpenFileInExplorer(path);
    m_outfitMsg = "截图已保存，请从文件管理器分享 📤";
#elif _WIN32
    // Windows: 打开文件管理器
    OpenFileInExplorer(path);
    m_outfitMsg = "截图已保存，请从文件管理器分享 📤";
#else
    m_outfitMsg = "截图已保存: " + path;
#endif

    m_outfitMsgTime = SDL_GetTicks();
}

void Game::OpenFileInExplorer(const std::string& path) {
#ifndef ANDROID_PLATFORM
    std::string absPath = fs::absolute(path).string();
#ifdef __APPLE__
    // macOS: 在 Finder 中高亮显示文件
    std::string cmd = "open -R \"" + absPath + "\"";
    system(cmd.c_str());
#elif _WIN32
    // Windows: 在资源管理器中高亮显示文件
    std::string cmd = "explorer /select,\"" + absPath + "\"";
    system(cmd.c_str());
#else
    // Linux: 打开文件所在目录
    std::string dir = fs::path(absPath).parent_path().string();
    std::string cmd = "xdg-open \"" + dir + "\"";
    system(cmd.c_str());
#endif
#endif
}

