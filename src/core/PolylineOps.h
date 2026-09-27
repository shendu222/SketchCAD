#pragma once
#include "core/Element.h"
#include "core/SketchModel.h"
#include <vector>

// ============================================================
// PolylineOps —— PEDIT 式多段线编辑的纯模型实现
// ============================================================
// 把「拟合 / 样条化 / 设宽 / 闭合 / 合并 / 反转」六个操作从 UI 工具中抽离为纯几何+模型
// 函数，UI 工具（PolylineEditTool）与无界面自测共用同一份逻辑，避免漂移。
// 全部返回 bool 表示是否成功；失败（对象类型不符、端点不重合等）只 log，不抛异常。
//
// 与 AutoCAD PEDIT 命令行行为的对应关系：
//   PEDIT → F  (Fit)      -> fit()        平滑曲线**通过**所有顶点
//   PEDIT → S  (Spline)   -> spline()     顶点作为三次 B 样条控制点的逼近曲线
//   PEDIT → W  (Width)    -> setWidth()   整条折线统一线宽（lineWidthAuto=false）
//   PEDIT → C  (Close)    -> setClosed(t) 闭合（新增一段直线闭合段）
//   PEDIT → J  (Join)     -> join()       把相邻直线/圆弧/折线接成一条
//   PEDIT → 反转顶点       -> reverse()    顶点与段类型同时逆序
namespace SketchCAD {
namespace polyops {

// 闭合 / 打开：close=true 闭合折线（新增一段直线闭合段并补一个 arcFlag），
// false 打开（删除闭合段对应的 arcFlag）。已是目标状态时幂等返回 true。
bool setClosed(SketchModel& m, int id, bool close);

// 反转：pts 与 arcFlags 同时逆序（闭合折线循环逆序，段几何保持一致）。
bool reverse(SketchModel& m, int id);

// 设置全局线宽（PEDIT Width）：lineWidthAuto=false，lineWidth=w（mm/屏幕 px）。
// 负数会被钳为 0。
bool setWidth(SketchModel& m, int id, double w);

// 拟合曲线（PEDIT Fit）：splineSubType=1，渲染/命中/离散时平滑通过所有顶点。
bool fit(SketchModel& m, int id);

// 样条曲线（PEDIT Spline）：splineSubType=2，渲染/命中/离散时顶点作为三次 B 样条控制点。
bool spline(SketchModel& m, int id);

// 合并：把 source（Line/Arc/Polyline）拼接到 target 折线的某个端点（需端点重合，tol mm）。
// 成功则更新 target（原地扩展 pts/arcFlags）并删除 source，返回 true。
// 约束：target 必须开放、source 必须开放（Arc 用采样点近似为直线段接入）。
bool join(SketchModel& m, int targetId, int sourceId, double tol = 1e-3);

} // namespace polyops
} // namespace SketchCAD
