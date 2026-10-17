#pragma once
#include <QString>
#include <QVector>
#include "SheetSpec.h"

namespace SketchCAD {

// ===================== 【新建图框】图框布置编辑模板（需求 13）=====================
// 把「图幅 + 绘图比例 + 标题栏 + 明细栏 + 附加栏/代号栏/参数栏 + 比例调整设置」
// 的组合保存为命名模板，选择模板即快速切换全部设置。
//
// 持久化：独立 JSON 文件 %LOCALAPPDATA%/SketchCAD/frame_templates.json
// （刻意不进 AppSettings specTable——模板是整结构多值数据，逐键声明负担过重，
//   且 specTable 定位是「应用习惯」标量键）。
// 文件缺失/损坏 → 回落内置默认模板（"sample"），绝不崩溃。

struct SheetFrameTemplate {
    QString name;                 // 模板名（唯一键）
    SheetFrameOptions opts;       // 图框布置全套选项
    double scaleNum = 1.0;        // 绘图比例前值（a:b 的 a）
    double scaleDen = 1.0;        // 绘图比例后值（a:b 的 b）
    double scaleRatio() const { return scaleDen > 0.0 ? scaleNum / scaleDen : 1.0; }
};

// 模板文件落盘路径（%LOCALAPPDATA%/SketchCAD/frame_templates.json；目录不存在则创建）
QString frameTemplatesFilePath();

// 读取全部模板（按保存顺序）。path 传空用默认路径（自测注入临时路径）。
// 文件缺失 → 内置默认（sample）；损坏/解析失败 → 同样回落内置默认，绝不抛出。
QVector<SheetFrameTemplate> loadFrameTemplates(const QString& path = QString());

// 保存全部模板（整体覆盖，原子写：临时文件 + rename）。失败返回 false。
bool saveFrameTemplates(const QVector<SheetFrameTemplate>& templates,
                        const QString& path = QString());

} // namespace SketchCAD
