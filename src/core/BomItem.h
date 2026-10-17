#pragma once
// 序号球标与明细表（BOM）数据模型 + 球标纯生成器
// ------------------------------------------------------------
// P2（功能扩充方案_参考CAD对照.md §3.3）核心实现。
// 设计要点：
//   * BomItem 为随图存档的「明细表」单行数据（顶层 JSON 键 "bom"，无数据不写键，
//     精确镜像 sheet 键的条件写键模式，见 SketchModel::toDrawingJson）。
//   * 球标 = 圆(细实线) + 居中编号文字 两个图元；Phase-1 用 Line(圆)+Text 纯生成器
//     代替 BlockRef 引用式球标（见 docs/feature-expansion/P2...md 偏差说明；后续可替换为
//     BlockRef 引用式球标，渲染/命中/导出三路同源展开）。
//   * srcIds 为「源几何」图元 id（球标指向的零部件几何），balloonIds 为「球标标记」图元
//     id（圆 + 文字）。后者是 Phase-1 在提示词 struct 基础上的扩展字段，用于悬停取数 /
//     删除球标时联动删标记，序列化一并存档以保证往返一致（已记入偏差文档）。
#include <QString>
#include <QVector>
#include <QColor>
#include "Element.h"
#include "Vec.h"

namespace SketchCAD {

// 明细表单行：序号 + 代号/名称/材料/单重/备注 + 关联源几何 id。
// seq 为球标编号，1 基、全局唯一递增（禁止重复，见提示词 FORBID）。
struct BomItem {
    int seq = 0;                         // 序号（球标编号，1 基，唯一递增）
    QString code;                        // 代号 / 图号
    QString name;                        // 名称
    QString material;                    // 材料
    QString weight;                      // 重量 / 单重
    QString remark;                     // 备注
    QVector<qint64> srcIds;              // 关联「源几何」图元 id（球标指向的零部件）
    QVector<qint64> balloonIds;         // 关联「球标标记」图元 id（圆 + 编号文字；Phase-1 扩展字段）

    bool operator==(const BomItem& o) const {
        return seq == o.seq && code == o.code && name == o.name &&
               material == o.material && weight == o.weight && remark == o.remark &&
               srcIds == o.srcIds && balloonIds == o.balloonIds;
    }
};

// 球标纯生成器：返回「圆 + 编号文字」两个图元的集合（与 SurfaceRoughness::buildRoughnessSymbol /
// SectionCutSymbol 同范式——生成器只产出图元，调用方负责 append 与撤销包裹）。
//   seq     : 序号编号（写入文字，QString::number(seq)）
//   anchor  : 球标圆心（世界坐标 mm；调用方决定放置点，通常取源几何质心 geo::elementCentroid）
//   layer   : 目标图层（默认 "0"）
//   r       : 球标圆半径(mm)，默认 4.0（GB 装配图序号圆直径约 6~8mm，半径 3~4 居中）
std::vector<Element> buildBalloon(int seq, const Vec3& anchor,
                                  const QString& layer = QStringLiteral("0"),
                                  double r = 4.0);

} // namespace SketchCAD
