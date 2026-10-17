#include "SheetFrameTemplate.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace SketchCAD {

// ----------------------------- JSON 序列化（显式逐键，缺键回落默认）-----------------------------
// 只覆盖 SheetFrameOptions 与模板相关的字段；缺键 = 默认值，旧文件/新字段天然前向兼容。
static QJsonObject optsToJson(const SheetFrameOptions& o) {
    QJsonObject j;
    j["sizeMode"]      = (int)o.sizeMode;
    j["basicFormat"]   = (int)o.basicFormat;
    j["landscape"]     = o.landscape;
    j["extBase"]       = (int)o.extBase;
    j["extFactor"]     = o.extFactor;
    j["customW"]       = o.customW;
    j["customH"]       = o.customH;
    j["bindingMargin"] = o.bindingMargin;
    j["bindingWidth"]  = o.bindingWidth;
    j["autoFillDate"]  = o.autoFillDate;
    j["specifyFramePos"] = o.specifyFramePos;
    j["centerMarks"]   = o.centerMarks;
    j["zonesEnabled"]  = o.zonesEnabled;
    j["zoneCountX"]    = o.zoneCountX;
    j["zoneCountY"]    = o.zoneCountY;
    j["zoneShowLeft"]  = o.zoneShowLeft;
    j["titleBlock"]    = (int)o.titleBlock;
    j["bomEnabled"]    = o.bomEnabled;
    j["bomStyle"]      = (int)o.bomStyle;
    j["bomRows"]       = o.bomRows;
    j["additionalBarEnabled"] = o.additionalBarEnabled;
    j["additionalBarKey"]     = o.additionalBarKey;
    j["codeBarEnabled"] = o.codeBarEnabled;
    j["codeBarKey"]     = o.codeBarKey;
    j["paramBarEnabled"] = o.paramBarEnabled;
    j["paramBarKey"]     = o.paramBarKey;
    j["dimScaleAdjust"]       = o.dimScaleAdjust;
    j["textScaleAdjust"]      = o.textScaleAdjust;
    j["textInitHeightEnabled"] = o.textInitHeightEnabled;
    j["textInitHeight"]        = o.textInitHeight;
    j["lineWidthScaleAdjust"]  = o.lineWidthScaleAdjust;
    j["lineWidthInitEnabled"]  = o.lineWidthInitEnabled;
    j["lineWidthInitRatio"]    = o.lineWidthInitRatio;
    j["projection"]     = (int)o.projection;
    return j;
}

static SheetFrameOptions optsFromJson(const QJsonObject& j) {
    SheetFrameOptions o;   // 默认值兜底：缺键即默认（前向兼容）
    o.sizeMode      = (SheetFrameOptions::SizeMode)j["sizeMode"].toInt((int)o.sizeMode);
    o.basicFormat   = (SheetFormat)j["basicFormat"].toInt((int)o.basicFormat);
    o.landscape     = j["landscape"].toBool(o.landscape);
    o.extBase       = (SheetFormat)j["extBase"].toInt((int)o.extBase);
    o.extFactor     = j["extFactor"].toInt(o.extFactor);
    o.customW       = j["customW"].toDouble(o.customW);
    o.customH       = j["customH"].toDouble(o.customH);
    o.bindingMargin = j["bindingMargin"].toBool(o.bindingMargin);
    o.bindingWidth  = j["bindingWidth"].toDouble(o.bindingWidth);
    o.autoFillDate  = j["autoFillDate"].toBool(o.autoFillDate);
    o.specifyFramePos = j["specifyFramePos"].toBool(o.specifyFramePos);
    o.centerMarks   = j["centerMarks"].toBool(o.centerMarks);
    o.zonesEnabled  = j["zonesEnabled"].toBool(o.zonesEnabled);
    o.zoneCountX    = j["zoneCountX"].toInt(o.zoneCountX);
    o.zoneCountY    = j["zoneCountY"].toInt(o.zoneCountY);
    o.zoneShowLeft  = j["zoneShowLeft"].toBool(o.zoneShowLeft);
    o.titleBlock    = (TitleBlockStyle)j["titleBlock"].toInt((int)o.titleBlock);
    o.bomEnabled    = j["bomEnabled"].toBool(o.bomEnabled);
    o.bomStyle      = (SheetFrameOptions::BomStyle)j["bomStyle"].toInt((int)o.bomStyle);
    o.bomRows       = j["bomRows"].toInt(o.bomRows);
    o.additionalBarEnabled = j["additionalBarEnabled"].toBool(o.additionalBarEnabled);
    o.additionalBarKey     = j["additionalBarKey"].toString();
    o.codeBarEnabled       = j["codeBarEnabled"].toBool(o.codeBarEnabled);
    o.codeBarKey           = j["codeBarKey"].toString();
    o.paramBarEnabled      = j["paramBarEnabled"].toBool(o.paramBarEnabled);
    o.paramBarKey          = j["paramBarKey"].toString();
    o.dimScaleAdjust        = j["dimScaleAdjust"].toBool(o.dimScaleAdjust);
    o.textScaleAdjust       = j["textScaleAdjust"].toBool(o.textScaleAdjust);
    o.textInitHeightEnabled = j["textInitHeightEnabled"].toBool(o.textInitHeightEnabled);
    o.textInitHeight        = j["textInitHeight"].toDouble(o.textInitHeight);
    o.lineWidthScaleAdjust  = j["lineWidthScaleAdjust"].toBool(o.lineWidthScaleAdjust);
    o.lineWidthInitEnabled  = j["lineWidthInitEnabled"].toBool(o.lineWidthInitEnabled);
    o.lineWidthInitRatio    = j["lineWidthInitRatio"].toDouble(o.lineWidthInitRatio);
    o.projection            = (ProjectionMethod)j["projection"].toInt((int)o.projection);
    return o;
}

