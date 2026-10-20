#pragma once
#include <QString>
#include <QJsonArray>
#include <QJsonObject>
#include <vector>
#include "Element.h"
#include "Vec.h"

// ============================================================================
// 块（Block）子系统 —— 参考 AutoCAD 的块 / 动态块 / 属性块机制
// ----------------------------------------------------------------------------
// 数据模型采用「引用式」（与图层的"快照式"相反）：
//
//   BlockDef   块定义 —— 块名 + 基点 + 块内图元（块坐标系）+ 属性定义 + 动态参数
//   Element    块引用 —— ElementType::BlockRef，只存：块名 + 插入点/旋转/缩放
//                        + 属性值(blockAttrs) + 动态参数值(dynValues)
//
// 关键结论：块内几何**不复制**，所有引用共享同一份 BlockDef，渲染/命中/导出时
// 按定义实时展开（block::expandRef）。因此「修改块内图元 → 所有引用同步更新」
// 不需要任何反向推送逻辑，天然成立（redefineBlock 只改一份定义）。
//
// 命名空间约定：**数据模型**（BlockDef 等）放在 SketchCAD 顶层，便于 SketchModel /
// Commands / MainWindow 直接使用；**纯算法**放在 SketchCAD::block。
// ============================================================================
namespace SketchCAD {

// ---- 属性定义（ATTDEF，属性块的"字段声明"）----
struct AttributeDef {
    QString tag;              // 属性标签（块内唯一，提取/编辑的键）
    QString prompt;           // 插入时提示语
    QString defaultValue;     // 默认值
    Vec3    pos;              // 块坐标系中的插入位置（文本锚点）
    double  height = 3.5;     // 文字高度(mm)
    bool    constant = false; // 常量属性：不可编辑，恒为 defaultValue
    bool    invisible = false;// 不可见属性：展开时不产生可见文字（仅参与提取）
    int     align = 1;        // 文字对齐（Element::textAlign 9 点；DXF ATTDEF 缺省 72/73 → 6 左下）
    QString styleName;        // 文字样式名（随 ATTDEF 的 7 组码，展开时赋给属性文字）
};

// ---- 动态参数类型 ----
enum class DynParamType {
    Visibility    = 0,   // 可见性：命名状态切换块内图元显隐
    LinearStretch = 1,   // 线性拉伸：沿方向按距离拉伸（仅影响 affected 中、且在"可动侧"的顶点）
    Rotate        = 2,   // 旋转：绕 base 旋转 value 度（仅 affected）
    Flip          = 3    // 翻转：关于过 base、方向为 direction 的轴镜像（仅 affected）
};
inline const char* dynParamTypeName(DynParamType t) {
    switch (t) {
        case DynParamType::Visibility:    return "可见性";
        case DynParamType::LinearStretch: return "线性拉伸";
        case DynParamType::Rotate:        return "旋转";
        case DynParamType::Flip:          return "翻转";
    }
    return "参数";
}

// ---- 可见性状态：一个命名状态 + 该状态下被隐藏的块内图元索引 ----
struct VisibilityState {
    QString name;                     // 状态名（如 "默认" / "展开"）
    std::vector<int> hiddenEntities;  // 本状态下隐藏的块内图元索引（其余可见）
};

// ---- 动态参数定义 ----
struct DynParam {
    QString name;                          // 参数名（实例按名赋值）
    DynParamType type = DynParamType::Visibility;
    std::vector<int> affected;             // 影响的块内图元索引（拉伸/旋转/翻转用）
    Vec3   base      = Vec3(0, 0, 0);      // 基点（拉伸固定端 / 旋转中心 / 翻转轴上一点）
    Vec3   direction = Vec3(1, 0, 0);      // 方向（拉伸方向 / 翻转轴方向）
    double defaultValue = 0.0;             // 默认值（拉伸 mm / 旋转 度 / 翻转 0|1）
    double minValue = 0.0, maxValue = 0.0; // 值域；maxValue<=minValue 表示不限制
    std::vector<VisibilityState> states;   // 仅可见性参数使用
};

// ---- 块定义 ----
struct BlockDef {
    QString name;                         // 块名（唯一键）
    QString description;
    Vec3    basePoint = Vec3(0, 0, 0);    // 基点（插入时与插入点对齐）
    std::vector<Element> entities;        // 块内图元（块坐标系，绝对坐标）
    std::vector<AttributeDef> attdefs;    // 属性定义
    std::vector<DynParam> dynParams;      // 动态参数定义
    bool    annotative = false;           // 注释性（占位，暂不参与展开）

