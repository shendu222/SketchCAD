#pragma once
// 标准件「参数实例」JSON —— 保存一次出图的类型标识、参数键值与定位信息，
// 重新打开时按 templateId 找回原图模板，重放占位符替换 + 几何缩放 + 定位变换。
// 不保存几何副本：同一 JSON 在任何机器上渲染结果确定一致（模板库版本一致时）。
#include <QString>
#include <QJsonObject>
#include <QVariantMap>
#include <vector>
#include "Element.h"
#include "Vec.h"

namespace SketchCAD {

struct StdPartTemplateMeta;

// 插入定位：基点 + 目标点 + 旋转 + 统一缩放 + 图层覆盖
struct StdPartPlacement {
    QString mode = QStringLiteral("viewCenter"); // viewCenter / pick / coordinate
    Vec3 point;                                   // 目标点（世界坐标 mm）
    QString pointText;                            // coordinate 模式的 UCS 坐标输入文本
    QString base = QStringLiteral("bboxCenter");  // bboxCenter / lowerLeft / circleCenter / custom
    Vec3 customBase;                              // base=custom 时使用的模板内基点
    double angleDeg = 0.0;                       // 绕基点逆时针旋转角
    double scale = 1.0;                          // 统一缩放（参数缩放之外的插入缩放）
    QString layer = QStringLiteral("0");          // 非空时覆盖图元图层

    QJsonObject toJson() const;
    static StdPartPlacement fromJson(const QJsonObject& o);
};

struct StdPartInstance {
    int formatVersion = 1;                        // 权威版本键
    int schemaVersion = 1;                        // 只读兼容别名
    QString kind = QStringLiteral("stdpart-instance");
    QString typeId;                               // 标准件类型标识（标准号规范化）
    QString category;                             // 品名/目录名
    QString standard;                             // GB 标准号
    QString templateId;                           // 模板库 id（index.json 的 16 位十六进制）
    int templateLibraryVersion = 1;               // 模板库 schemaVersion
    QString units = QStringLiteral("mm");
    QVariantMap params;                           // 键与 {{KEY}} 占位符一一对应
    StdPartPlacement placement;
    QString createdAt;                            // Qt::ISODate UTC

    QJsonObject toJson() const;
    static StdPartInstance fromJson(const QJsonObject& o, QString* err = nullptr);
    static bool writeFile(const QString& path, const StdPartInstance& inst,
                          QString* err = nullptr);
    static bool readFile(const QString& path, StdPartInstance& out,
                         QString* err = nullptr);

    // 由模板元数据生成稳定 typeId（标准号优先，品名回落）
    static QString typeIdFor(const StdPartTemplateMeta& meta);

    // 完整重渲染：加载模板 → 占位符替换/参数缩放 → 插入定位变换
    static std::vector<Element> render(const StdPartInstance& inst,
                                       QString* err = nullptr);
    // 仅做定位变换（面板预览与自测可直接调用）
    static std::vector<Element> placeElements(const std::vector<Element>& src,
                                              const StdPartPlacement& p,
                                              QString* err = nullptr);
};

} // namespace SketchCAD
