#pragma once
// P7（Ch5 辅助工具）：批量 BOM 分析
// ------------------------------------------------------------
// 扫描多个图纸文件，仅解析顶层 "bom" 键（IO_SPEC「批量扫描只读档头，避免全量解析」：
// 不实例化 SketchModel / 元素，只读 JSON 顶层 bom 数组），按 (名称, 代号) 汇总数量。
#include <QStringList>
#include <QVector>

namespace SketchCAD {

struct BomSummaryRow {
    QString name;            // 零件名称
    QString code;            // 代号/图号
    int     quantity = 0;    // 汇总数量（跨文件累加）
};

namespace BatchBom {

// 分析多个图纸文件 → 汇总行。readErrors（可空）收集无法解析/无 bom 的文件原因。
QVector<BomSummaryRow> analyze(const QStringList& paths, QStringList* readErrors = nullptr);

} // namespace BatchBom
} // namespace SketchCAD
