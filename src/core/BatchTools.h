#pragma once
// P9（Ch15 扩展工具）：机械图拆分 / 自动排图 / 批量签名
// ------------------------------------------------------------
// 三个纯函数工具（不读取/不改动模型；产出数据交由调用方落盘或 apply）：
//   ① 机械图拆分：按图层或按图框矩形把图元分组 → 每组克隆源图 JSON 根、
//      仅替换 "elements" → 独立子档字节（存档格式零改动，FORBID 遵守）；
//   ② 自动排图：shelf 装箱算法（自写简单算法，IO_SPEC 明示 G1 非几何无风险）；
//   ③ 批量签名：定位标题栏 tb:field: 单元格填字（锚点/字高按单元格适配，
//      复用 TitleBlockLayout 单一入口）。
#include <QString>
#include <QStringList>
#include <QVector>
#include <QHash>
#include <QRectF>
#include <QJsonObject>
#include <vector>
#include <cstdint>
#include "Element.h"

namespace SketchCAD {

class SketchModel;

// ===================== ① 机械图拆分 =====================
struct SplitGroup {
    QString             name;     // 组名（图层名 / "帧-N" / "框外"）
    std::vector<qint64> elemIds;  // 该组图元 id（保源序）
};

// 按图层分组（保源序）。layer 为空串的图元归 "<无图层>"（正常图纸不会出现）。
QVector<SplitGroup> groupByLayer(const std::vector<Element>& elems);

// 按图框矩形分组：元素归入**首个**包含其包围盒中心的帧（ frames 保序即优先级）；
// 不在任何帧内 → outsideName 组（不丢弃）。frames 为空 → 全部进 outsideName。
QVector<SplitGroup> groupByFrame(const std::vector<Element>& elems,
                                 const QVector<QRectF>& frames,
                                 const QString& outsideName = QStringLiteral("框外"));

// 子档写出结果（与 groups 顺序对应；写失败的组在 outPaths 中缺位、errors 记因）
struct SplitResult {
    QVector<QString> outPaths;
    QStringList      errors;
    int              written = 0;
};

// 由源图 JSON 根 + 分组写子档：每组一份 <outDir>/<baseName>-<组名>.<ext>。
//   * 根对象深拷贝后仅替换 "elements" 为该组图元的 elementToJson 数组——
//     其余顶层键（layers/blocks/sheet/bom/params/properties/…）原样保留：
//     存档格式零改动，且子档与 toDrawingBytes() 同构、可独立打开；
//   * 组名做文件系统净化（非法字符 → '_'，空 → '_'）；
//   * 原子写：QSaveFile::commit()（Windows/MinGW 禁 QTemporaryFile+rename，见工程约束）；
//   * elems 用于 id→element 反查；组内 id 在 elems 中不存在 → 记 error 跳过该组。
SplitResult writeSplitDrawings(const QJsonObject& sourceRoot,
                               const QVector<SplitGroup>& groups,
                               const std::vector<Element>& elems,
                               const QString& outDir,
                               const QString& baseName,
                               const QString& ext = QStringLiteral(".skd"));

// ===================== ② 自动排图（shelf 装箱）=====================
struct PackRect {
    qint64 id  = -1;      // 调用方标识（图框/任务 id，原样回带）
    double w   = 0.0;     // 宽 (mm)
    double h   = 0.0;     // 高 (mm)
};
struct PackOptions {
    double sheetW = 420.0;         // 可用区域宽（已扣除边距的净尺寸）
    double sheetH = 297.0;         // 可用区域高
    double gap    = 5.0;           // 图框间距 (mm)
    bool   allowRotate = false;    // Phase-1 不实现旋转（字段保留；恒按原方向排）
};
struct PackPlacement {
    qint64 id      = -1;
    double x = 0.0, y = 0.0;       // 左下角（相对可用区域原点）
    double w = 0.0, h = 0.0;
    bool   rotated = false;        // 恒 false（Phase-1）
};
struct PackResult {
    bool                        ok          = false;  // 全部放入且不重叠
    QVector<PackPlacement>      placements;            // 与输入顺序一致
    double                      usedW = 0.0, usedH = 0.0;  // 实际占用包围盒
    double                      areaSum     = 0.0;    // Σ 图框面积
    double                      utilization = 0.0;    // areaSum / (usedW*usedH)
    QString                     error;                // 放不下/非法输入等原因
};

// Shelf 装箱：按高度降序（同高按宽降序、再按 id 升序——确定性）上架，
// 当前架剩余宽度不足则新起一架（y 累加上一架最大高度 + gap）。
// 构造上保证不重叠；任一图框超出纸张或排不下 → ok=false + error（placements 仍给出已排部分）。
PackResult packShelf(const QVector<PackRect>& rects, const PackOptions& opt);

// ===================== ③ 批量签名 =====================
struct SignUpdate {
    qint64 elemId   = -1;
    Element newElem;      // 填字后的图元副本（同 id；锚点/字高按单元格适配）
    QString fieldKey;     // 命中的栏目 key（如 "design"）
    bool    refit   = false;  // 是否按标题栏单元格重定位/收缩
};
struct SignResult {
    QVector<SignUpdate> updates;    // 命中字段（保元素序）
    QStringList         unmatched;  // 签名表中未在图纸标题栏找到的 key/label
};

// 标题栏字段元素提取：name 以 "tb:field:" 开头 → (key, 元素 id)，保元素序。
QVector<QPair<QString, qint64>> titleBlockFieldElems(const std::vector<Element>& elems);

// 批量签名：fields 为 key 或 label → 签名文字（label 经 titleBlockFields() 映射）。
//   * 只改文本；空值跳过（不擦除已有签名）；单元格无效 → 保留原锚点（绝不猜）；
//   * 只读模型：产出 updates 副本，调用方 beginEdit + updateElementQuiet 应用；
//   * unmatched 回填签名表中没有任何对应字段的 key/label（防静默漏签）。
SignResult batchSign(const SketchModel& m, const QHash<QString, QString>& fields);

} // namespace SketchCAD