QString frameTemplatesFilePath() {
    QString local = QDir::fromNativeSeparators(
        QString::fromLocal8Bit(qgetenv("LOCALAPPDATA")));
    if (local.isEmpty()) local = QDir::homePath() + "/AppData/Local";
    if (local.isEmpty()) local = QDir::homePath();
    const QString dir = local + "/SketchCAD";
    QDir().mkpath(dir);
    return dir + "/frame_templates.json";
}

// 内置默认模板（文件缺失/损坏时回落）："sample" = 附图示例状态
// （A2 横置 1:1、分区 6/8、对中符号、装订线 25、标题栏/明细栏/附加栏/代号栏启用、
//   参数栏关闭、比例调整三开关开启且两个子选项关闭）
static QVector<SheetFrameTemplate> builtinTemplates() {
    QVector<SheetFrameTemplate> out;
    SheetFrameTemplate t;
    t.name = QStringLiteral("sample");
    t.scaleNum = 1.0;
    t.scaleDen = 1.0;
    SheetFrameOptions& o = t.opts;
    o.sizeMode = SheetFrameOptions::SizeMode::Basic;
    o.basicFormat = SheetFormat::A2;
    o.landscape = true;
    o.bindingMargin = true;
    o.bindingWidth = 25.0;
    o.centerMarks = true;
    o.zonesEnabled = true;
    o.zoneCountX = 6;
    o.zoneCountY = 8;
    o.zoneShowLeft = false;
    o.titleBlock = TitleBlockStyle::Standard;
    o.bomEnabled = true;
    o.additionalBarEnabled = true;
    o.additionalBarKey = QStringLiteral("revision");
    o.codeBarEnabled = true;
    o.codeBarKey = QStringLiteral("Tydh");
    o.paramBarEnabled = false;
    o.dimScaleAdjust = true;
    o.textScaleAdjust = true;
    o.textInitHeightEnabled = false;
    o.textInitHeight = 3.5;
    o.lineWidthScaleAdjust = true;
    o.lineWidthInitEnabled = false;
    o.lineWidthInitRatio = 0.3;
    out.append(t);
    return out;
}

QVector<SheetFrameTemplate> loadFrameTemplates(const QString& path) {
    const QString file = path.isEmpty() ? frameTemplatesFilePath() : path;
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly))
        return builtinTemplates();                    // 缺失 → 内置默认
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isObject())
        return builtinTemplates();                    // 损坏 → 内置默认，绝不抛出
    const QJsonArray arr = doc.object()["templates"].toArray();
    QVector<SheetFrameTemplate> out;
    for (const auto& v : arr) {
        const QJsonObject jo = v.toObject();
        SheetFrameTemplate t;
        t.name     = jo["name"].toString();
        t.opts     = optsFromJson(jo["options"].toObject());
        t.scaleNum = jo["scaleNum"].toDouble(1.0);
        t.scaleDen = jo["scaleDen"].toDouble(1.0);
        if (t.scaleDen <= 0.0) t.scaleDen = 1.0;      // 守卫：除零比例回落 1:1
        if (!t.name.isEmpty())
            out.append(t);
    }
    return out.isEmpty() ? builtinTemplates() : out;
}

bool saveFrameTemplates(const QVector<SheetFrameTemplate>& templates,
                        const QString& path) {
    const QString file = path.isEmpty() ? frameTemplatesFilePath() : path;
    QJsonArray arr;
    for (const auto& t : templates) {
        QJsonObject jo;
        jo["name"]     = t.name;
        jo["options"]  = optsToJson(t.opts);
        jo["scaleNum"] = t.scaleNum;
        jo["scaleDen"] = t.scaleDen;
        arr.append(jo);
    }
    QJsonObject root;
    root["formatVersion"] = 1;
    root["templates"] = arr;

    // 原子写：QSaveFile 内部用临时文件，commit() 时 rename 覆盖（与 AutoSave 快照同策略）。
    // ⚠️ 不手写 QTemporaryFile + QFile::rename：MinGW/Windows 上 QTemporaryFile 默认
    // autoRemove（delete-on-close 语义）会阻止手动 rename，导致保存静默失败（已用探针验证）。
    QSaveFile sf(file);
    if (!sf.open(QIODevice::WriteOnly)) return false;
    const QByteArray bytes = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (sf.write(bytes) != bytes.size()) return false;
    return sf.commit();
}

} // namespace SketchCAD
