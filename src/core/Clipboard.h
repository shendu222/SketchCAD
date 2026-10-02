#pragma once
#include <QString>
#include <QJsonObject>
#include <vector>
#include "Element.h"
#include "Block.h"
#include "Vec.h"

// ============================================================================
// 内部剪贴板（copyclip / copybase / pasteclip / pasteblock 共用核心）
// ----------------------------------------------------------------------------
// 数据形态：QClipboard 上的自定义 mime（application/x-sketchcad-clip），
// 内容 = 版本化 JSON 文本：
//   {
//     "v": 1,                     // 版本号：!= kVersion 一律拒绝（旧版内容忽略不崩溃）
//     "base": {"x":.., "y":..},   // 基点（粘贴时对齐光标/落点）
//     "elements": [ elementToJson... ],
//     "blocks":   [ blockDefToJson... ]   // 元素所依赖的块定义（跨实例粘贴保留块表）
//   }
//
// 为什么用 JSON 而不是 QDataStream：
//   * 与存档序列化共用 elementToJson / blockDefToJson 单一实现（DRY，字段增减不漂移）；
//   * 文本可读、可诊断；跨 SketchCAD 实例（两个进程）天然可用——系统剪贴板搬运文本。
//
// 版本化策略：只认 kVersion；读到更高/更低版本 → fromJson 返回 false 并给出原因，
// 调用方 log 提示后安全退出（绝不越界解析）。
// ============================================================================
namespace SketchCAD {

class SketchModel;

namespace modify { struct Change; }

namespace clipboard {

inline const char* kMimeName = "application/x-sketchcad-clip";
constexpr int kVersion = 1;

struct Payload {
    Vec3 base;                        // 基点（世界坐标）
    std::vector<Element> elements;    // 图元（世界坐标）
    std::vector<BlockDef> blocks;     // 依赖的块定义（含嵌套依赖）
};

// ---- 序列化 ----
QJsonObject toJson(const Payload& p);
// version 不符 / 结构损坏 → false（*err 给出原因，可空）；绝不抛异常、绝不全量解析半截数据。
bool fromJson(const QJsonObject& o, Payload* out, QString* err = nullptr);

// ---- 几何操作（纯函数，供自测直测）----
// 把 payload 的全部图元平移 delta（base 同步平移）。
void translate(Payload& p, const Vec3& delta);
// 平移使 base 对齐 to（等价 translate(p, to - p.base)）。
void translateTo(Payload& p, const Vec3& to);

// 粘贴为独立图元集合：全部产出 id=-1 的新增 Change（走 applyChanges 单步撤销）。
std::vector<modify::Change> makePasteChanges(const Payload& p);

// ---- 粘贴为块（PASTEBLOCK）----
// 把 payload 图元收进一个块定义（基点 = p.base；块内几何保持世界坐标，
// 与 block::makeDefFromEntities 同语义）。调用方先解决块名冲突。
BlockDef makeBlockDefFromPayload(const Payload& p, const QString& name);
// 依据块定义构造插入位置的块引用图元（旋转 0、等比 1）。
Element makeBlockRefElement(const BlockDef& def, const Vec3& insertPos);

// 块名去重：base 不冲突原样返回；冲突则 base_1、base_2 …
QString uniqueBlockName(const SketchModel& m, const QString& base);

// 收集 elements 中 BlockRef 引用的块定义（含嵌套依赖），从模型块表拷出。
// copyclip / copybase 组包共用（命令行与 GUI 同一实现，语义单一不漂移）。
std::vector<BlockDef> collectBlockDeps(const SketchModel& m,
                                       const std::vector<Element>& elements);

// ---- 粘贴落盘（命令行 / GUI 菜单共用入口）----
// 流程：合并块表（冲突自动改名 + 同步 BlockRef 引用名）→ asBlock ? 转块+插入 : 平移+新增。
// 调用方保证已 beginEdit（本函数内所有落盘均走 Quiet 路径，单步撤销由外层一次完成）。
// p.elements 为空 → false + *msg。
bool pasteInto(SketchModel* m, const Payload& p, const Vec3& to,
               bool asBlock, QString* msg);

// ---- QClipboard 桥（Qt 应用进程内 / 跨实例）----
bool putToClipboard(const Payload& p);
// 无本应用内容 / 版本不符 → false（*err 原因）。
bool getFromClipboard(Payload* out, QString* err = nullptr);

} // namespace clipboard
} // namespace SketchCAD
