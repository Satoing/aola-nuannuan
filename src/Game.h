// Game.h - 换装游戏主类（参考奥拉星 UI 设计）
#pragma once
#include "AvatarRenderer.h"
#include "ResourceManager.h"
#include "ClothDefine.h"
#include <SDL.h>
#include <SDL_ttf.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <thread>
#include <mutex>
#include <queue>
#include <functional>
#include <sqlite3.h>

#include <SDL_image.h>  // IMG_SavePNG

// 服装列表项
struct ClothItem {
    int clothID;
    std::string name;
    int clothType;
    int clothVal;      // 时尚值
};

// 套装列表项（用于左侧套装分类列表显示）
struct SuitItem {
    int suitId;
    std::string name;
    std::vector<int> clothesList;  // 包含的服装ID列表
    int totalVal;                  // 套装总时尚值
    int thumbClothID;              // 用于显示缩略图的服装ID（套装第一件）
};

class Game {
public:
    Game();
    ~Game();

    bool Init(const std::string& title, int width, int height);
    void Run();
    void Cleanup();

private:
    // 核心组件
    SDL_Window* m_window;
    SDL_Renderer* m_renderer;
    TTF_Font* m_fontLarge;     // 分类标题大字
    TTF_Font* m_font;          // 正常字体
    TTF_Font* m_fontSmall;     // 小字体（拼音/标签）
    TTF_Font* m_fontTiny;      // 更小字体
    ResourceManager* m_resMgr;
    AvatarRenderer* m_avatar;

    // 窗口尺寸
    int m_width, m_height;
    bool m_running;

    // UI 状态
    int m_currentCategory;     // 当前选中的服装分类
    int m_gridScrollOffset;    // 右侧网格滚动偏移
    int m_catScrollOffset;     // 左侧分类滚动偏移
    int m_model;               // 当前人模 (0/1/2)

    // 搜索
    std::string m_searchText;
    bool m_searchActive;

    // 分类数据
    std::unordered_map<int, std::vector<ClothItem>> m_categoryItems;
    std::vector<SuitItem> m_suitItems;  // 套装列表

    // ======== 三栏布局常量 ========
    static const int LEFT_PANEL_W = 150;    // 左侧分类栏宽度
    static const int RIGHT_PANEL_W = 440;   // 右侧服装面板宽度
    // 中间角色展示区宽度 = m_width - LEFT_PANEL_W - RIGHT_PANEL_W

    static const int CAT_ITEM_H = 72;       // 分类项高度
    static const int GRID_COLS = 4;          // 网格列数
    static const int GRID_CELL_SIZE = 100;   // 网格单元格大小
    static const int GRID_PADDING = 8;       // 网格间距
    static const int GRID_NAME_H = 28;        // 网格名称区域高度
    static const int CARD_RADIUS = 14;        // 卡片圆角半径
    static const int SEARCH_BAR_H = 50;      // 搜索栏高度
    static const int BOTTOM_BAR_H = 60;      // 底部状态栏高度
    static const int ACCENT_BAR_W = 4;       // 选中指示条宽度

    // 默认底色服装
    std::vector<int> m_defaultClothes;

    // ======== 装扮方案保存/加载（SQLite） ========
    sqlite3* m_outfitDB;            // SQLite 数据库连接
    static constexpr const char* OUTFIT_DB = "outfits.db";
    void InitOutfitDB();            // 初始化数据库（建表、迁移旧数据）
    void CloseOutfitDB();           // 关闭数据库
    void MigrateFromJSON();         // 从旧的JSON文件迁移数据到SQLite

    bool m_outfitDialogActive;      // 是否显示编号输入弹窗
    std::string m_outfitInput;      // 编号输入内容
    std::string m_outfitMsg;        // 提示信息（保存成功/加载失败等）
    Uint32 m_outfitMsgTime;         // 提示信息显示时间戳
    bool m_outfitDialogIsLoad;      // true=加载模式, false=保存完成展示编号
    std::string m_outfitSavedToken; // 保存完成后的口令（用于展示）
    std::string m_outfitSavedCode;  // 保存完成后的编号（用于展示）
    bool m_outfitShowToken;         // 是否正在展示保存结果（口令）

    // 已保存方案列表（用于加载弹窗预览）
    struct OutfitEntry {
        std::string code;
        std::string name;  // 方案名称
        std::string token; // 分享口令
        int model;
        int fashionValue;
    };
    std::vector<OutfitEntry> m_outfitEntries;  // 已保存方案列表
    int m_outfitSelectedIdx;                   // 当前选中的方案索引（-1=未选中）
    int m_outfitListScroll;                    // 方案列表滚动偏移
    void LoadOutfitList();                     // 从数据库读取已保存方案列表

    // 重命名状态
    int m_outfitRenameIdx;                     // 正在重命名的方案索引（-1=未重命名）
    std::string m_outfitRenameInput;           // 重命名输入内容

    // IME输入法组合文本状态
    std::string m_imeComposition;              // 正在输入的组合文本（如拼音）
    int m_imeCursor;                           // 组合文本中的光标位置
    int m_imeSelLen;                           // 组合文本中的选中长度
    void RenameOutfit(const std::string& code, const std::string& newName);  // 重命名方案

