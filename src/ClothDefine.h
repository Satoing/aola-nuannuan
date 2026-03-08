// ClothDefine.h - 服装数据结构定义
#pragma once
#include <string>
#include <vector>

// 服装部件
struct ClothPart {
    std::string clothPartFileName;  // 部件文件名，如 "a0080530"
    int clothesPos;                  // 挂载点位置，决定绘制层级（Z-order）
    bool forHide = false;            // 是否为隐藏标记部件（不实际渲染，只用于标记隐藏位置）
};

// 身体隐藏标志位（对应前端 BodyHideParts）
enum BodyHideFlags {
    HIDE_NULL       = 0,
    HIDE_LEFT_HAND  = 1 << 0,  // 隐藏左手（后手）
    HIDE_RIGHT_HAND = 1 << 1,  // 隐藏右手（前手）
    HIDE_UP_BODY    = 1 << 2,  // 隐藏上半身
    HIDE_LEG        = 1 << 3,  // 隐藏腿部
    HIDE_ALL_BODY   = HIDE_LEFT_HAND | HIDE_RIGHT_HAND | HIDE_LEG | HIDE_UP_BODY
};

// 服装
struct Cloth {
    int clothID;
    std::string clothName;
    int clothType;          // 服装品类（类型）
    int clothVal;           // 时尚值
    int classify;           // 服装分类，决定 model 后缀
    std::string fileName;   // 资源文件名前缀，如 "a008053"
    std::vector<ClothPart> clothParts;
};

// 服装分类常量（对应前端 ClothesClassify）
enum ClothClassify {
    CLASSIFY_OLD = 0,           // 旧人模
    CLASSIFY_NEW_BOY = 1,       // 新男模
    CLASSIFY_NEW_GIRL = 2,      // 新女模
    CLASSIFY_NEW_ALL = 3,       // 新人模通用（后缀_0）
    CLASSIFY_OLD_BG = 4,        // 旧背景
    CLASSIFY_NEW_BG = 5,        // 新背景
    CLASSIFY_ALL = 6,           // 三模通用（分 _0, _1, _2）
    CLASSIFY_OLD_AND_BOY = 7,   // 旧+男
    CLASSIFY_OLD_AND_GIRL = 8,  // 旧+女
    CLASSIFY_NEW_ALL_SPECIAL = 9 // 新人模专属（分 _1, _2）
};

// 服装品类（用于 UI 分类标签）
enum ClothCategory {
    CAT_SUIT = 0,     // 套装（放在第一栏）
    CAT_SKIN,         // 底色/皮肤
    CAT_HAIR,         // 头发
    CAT_FACE,         // 面部（表情/面妆）
    CAT_HEAD_DECO,    // 头饰
    CAT_TOP,          // 上衣
    CAT_BOTTOM,       // 下装（裙/裤）
    CAT_SHOES,        // 鞋子
    CAT_ACCESSORY,    // 配饰（颈饰/耳饰/腰饰等）
    CAT_WINGS,        // 翅膀/背饰
    CAT_HANDHELD,     // 手持物
    CAT_BACKGROUND,   // 背景
    CAT_COUNT
};

// 套装数据结构
struct ClothesSuit {
    int suitId;
    std::string suitName;
    std::vector<int> clothesList;  // 包含的服装ID列表
};

// 分类显示信息（中文名 + 拼音）
struct CategoryInfo {
    const char* name;
    const char* pinyin;
};

// 获取分类显示信息
inline CategoryInfo GetCategoryInfo(int cat) {
    static CategoryInfo infos[] = {
        {"套装",   "TAO ZHUANG"},
        {"底色",   "DI SE"},
        {"头发",   "TOU FA"},
        {"面部",   "MIAN BU"},
        {"头饰",   "TOU SHI"},
        {"上衣",   "SHANG YI"},
        {"下装",   "XIA ZHUANG"},
        {"鞋子",   "XIE ZI"},
        {"配饰",   "PEI SHI"},
        {"翅膀",   "CHI BANG"},
        {"手持",   "SHOU CHI"},
        {"背景",   "BEI JING"},
    };
    if (cat >= 0 && cat < CAT_COUNT) return infos[cat];
    return {"其他", "QI TA"};
}

// 根据 clothType 获取类别名称
inline const char* GetCategoryName(int cat) {
    return GetCategoryInfo(cat).name;
}

// 分类显示顺序（底色放到最下面，背景之前）
inline const int* GetCategoryDisplayOrder() {
    static const int order[] = {
        CAT_SUIT, CAT_HAIR, CAT_FACE, CAT_HEAD_DECO,
        CAT_TOP, CAT_BOTTOM, CAT_SHOES, CAT_ACCESSORY,
        CAT_WINGS, CAT_HANDHELD, CAT_SKIN, CAT_BACKGROUND
    };
    return order;
}
