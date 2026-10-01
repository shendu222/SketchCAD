#pragma once
#include <QString>
#include <vector>
#include "Vec.h"
#include "Modify.h"

namespace SketchCAD {

// ============================================================
// GripOps —— 夹点热操作几何引擎
// ------------------------------------------------------------
// 把「热夹点语义」翻译为 modify::Change 变更集：
// 复用 Modify 层旋转/缩放/镜像/移动实现（不重写几何），
// 只补充夹点特有的两块：
//   1) Stretch 按 (图元, 控制点) 引用精确迁移 —— 保留现有端点拖拽
//      的「重合联动」拓扑语义（collectLinked 结果直接可用）；
//   2) 各模式的相对参考量（旋转 0° / 缩放 1 倍 / 移动 0 偏移都以
//      进入该模式瞬间的光标位置为零点），保证 Ctrl 循环切换模式时
//      预览从恒等状态平滑起步。
//
// 纯函数 + 纯数据（headless 可测）；落盘统一走
// model->beginEdit(label) + modify::applyChanges（translateSelection 同款）。
// ============================================================
namespace grip {

enum class Mode { Stretch = 0, Move, Rotate, Scale, Mirror };

Mode nextMode(Mode m);              // Ctrl 循环：Stretch→…→Mirror→Stretch
QString modeName(Mode m);           // 拉伸 / 移动 / 旋转 / 缩放 / 镜像

// 热夹点几何上下文（UI 层组装，纯数据）
struct Grips {
    Vec3 base;                                      // 热夹点（基准点）
    std::vector<Element> moveSet;                   // Move/Rotate/Scale/Mirror 作用集（选中集）
    std::vector<Element> stretchSet;                // Stretch 作用集（选中集 ∪ 重合联动图元）
    std::vector<std::pair<int, int>> stretchRefs;   // (elemId, handleIdx) 迁移引用
    Vec3 refCursor;                                 // 当前模式零参考光标
};

// 一次 apply 的结果：changes 为空且 reject 非空 = 被边界校验拒绝
struct GripResult {
    std::vector<modify::Change> changes;
    QString reject;                                 // 拒绝原因（日志用）
};

// 按模式计算 base→cursor 的变更集（不落盘、不改模型）
GripResult apply(const Grips& g, Mode m, const Vec3& cursor);

// 模式几何量（状态栏/日志显示；相对 refCursor）
double rotationDeg(const Grips& g, const Vec3& cursor);
double scaleFactor(const Grips& g, const Vec3& cursor);

} // namespace grip
} // namespace SketchCAD