    // ---- 外部参照（Xref）属性 ----
    // 外部参照在本模型中同样占用一条块表记录（AutoCAD 语义：Xref 是 Block 表的一类记录），
    // 用下面几个字段把它与普通块区分开。路径类型字段用 int 而非 XrefPathType 枚举，
    // 以避免 Block.h ↔ Xref.h 循环包含（与 Element::sectionExempt/sectionMaterial 同手法）：
    // 具体枚举见 Xref.h，此处仅存整数，使用时按枚举解释。
    // 旧存档缺这些键时全部回落默认（isXref=false），语义与普通块完全一致。
    bool    isXref       = false;         // 是否为外部参照记录
    QString xrefPath;                     // 源文件路径（按 xrefPathType 的形态存储）
    int     xrefPathType = 0;             // 0 == XrefPathType::FullPath（见 Xref.h）
    bool    xrefNested   = false;         // 嵌套参照（随宿主参照带入），不可在本图直接分离
    int     xrefStatus   = 0;             // 0 == XrefStatus::Loaded（见 Xref.h）
};

// ============================================================================
// 2D 仿射变换（块展开的核心）
// ----------------------------------------------------------------------------
// 点：p' = M·p + t                    （M 为 2×2 线性部分）
// 「this 先、outer 后」的复合用 then(outer)：嵌套块每层左乘父变换即得整体变换，
// 无需为不同嵌套深度写不同公式。
// ============================================================================
struct X2D {
    double m00 = 1, m01 = 0, m10 = 0, m11 = 1;   // 线性部分
    Vec3   t   = Vec3(0, 0, 0);                  // 平移

    Vec3 apply(const Vec3& p) const {
        return Vec3(m00 * p.x + m01 * p.y + t.x,
                    m10 * p.x + m11 * p.y + t.y, p.z);
    }
    // 方向向量：只作用线性部分，不含平移
    Vec3 applyDir(const Vec3& d) const {
        return Vec3(m00 * d.x + m01 * d.y, m10 * d.x + m11 * d.y, 0.0);
    }
    // 角度（度）经线性部分映射后的新角度
    double applyAngleDeg(double deg) const;
    // 线性部分的"等效缩放"：两列向量长度的几何平均（圆半径/字高按此缩放）
    double scaleFactor() const;
    // X / Y 两轴的缩放量（用于判定是否等比例）
    double scaleX() const, scaleY() const;
    // 线性部分是否为"轴对齐"（即矩形经此变换仍是轴对齐矩形）。
    // 含旋转/错切时矩形必须离散成多边形，否则轴对齐表示会画错。
    bool   isAxisAligned() const;
    // 复合：先执行 *this，再执行 outer。返回 outer ∘ this。
    X2D then(const X2D& outer) const;

    // 工厂：平移 + 旋转 + 缩放 + 绕 pivot 的复合（对应一次块插入）
    static X2D insert(const Vec3& insertPos, double rotDeg,
                      double sx, double sy, const Vec3& basePoint);
};

// ============================================================================
// 纯算法（SketchCAD::block）
// ============================================================================
namespace block {

// 用「块表 table」把块引用 ref 展开为**世界坐标**子图元列表。
//   * 处理：插入变换 → 动态参数（可见性过滤 / 拉伸 / 旋转 / 翻转）→ 属性文本替换
//   * 支持嵌套块（递归复合变换）；用 visiting 集合做环检测（自引用安全返回空）
//   * 展开结果是"瞬态几何"：id 一律置 0，不写回模型（渲染/命中/导出用）
// 找不到块定义 / 深度超限 / 检测到环 → 返回空列表。
std::vector<Element> expandRef(const Element& ref, const std::vector<BlockDef>& table);

// 依据块定义与插入参数构造一个块引用图元（属性值取 attdefs 的默认值、动态参数取默认值）。
Element makeRef(const BlockDef& def, const Vec3& insertPos,
                double rotDeg, double sx, double sy);

// 属性提取：把 ref 的属性值整理成 (tag → value) 列表（不可见属性也提取，供 BOM 使用）。
std::vector<BlockAttrValue> extractAttributes(const Element& ref, const std::vector<BlockDef>& table);

// 从一组源图元创建块定义（基点 basePt）。块内图元保留原世界坐标，基点作为插入参考，
// 插入到同一位置时为恒等变换（与 AutoCAD 块定义几何保持 WCS 的做法一致）。
BlockDef makeDefFromEntities(const QString& name, const Vec3& basePt,
                             const std::vector<Element>& srcs,
                             const QString& description = QString());

// ---- 序列化（块表随图存盘）----
QJsonObject blockDefToJson(const BlockDef& d);
BlockDef    blockDefFromJson(const QJsonObject& o);

} // namespace block
} // namespace SketchCAD
