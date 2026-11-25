#pragma once
// GB 标准件「DWG 原图模板库」—— 占位符替换式参数化出图
// ---------------------------------------------------------------------------
// 与 StdPartCatalog（程序化几何重建）并存的另一条路线：
//   * 把源图库每个可打开的 DWG 转成 DXF 原图，直接作为模板存入库中
//     （qCompress 压缩的 .tpl 文件 + 索引 JSON），保留原始几何/图层/线型；
//   * 从「目录名(GB 标准) + 文件名(规格)」提取可变参数，在模板的 TEXT 文字
//     与追加的规格标注中写入统一占位符 {{KEY}}；同时在元数据中记录几何
//     基准尺寸（baselineNominal / baselineLength），供渲染时按比例缩放；
//   * 生成时界面输入参数值 → 替换占位符 + 几何缩放 → 得到 Element 集合，
//     实时预览并可落盘为 DXF / DWG / .sketch。
//
// 占位符命名规则（统一、唯一数据源 kPlaceholderKeys）：
//   {{NOMINAL_D}}  公称直径 d(mm)     {{LENGTH_L}}  公称长度 L(mm)
//   {{PITCH_P}}    螺距 P(mm)         {{ACROSS_FLATS_S}} 对边宽 s(mm)
//   {{THICKNESS_T}} 厚度 k/m/h(mm)    {{SPEC_TEXT}}  完整规格文字(如 M6x30)
//   规则：双花括号 + 全大写 + 下划线；只出现在 TEXT/MText 的显示文字与
//   自动追加的规格标注里；导出时 {{...}} 不会被 DXF 字段机制消费。
#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QVariantMap>
#include <QVector>
#include <vector>
#include "Element.h"

namespace SketchCAD {

// 占位符键（顺序即 UI 表单顺序，也是导出模板时的固定顺序）
namespace StdPartPlaceholders {
constexpr const char* kNominalD    = "NOMINAL_D";
constexpr const char* kLengthL     = "LENGTH_L";
constexpr const char* kPitchP      = "PITCH_P";
constexpr const char* kAcrossFlats = "ACROSS_FLATS_S";
constexpr const char* kThickness   = "THICKNESS_T";
constexpr const char* kSpecText    = "SPEC_TEXT";
const QStringList& keys();          // 上述 6 键（含 SPEC_TEXT）
QString bracketed(const QString& key); // {{KEY}}
}

// 单个模板的元数据
struct StdPartTemplateMeta {
    QString id;              // 稳定 id（目录+文件名哈希）
    QString category;        // 品名/目录名
    QString standard;        // GB 标准号
    QString sourceFile;      // 源 DWG 相对路径
    QString tplFile;         // 压缩模板文件名（相对 store 目录）
    qint64  sourceBytes = 0;
    int     elementCount = 0;
    int     textCount = 0;
    int     placeholderCount = 0;
    bool    ok = false;
    QString failReason;      // 解析失败原因（成功为空）
    double  baselineNominal = 0.0;  // 文件名提取的 d（几何缩放基准）
    double  baselineLength  = 0.0;  // 文件名提取的 L
    double  baselinePitch   = 0.0;
    double  baselineAcrossFlats = 0.0;
    double  baselineThickness   = 0.0;
    QString specText;        // 原规格文字（如 M5x25）
    QStringList layers;
};

// 模板库：一个目录 + index.json + *.tpl（qCompress 的 DXF 文本）
class StdPartTemplateStore {
public:
    static StdPartTemplateStore& instance();
    // 默认库路径：exe 同级 data/stdpart/templates → 源码树相对路径（开发期）→ stdpart_tpl
    static QString defaultStoreDir();
    bool load(const QString& storeDir, QString* err = nullptr);
    bool loaded() const { return loaded_; }
    QString storeDir() const { return storeDir_; }
    const QVector<StdPartTemplateMeta>& metas() const { return metas_; }
    const StdPartTemplateMeta* find(const QString& id) const;
    QVector<QString> categories() const;
    QVector<const StdPartTemplateMeta*> byCategory(const QString& cat) const;

    // 读取模板原始 Element（含占位符文字），并给出元数据
    bool loadElements(const QString& id, std::vector<Element>& out,
                      const StdPartTemplateMeta** meta = nullptr,
                      QString* err = nullptr) const;

    // 参数替换 + 几何缩放，生成可渲染/可保存的 Element 集合
    // params: 键为占位符 key（NOMINAL_D 等），值为 mm 数值；SPEC_TEXT 可省略（自动拼）
    static std::vector<Element> render(const std::vector<Element>& src,
                                       const StdPartTemplateMeta& meta,
                                       const QVariantMap& params);

    // 纯文本占位符替换（供自测 / DXF 文本前处理）
    static QString replaceInText(const QString& text, const QVariantMap& values);

    // 由 index.json + *.tpl 构建/校验；构建器工具使用
    static bool writeIndex(const QString& storeDir, const QVector<StdPartTemplateMeta>& metas,
                           QString* err = nullptr);

    // 由单个 DWG 字节构建占位符模板（构建器工具调用）；写入 *tplDxfOut，填充 meta
    static bool buildFromDwg(const QByteArray& dwg, const QString& sourceRel,
                             const QString& categoryDir, QByteArray& tplDxfOut,
                             StdPartTemplateMeta& meta, QString* err = nullptr);
private:
    StdPartTemplateStore() = default;
    QString storeDir_;
    QVector<StdPartTemplateMeta> metas_;
    bool loaded_ = false;
};

} // namespace SketchCAD
