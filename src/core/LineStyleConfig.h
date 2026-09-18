#pragma once
#include "Element.h"
#include <QMap>

namespace SketchCAD {

// ============================================================
// 线型 ↔ 线宽 统一映射配置（单一数据源）
// ============================================================
//
// 设计目标（对应需求）：
//   * 统一映射：所有"线型默认线宽"集中在一处，改一处全局生效。
//   * 遵循 GB/T 14665-2012 表1：线宽以「mm」为单位，取自当前「线宽组别」
//     (LineWidthGroup)，粗/细成 2:1。切换组别后全图线宽同步更新。
//   * 全局可维护：运行时可通过 setDefaultWidth() 覆盖某一线型的默认线宽(mm)。
//   * 易扩展新线型：新增一种线型 = 在 Element.h 的 LineStyle 枚举末尾加一个值
//     + 在 lineStyleIsThick() 归类为粗/细，其余消费方自动获得结果。
//
// 粗/细归类（决定取 groupThickMm 还是 groupThinMm）：
//   粗：Solid(粗实线)、ThickDashed(粗虚线)、ThickDashDot(粗点画线)
//   细：Dashed(细虚线)、Dotted、DashDot、DashDotDot、Wave、DoubleBreak
//
// 注意：LineStyle 枚举的"整数值顺序只增不改"（存盘以整数序列化）。
class LineStyleConfig {
public:
    // 进程内单例（工厂 + 注册表模式，全局唯一真源）
    static LineStyleConfig& instance();

    // 查询某线型的默认线宽(mm)：按当前线组派生（粗/细档），除非被 setDefaultWidth 覆盖。
    // 未知线型回落到细线档，绝不返回 0/负数。
    double defaultWidth(LineStyle ls) const;

    // 运行时覆盖某线型的默认线宽(mm)（全局可维护）。mm<=0 时忽略。
    void setDefaultWidth(LineStyle ls, double mm);

    // 清除某线型的显式覆盖，恢复"按当前线组派生"。
    void clearOverride(LineStyle ls);

private:
    LineStyleConfig();                       // 私有构造
    LineStyleConfig(const LineStyleConfig&) = delete;
    LineStyleConfig& operator=(const LineStyleConfig&) = delete;

    QMap<LineStyle, double> map_;            // 线型 → 显式覆盖线宽(mm)
};

// 线型是否属「粗线档」（GB/T 14665-2012 表1：粗线档取 groupThickMm）。
bool lineStyleIsThick(LineStyle ls);

// 自由函数桥：供 Element::effectiveLineWidth() 调用，避免 Element.h 与
// LineStyleConfig.h 形成头文件循环依赖（Element.h 仅前向声明此函数）。
double defaultWidthForStyle(LineStyle ls);

} // namespace SketchCAD
