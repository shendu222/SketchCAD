#pragma once
#include <QString>
#include <vector>
#include "Element.h"
#include "Block.h"

// ============================================================================
// 外部参照（Xref）子系统
// ----------------------------------------------------------------------------
// 参照 AutoCAD 的行为准则（Attach / Detach Xrefs）：
//   * 附加的图形在本图中表现为**一条块表记录**（BlockDef），符号名默认取文件名主体；
//     这与本程序既有的「引用式」块模型一致：定义只存一份，画面上的每个参照实例是一条
//     ElementType::BlockRef，渲染 / 命中 / 导出时按定义实时展开。
//   * Xref 记录额外携带：源路径、路径类型、是否嵌套、状态。这些字段对普通块无副作用
//     （旧存档缺键时全部回落默认 → 与普通块完全一致）。
//   * 分离（Detach）必须同时移除「定义」与「全部参照实例」；随宿主带到本图的依赖
//     （嵌套参照）不可直接分离，只允许随宿主一起消失。
//
// 命名空间约定与 Block 一致：**数据**（路径类型 / 状态 / 读取结果）在 SketchCAD 顶层，
// **纯算法**（读文件、路径换算、依赖收集）在 SketchCAD::xref。
// ============================================================================
namespace SketchCAD {

// 外部参照的路径类型（AutoCAD 外部参照管理器中的 Path Type）
enum class XrefPathType {
    FullPath     = 0,   // 完整路径：原样记录绝对路径
    RelativePath = 1,   // 相对路径：相对宿主图所在目录记录
    NoPath       = 2    // 无路径：只记录文件名（在本图所在目录 / 搜索路径下查找）
};
inline const char* xrefPathTypeName(XrefPathType t) {
    switch (t) {
        case XrefPathType::FullPath:     return "完整路径";
        case XrefPathType::RelativePath: return "相对路径";
        case XrefPathType::NoPath:       return "无路径";
    }
    return "完整路径";
}
// 命令行 / 对话框用的解析：full|relative|none（以及 path|rel|nop 简写）。
// ok 非空时回写解析是否成功；失败不改动原值，由调用方决定兜底。
inline XrefPathType xrefPathTypeFromName(const QString& s, bool* ok = nullptr) {
    const QString k = s.trimmed().toLower();
    if (k == QStringLiteral("full") || k == QStringLiteral("path") || k == QStringLiteral("complete")) {
        if (ok) *ok = true;
        return XrefPathType::FullPath;
    }
    if (k == QStringLiteral("relative") || k == QStringLiteral("rel") || k == QStringLiteral("relativepath")) {
        if (ok) *ok = true;
        return XrefPathType::RelativePath;
    }
    if (k == QStringLiteral("none") || k == QStringLiteral("nop") || k == QStringLiteral("nopath")) {
        if (ok) *ok = true;
        return XrefPathType::NoPath;
    }
    if (ok) *ok = false;
    return XrefPathType::FullPath;
}

// 外部参照状态（简化自 AutoCAD Xref Manager 的 Status 列）
enum class XrefStatus {
    Loaded   = 0,   // 已载入：定义内保留最近一次读取到的几何
    NotFound = 1    // 未找到：源文件缺失 / 解析失败（几何为最近一次快照，仅作提示）
};
inline const char* xrefStatusName(XrefStatus s) {
    switch (s) {
        case XrefStatus::Loaded:   return "已载入";
        case XrefStatus::NotFound: return "未找到";
    }
    return "已载入";
}

// 一次「读参照源文件」的结果（纯数据，读取过程不触碰任何模型状态）
struct XrefSource {
    std::vector<Element>  elements;    // 顶层图元（世界坐标）
    std::vector<BlockDef> blocks;      // 该文件自带的块表（供依赖引入为嵌套参照）
    QString               formatName;  // 格式名（日志用）
};

namespace xref {

// 读取参照源文件。支持本程序 JSON / DXF / DWG；STEP 为只写格式，拒绝并回 err。
// 路径须为已存在的文件（存在性由调用方或本函数判定均可）。
bool readSource(const QString& absPath, XrefSource& out, QString* err = nullptr);

// Xref 符号名 = 文件名主体（AutoCAD 语义：附加后块名默认取文件名）
QString blockNameForPath(const QString& path);

// 按路径类型算出「存盘形态」的路径（随块表写盘之前调用）：
//   FullPath → 原绝对路径；RelativePath → 相对 hostDir；NoPath → 纯文件名。
//   hostDir 为空时 RelativePath 退化为文件名（无基准可用时不可假装有基准）。
QString storedPath(const QString& absPath, XrefPathType t, const QString& hostDir);

// 由「存盘形态」还原绝对路径（用于重解析 / 日志 / 重复附加判定）。
// 无法还原时返回原串（调用方据此判定文件是否存在）。
QString resolvedPath(const QString& stored, XrefPathType t, const QString& hostDir);

// 依赖收集：src 中被引用、但 table 里尚缺定义的块名（去重、保持首现顺序）。
// 引入后这些定义在宿主图中标记为「嵌套参照」。
std::vector<QString> missingDependencies(const std::vector<Element>& src,
                                         const std::vector<BlockDef>& table);

} // namespace xref
} // namespace SketchCAD