    // 齿轮菜单状态
    int m_outfitMenuIdx;                       // 弹出菜单对应的方案索引（-1=未打开）
    void DeleteOutfit(const std::string& code); // 删除方案

    // 缩略图缓存
    std::unordered_map<int, SDL_Texture*> m_thumbCache;
    std::unordered_set<int> m_thumbLoading;  // 正在加载中的缩略图
    std::mutex m_thumbMutex;

    // 待在主线程创建纹理的下载数据
    struct ThumbData {
        int clothID;
        std::vector<uint8_t> data;
    };
    std::queue<ThumbData> m_thumbPending;
    std::mutex m_thumbPendingMutex;

    // HiDPI 缩放比例
    float m_dpiScale;

    // 设计分辨率（逻辑坐标始终基于此尺寸）
    int m_designWidth;
    int m_designHeight;

    // 方法
    void HandleEvents();
    void Update();
    void Render();
    void OnWindowResize(int newW, int newH);
    void ReloadFonts();

    void BuildCategoryData();
    int MapClothTypeToCat(int clothType);

    // 三栏绘制
    void DrawLeftPanel();       // 左侧分类栏
    void DrawCenterPanel();     // 中间角色展示
    void DrawRightPanel();      // 右侧服装面板
    void DrawSearchBar();       // 搜索栏
    void DrawClothGrid();       // 服装网格
    void DrawBottomBar();       // 底部状态栏

    // 绘制辅助
    void DrawRoundedRect(const SDL_Rect& rect, SDL_Color color, int radius);
    void DrawRoundedRectOutline(const SDL_Rect& rect, SDL_Color color, int radius, int thickness = 2);
    void FillRect(const SDL_Rect& rect, SDL_Color color);
    void DrawText(const std::string& text, int x, int y, TTF_Font* font, SDL_Color color);
    void DrawTextCentered(const std::string& text, const SDL_Rect& rect, TTF_Font* font, SDL_Color color);
    SDL_Texture* RenderText(const std::string& text, TTF_Font* font, SDL_Color color);

    // 将窗口坐标转换为逻辑坐标（解决缩放/DPI下鼠标偏移）
    void WindowToLogical(int windowX, int windowY, int& logicalX, int& logicalY);

    // 获取过滤后的当前分类服装列表
    const std::vector<ClothItem>& GetCurrentItems() const;
    std::vector<ClothItem> GetFilteredItems() const;
    std::vector<SuitItem> GetFilteredSuits() const;  // 获取过滤后的套装列表

    // 缩略图
    std::string GetThumbURL(int clothID);
    SDL_Texture* GetThumbTexture(int clothID);
    void LoadThumbAsync(int clothID);

    // 套装一键穿戴
    void WearSuit(const SuitItem& suit);

    // 根据人模类型设置默认服装和挂载点映射
    void ApplyModelDefaults(int model);

    // 人模切换动画
    float m_modelSwitchAnimX;    // 选中滑块的当前动画X位置
    float m_modelSwitchTargetX;  // 选中滑块的目标X位置

    // 右侧面板收起/展开
    bool m_rightPanelCollapsed;      // 是否收起右侧面板
    float m_rightPanelAnimProgress;  // 动画进度 0.0=展开 1.0=收起

    // 装扮方案保存/加载
    std::string SaveOutfit(const std::string& name = "");  // 保存当前装扮（带名称），返回编号
    bool LoadOutfit(const std::string& code);              // 通过编号加载装扮
    std::string m_outfitNameInput;                         // 保存模式：方案名称输入
    void DrawOutfitDialog();                          // 绘制编号输入弹窗
    std::string GenerateOutfitCode(int id);           // 生成6位编号字符串

    // 口令加密/解密
    std::string EncodeOutfitToken(int model, const std::vector<int>& clothes);  // 编码当前搭配为口令
    bool DecodeOutfitToken(const std::string& token, int& outModel, std::vector<int>& outClothes);  // 解码口令
    bool LoadOutfitFromToken(const std::string& token);  // 从口令加载搭配

    // ======== 截图保存与分享 ========
    void SaveScreenshot();                            // 截图保存到本地
    void ShareScreenshot();                           // 分享截图到社交平台
    std::string CaptureAvatarToFile();               // 将角色渲染为PNG文件，返回文件路径
    void OpenFileInExplorer(const std::string& path); // 在系统文件管理器中打开（桌面端）

    // 获取右侧面板当前实际宽度（动画中间值）
    int GetCurrentRightPanelW() const;
    void DrawCollapseToggle();  // 绘制收起/展开切换按钮

    // ======== 触摸滚动与拖拽/点击区分（Android） ========
    bool m_touchActive;              // 是否有活跃的触摸
    float m_touchStartX, m_touchStartY;  // 触摸起始位置（逻辑坐标）
    float m_touchLastY;              // 上一帧触摸Y位置（用于计算滚动增量）
    bool m_touchIsDrag;              // 是否已判定为拖拽（移动距离超过阈值）
    int m_touchScrollTarget;         // 触摸滚动目标区域：0=无，1=右侧网格，2=左侧分类，3=弹窗列表
    static const int TOUCH_DRAG_THRESHOLD = 10;  // 判定为拖拽的最小像素距离
    float m_scrollVelocity;          // 惯性滚动速度
    Uint32 m_touchStartTime;         // 触摸开始时间戳
};
