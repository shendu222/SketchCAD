#pragma once
// GB 标准件参数化库（由 DWG 图库批量解析结果驱动）
// ---------------------------------------------------------------------------
// 设计要点（详见 docs/GB标准件图库参数化-推理与实现.md）：
//   1) 数据源：tools/stdpart 批量解析 stdpart_index.json 提取的「目录名(GB 标准号/品名)
//      + 文件名(规格 d×L)」映射，以及逐图元/图层/标注统计特征；
//   2) 目录与规格：StdPartCatalog::load() 读 JSON 规格表（每类含 id、标准、品名、
//      参数 schema、默认值、规格点列表），作为单一数据源供 GUI/命令/自测共用；
//   3) 参数驱动：generate(id, params) 按 schema 校验/补默认 → 调用对应参数化生成器
//      产出几何 + 自动补全尺寸标注（dim::Style 同源字号/箭头）；
//   4) 可变参数：nominal(d)、length(L)、pitch(P)、acrossFlats(s)、thickness(k/m/h)、
//      innerD、outerD 等，修改参数后重新 generate 即自动更新图形与标注。
#include <QString>
#include <QJsonObject>
#include <QVariantMap>
#include <QVector>
#include <memory>
#include <vector>
#include "Element.h"

namespace SketchCAD {

struct StdPartParamDef {
    QString key;
    QString label;      // 中文名（如 公称直径 d）
    QString unit;       // mm / 无
    double  defaultValue = 0.0;
    double  minValue = 0.0;
    double  maxValue = 0.0;
    bool    required = false;
    QString description;
};

struct StdPartCategory {
    QString id;           // 稳定 id（如 gb5782_hexbolt）
    QString name;         // 品名（如 六角头螺栓）
    QString standard;     // GB 标准号（如 GB/T 5782-86）
    QString generator;    // 生成器类型（hexbolt/nut/washer/pin/screw…）
    QVector<StdPartParamDef> params;
    QVector<QVariantMap>     specs;    // 规格点（从 DWG 文件名/尺寸反推）
    QJsonObject sourceStats;           // 解析统计（图元/图层/标注特征）
};

class StdPartCatalog {
public:
    static StdPartCatalog& instance();
    bool load(const QString& jsonPath, QString* err = nullptr);
    bool loaded() const { return loaded_; }
    const QVector<StdPartCategory>& categories() const { return cats_; }
    const StdPartCategory* find(const QString& id) const;
    QVector<QString> ids() const;

    // 由解析索引 stdpart_index.json 生成目录规格 JSON（离线构建步骤）
    static bool buildFromAnalysis(const QString& analysisJson,
                                  const QString& outJson,
                                  QString* err = nullptr,
                                  int* categoryCount = nullptr,
                                  int* specCount = nullptr);

    // 参数化出图：校验参数 → 生成几何 → 自动补全尺寸标注。
    // withDims=true 时追加 Dimension 图元（数值由几何实测，符合国标标注样式）。
    bool generate(const QString& id, const QVariantMap& params,
                  std::vector<Element>& out, QString* note = nullptr,
                  bool withDims = true) const;
private:
    StdPartCatalog() = default;
    QVector<StdPartCategory> cats_;
    bool loaded_ = false;
};

} // namespace SketchCAD
