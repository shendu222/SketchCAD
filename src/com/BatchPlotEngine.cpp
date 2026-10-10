// ============================================================================
// 批量出图引擎 —— 实现
// ============================================================================
#include "BatchPlotEngine.h"
#include "ComError.h"
#include "core/SketchModel.h"
#include "core/Element.h"
#include "core/ElementValidate.h"   // 鲁棒性守卫层：读档前图元只读校验/丢弃/修复
#include "core/CadFormats.h"
#include "core/Units.h"
#include "ui/PrintEngine.h"

#include <QCoreApplication>
#include <QApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDateTime>
#include <QMutex>
#include <QMutexLocker>

#include <mutex>

namespace SketchCAD {
namespace comauto {

// ---- 进程级状态（全部受锁保护）----
namespace {

QMutex  g_qtInitMutex;                    // QApplication 懒初始化锁
std::mutex g_sinkMutex;                   // 事件槽读写锁
BatchPlotEngine::EventFn g_eventSink;     // 进程级唯一事件槽
std::mutex g_runMutex;                    // 批量执行串行化锁（并发调用稳定性核心）

void emitEvent(const QString& event, const QJsonObject& payload) {
    BatchPlotEngine::EventFn fn;
    {   // 锁内拷贝，锁外调用：避免回调重入造成死锁
        std::lock_guard<std::mutex> lk(g_sinkMutex);
        fn = g_eventSink;
    }
    if (fn) fn(event, payload);
}

// 任务结果 JSON（单条）
QJsonObject taskResultJson(const BatchTaskResult& r) {
    QJsonObject o;
    o["index"]     = r.index;
    o["source"]    = r.source;
    o["output"]    = r.output;
    o["ok"]        = r.ok;
    o["error"]     = r.error;
    o["bytes"]     = static_cast<double>(r.bytes);
    o["elapsedMs"] = static_cast<double>(r.elapsedMs);
    return o;
}

} // namespace

// ---------------------------------------------------------------------------
// 事件槽注册
// ---------------------------------------------------------------------------
void BatchPlotEngine::setEventSink(EventFn fn) {
    std::lock_guard<std::mutex> lk(g_sinkMutex);
    g_eventSink = std::move(fn);
}

// ---------------------------------------------------------------------------
// Qt 应用懒初始化：宿主进程已有实例（GUI 程序 / 自测）直接复用；
// 纯 COM 宿主（如 wscript.exe 注入的进程外场景）则补建一个 QApplication。
// ---------------------------------------------------------------------------
void BatchPlotEngine::ensureQtApplication() {
    QMutexLocker lock(&g_qtInitMutex);
    if (QCoreApplication::instance()) return;
    static int    argc = 1;
    static char   arg0[] = "SketchCADAutomation";
    static char*  argv[] = { arg0, nullptr };
    // 进程生命周期内只构造一次；static 存储期与进程共存亡
    static QApplication app(argc, argv);
    Q_UNUSED(app)
}

// ---------------------------------------------------------------------------
// 结果汇总 JSON
// ---------------------------------------------------------------------------
QJsonObject BatchResult::toJson() const {
    QJsonObject o;
    o["allOk"]     = allOk;
    o["total"]     = total;
    o["succeeded"] = succeeded;
    o["failed"]    = failed;
    QJsonArray arr;
    for (const auto& r : tasks) arr.append(taskResultJson(r));
    o["tasks"] = arr;
    return o;
}

// ---------------------------------------------------------------------------
// 无头读档：JSON 完整往返 / DXF / DWG（与 MainWindow::readDrawing 同语义，
// 但不触碰任何视图对象；缺键容缺，旧文件向后兼容）
// ---------------------------------------------------------------------------
bool BatchPlotEngine::loadDrawing(SketchModel& model, const QString& path, QString* err) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (err) *err = comErrorText(SCAD_E_FILE_NOT_FOUND) + QStringLiteral("：") + path;
        return false;
    }
    const QByteArray raw = f.readAll();
    const cadfmt::CadFormat fmt = cadfmt::formatFromPath(path);

    if (fmt == cadfmt::CadFormat::Step) {
        if (err) *err = comErrorText(SCAD_E_UNSUPPORTED_FMT);
        return false;
    }

    if (fmt == cadfmt::CadFormat::Dxf || fmt == cadfmt::CadFormat::Dwg) {
        std::vector<Element> imported;
        QString e;
        cadfmt::DxfImportStats st;
        const bool ok = (fmt == cadfmt::CadFormat::Dxf)
            ? cadfmt::fromDxf(raw, imported, &e, &st)
            : cadfmt::fromDwg(raw, imported, &e, &st);
        if (!ok) {
            if (err) *err = comErrorText(SCAD_E_PARSE_FAILED) + QStringLiteral("：") + e;
            return false;
        }
        // 跨单位交换：DXF 头 $INSUNITS ≠ mm 时整体换算到 mm（与交互读档同规则）
        if (fmt == cadfmt::CadFormat::Dxf) {
            const LengthUnit srcUnit = lengthUnitFromInsunits(st.insunits);
            if (lengthFactorToMm(srcUnit) != 1.0)
                scaleElementsToUnit(imported, srcUnit);
        }
        model.clear();
        // 鲁棒性守卫：解析得到的图元先做只读校验/丢弃/修复，避免损坏数据进入模型造成崩溃或画错。
        int robSkip = 0, robFix = 0; QString robRep;
        std::vector<SketchCAD::Element> kept = SketchCAD::filterElements(imported, &robSkip, &robFix, &robRep);
        if (robSkip > 0 || robFix > 0)
            appendLog(QString(), QStringLiteral("[robust] DXF/DWG 打开校验：%1 个损坏图元已跳过，%2 个已修复。%3")
                          .arg(robSkip).arg(robFix).arg(robRep));
        model.beginEdit(QStringLiteral("自动化导入"));
        model.addElementsQuiet(kept);
        model.setDrawingPath(path);
        return true;
    }

    // ---- 草图 JSON（原生格式，完整往返）----
    const QJsonDocument d = QJsonDocument::fromJson(raw);
    if (!d.isObject()) {
        if (err) *err = comErrorText(SCAD_E_PARSE_FAILED) + QStringLiteral("：不是有效的草图文件");
        return false;
    }
    const QJsonObject root = d.object();
    model.clear();
    std::vector<Element> loaded;
    for (const auto& v : root["elements"].toArray())
        loaded.push_back(::SketchCAD::elementFromJson(v.toObject()));
    // 鲁棒性守卫：解析得到的图元先做只读校验/丢弃/修复，避免损坏数据进入模型造成崩溃或画错。
    int robSkip = 0, robFix = 0; QString robRep;
    std::vector<Element> kept = SketchCAD::filterElements(loaded, &robSkip, &robFix, &robRep);
    if (robSkip > 0 || robFix > 0)
        appendLog(QString(), QStringLiteral("[robust] 草图打开校验：%1 个损坏图元已跳过，%2 个已修复。%3")
                      .arg(robSkip).arg(robFix).arg(robRep));
    model.beginEdit(QStringLiteral("自动化打开"));
    model.addElementsQuiet(kept);
    // 以下各表均容缺（旧文件无键回落默认），与交互读档逐项对齐
    model.setLayersFromJson(root["layers"].toArray());
    model.setBlocksFromJson(root["blocks"].toArray());
    model.setTextStylesFromJson(root["textStyles"].toArray());
    model.setDimStylesFromJson(root["dimStyles"].toObject());   // 旧文件无此键 → 单一默认 "Standard"
    model.setSavedFiltersFromJson(root["savedFilters"].toArray());
    model.setLayoutsFromJson(root);   // 布局表（旧文件缺键 → 空表回落模型空间打印，与交互读档对齐）
    // 与交互读档同源：这四项已改为随图属性（入撤销栈），此处必须抑制，
    // 否则一次"自动化打开"会被拆成多步撤销（调用方已用 beginEdit 建立唯一编辑步）。
    {
        SketchModel::UndoGuard guard(&model);
        model.setUnits(UnitsMeta::fromJson(root["units"].toObject()));
        model.setLimits(LimitsMeta::fromJson(root["limits"].toObject()));
        model.setSnapConfig(snap::SnapConfig::fromJson(root["snapConfig"].toObject()));
        model.setTrackConfig(snap::TrackConfig::fromJson(root["trackConfig"].toObject()));
    }
    model.setConstraintsFromJson(root["constraints"].toArray());
    model.setParamsFromJson(root["params"].toObject());
    model.setPropsFromJson(root["properties"].toObject());   // 自定义图纸特性（缺键 → 空表，与交互读档对齐）
    // 存档比例只作元数据恢复（几何已是成品，不再缩放一次）
    if (root.contains("scaleRatio"))
        model.setScaleRatioValue(root["scaleRatio"].toDouble(1.0));
    // 全局线型比例随图恢复（缺键 → 回落 1.0）
    if (root.contains("ltscale"))
        model.setLtscale(root["ltscale"].toDouble(1.0));
    model.setDrawingPath(path);
    return true;
}

// ---------------------------------------------------------------------------
// 无头导出：按扩展名分派 pdf/dxf/dwg/step/json（与 MainWindow 的
// writeDrawing/exportPdfDirect 同语义：BlockRef 先展开，导出物即所见）
// ctbText/preset 非空时叠加打印样式表（黑白出图），优先于 settings 内的 plotStyle。
// ---------------------------------------------------------------------------
bool BatchPlotEngine::exportDrawing(SketchModel& model, const QString& path,
                                    const QJsonObject& settings, QString* err,
                                    const QString& ctbText, const QString& preset) {
    if (path.isEmpty()) {
        if (err) *err = comErrorText(SCAD_E_BAD_ARG) + QStringLiteral("：输出路径为空");
        return false;
    }
    const QFileInfo fi(path);
    const QString suffix = fi.suffix().toLower();

    // ---- PDF 出图（QPdfWriter，offscreen 可行）----
    if (suffix == QLatin1String("pdf")) {
        // 发布一致性：任务未显式指定页面设置且图纸带布局表 → 按布局逐页出图，
        // 与 GUI「发布 / 导出 PDF（全部布局）」同走 PrintEngine::exportPdfLayouts
        // （页数一致、渲染同一 renderPageBody 裁决）。显式给 settings 时维持旧单页行为。
        if (settings.isEmpty() && model.layoutCount() > 0) {
            const int pages = PrintEngine::exportPdfLayouts(&model, path);
            if (pages <= 0) {
                if (err) *err = comErrorText(SCAD_E_PLOT_FAILED) + QStringLiteral("：") + path;
                return false;
            }
            return true;
        }
        PrintSettings s = PrintSettings::defaults();
        if (!settings.isEmpty())
            s.fromJson(settings);
        // 打印样式表透传：ctbText 优先于 preset；两者都空则沿用 settings/默认。
        if (!ctbText.isEmpty()) {
            PlotStyleTable t;
            QString e2;
            if (!PlotStyleTable::fromCtb(ctbText, &t, &e2)) {
                // 解析有瑕疵：已解析的行仍生效，仅记录警告，不阻断出图
                appendLog(QString(), QStringLiteral("[plotstyle] CTB 部分行解析失败：") + e2);
            }
            s.plotStyle = t;
            s.monochrome = true;   // 导入样式表即视为黑白出图意图
        } else if (!preset.isEmpty()) {
            if (preset.compare(QLatin1String("gb"), Qt::CaseInsensitive) == 0) {
                s.plotStyle = PlotStyleTable::gbPreset(currentLineWidthGroup());
                s.monochrome = true;
            } else if (preset.compare(QLatin1String("gbGray"), Qt::CaseInsensitive) == 0) {
                s.plotStyle = PlotStyleTable::gbPresetGrayDither();
                s.monochrome = true;
            } else if (preset.compare(QLatin1String("none"), Qt::CaseInsensitive) == 0) {
                s.plotStyle.clearAll();
                s.monochrome = false;
            }
        }
        if (!PrintEngine::exportPdf(&model, s, path)) {
            if (err) *err = comErrorText(SCAD_E_PLOT_FAILED) + QStringLiteral("：") + path;
            return false;
        }
        return true;
    }
    // ---- 图纸格式导出 ----
    cadfmt::CadFormat fmt = cadfmt::formatFromPath(path);
    if (fmt == cadfmt::CadFormat::Unknown)
        fmt = cadfmt::CadFormat::Json;

    // DXF/DWG/STEP 不认识 BlockRef：展开为真实子图元（导出物即所见）
    std::vector<Element> exp;
    exp.reserve(model.elements().size());
    for (const auto& e2 : model.elements()) {
        if (e2.type == ElementType::BlockRef) {
            const auto subs = model.expandBlockRef(e2);
            exp.insert(exp.end(), subs.begin(), subs.end());
        } else {
            exp.push_back(e2);
        }
    }

    QByteArray out;
    QString e;
    const double ds = model.scaleRatio();
    switch (fmt) {
    case cadfmt::CadFormat::Dxf: {
        // 原生导出：块/标注/填充以原生实体写出（与 GUI 侧 saveFileAs/exportDxf 同一行为）。
        // DWG/STEP 仍走预炸开的 exp（R12 二进制容器与 STEP 无块/标注语义）。
        cadfmt::DxfExportOptions no;
        no.nativeBlocks = no.nativeDims = no.nativeHatch = true;
        out = cadfmt::toDxf(model.elements(), &e, ds, insunitsCode(model.units().length),
                            &model.blockTable(), &no);
        break;
    }
    case cadfmt::CadFormat::Dwg:
        out = cadfmt::toDwg(exp, &e, ds);
        break;
    case cadfmt::CadFormat::Step:
        out = cadfmt::toStep(exp, nullptr, &e, ds).toUtf8();
        break;
    case cadfmt::CadFormat::Json:
    default: {
        fmt = cadfmt::CadFormat::Json;
        QJsonObject root;
        root["app"] = QStringLiteral("SketchCAD");
        root["formatVersion"] = 1;
        root["scaleRatio"] = model.scaleRatio();
        root["ltscale"] = model.ltscale();
        QJsonArray arr;
        for (const auto& e2 : model.elements())
            arr.append(::SketchCAD::elementToJson(e2));
        root["elements"]    = arr;
        root["layers"]      = model.layersToJson();
        root["blocks"]      = model.blocksToJson();
        root["textStyles"]  = model.textStylesToJson();
        root["dimStyles"]   = model.dimStylesToJson();
        root["savedFilters"] = model.savedFiltersToJson();
        root["layouts"]      = model.layoutsToJson();
        root["currentLayout"] = model.currentLayout();
        root["units"]       = model.units().toJson();
        root["limits"]      = model.limits().toJson();
        root["snapConfig"]  = model.snapConfig().toJson();
        root["trackConfig"] = model.trackConfig().toJson();
        root["constraints"] = model.constraintsToJson();
        root["params"]      = model.paramsToJson();
        out = QJsonDocument(root).toJson(QJsonDocument::Indented);
        break;
    }
    }
    if (out.isEmpty()) {
        if (err) *err = comErrorText(SCAD_E_WRITE_FAILED) + QStringLiteral("：") + e;
        return false;
    }
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        if (err) *err = comErrorText(SCAD_E_WRITE_FAILED) + QStringLiteral("：") + path;
        return false;
    }
    f.write(out);
    f.close();
    model.setDrawingPath(path);
    return true;
}

// ---------------------------------------------------------------------------
// 日志追加（一行一条；目录不存在则静默放弃——日志失败不影响出图本身）
// ---------------------------------------------------------------------------
void BatchPlotEngine::appendLog(const QString& logPath, const QString& line) {
    if (logPath.isEmpty()) return;
    QFile f(logPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        return;
    const QString stamped =
        QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"))
        + QLatin1Char(' ') + line + QLatin1Char('\n');
    f.write(stamped.toUtf8());
}

// ---------------------------------------------------------------------------
// 批量执行（真正的入口；进程级互斥保证串行）
// ---------------------------------------------------------------------------
BatchResult BatchPlotEngine::runTasks(const QVector<BatchTask>& tasks, const QString& logPath) {
    BatchResult result;
    result.total = tasks.size();

    // 进程级互斥：并发调用在锁上排队串行执行，杜绝双任务同时写同一输出
    std::lock_guard<std::mutex> runLock(g_runMutex);

    appendLog(logPath, QStringLiteral("批量开始：共 %1 个任务").arg(tasks.size()));

    for (int i = 0; i < tasks.size(); ++i) {
        const BatchTask& t = tasks[i];
        BatchTaskResult r;
        r.index  = i;
        r.source = t.source;
        r.output = t.output;

        emitEvent(QStringLiteral("onTaskStart"),
                  QJsonObject{ {"index", i}, {"source", t.source}, {"output", t.output} });

        const QDateTime t0 = QDateTime::currentDateTime();
        if (t.source.isEmpty() || t.output.isEmpty()) {
            r.error = comErrorText(SCAD_E_BAD_TASK);
        } else {
            ensureQtApplication();
            SketchModel model;   // 每任务独立模型，任务间零串扰
            QString e;
            if (!loadDrawing(model, t.source, &e)) {
                r.error = e;
            } else if (!exportDrawing(model, t.output, t.settings, &e, t.ctbText, t.preset)) {
                r.error = e;
            } else {
                r.ok = true;
                r.bytes = QFileInfo(t.output).size();
            }
        }
        r.elapsedMs = t0.msecsTo(QDateTime::currentDateTime());

        if (r.ok) ++result.succeeded; else ++result.failed;
        result.tasks.append(r);

        emitEvent(QStringLiteral("onTaskDone"), taskResultJson(r));
        appendLog(logPath, QStringLiteral("任务 %1/%2 %3 src=%4 out=%5 %6")
                      .arg(i).arg(tasks.size())
                      .arg(r.ok ? QStringLiteral("成功") : QStringLiteral("失败"))
                      .arg(r.source, t.output,
                           r.ok ? QStringLiteral("%1 字节").arg(r.bytes) : r.error));
    }

    result.allOk = (result.failed == 0);
    emitEvent(QStringLiteral("onBatchFinished"),
              QJsonObject{ {"allOk", result.allOk}, {"total", result.total},
                           {"succeeded", result.succeeded}, {"failed", result.failed} });
    appendLog(logPath, QStringLiteral("批量结束：总 %1 成功 %2 失败 %3")
                  .arg(result.total).arg(result.succeeded).arg(result.failed));
    return result;
}

// ---------------------------------------------------------------------------
// 任务列表 JSON 解析 + 执行
// ---------------------------------------------------------------------------
BatchResult BatchPlotEngine::runBatch(const QString& tasksJson, const QString& logPath) {
    const QJsonDocument d = QJsonDocument::fromJson(tasksJson.toUtf8());
    QJsonArray arr;
    if (d.isObject())
        arr = d.object()["tasks"].toArray();
    else if (d.isArray())
        arr = d.array();

    if (arr.isEmpty()) {
        // 解析失败或空列表：以单条失败任务的形式如实上报，便于外部系统定位
        BatchResult r;
        r.total = 1; r.failed = 1;
        BatchTaskResult bad;
        bad.index = 0;
        bad.error = comErrorText(SCAD_E_BAD_TASK)
            + QStringLiteral("：任务列表为空或 JSON 非法（期望 {\"tasks\":[..]} 或 [..]）");
        r.tasks.append(bad);
        appendLog(logPath, QStringLiteral("批量失败：") + bad.error);
        return r;
    }

    QVector<BatchTask> tasks;
    tasks.reserve(arr.size());
    for (const auto& v : arr) {
        const QJsonObject o = v.toObject();
        BatchTask t;
        t.source   = o["source"].toString();
        t.output   = o["output"].toString();
        t.settings = o["settings"].toObject();
        // 打印样式表透传：
        //   "styleTable": "gb" | "gbGray" | "none"        → 内置预设
        //   "styleTable": "# 注释...\nLAYER 0 WIDTH=0.35" → 简化 CTB 文本
        //   "preset": "gb"                                → 内置预设（等价字段）
        const QJsonValue stv = o["styleTable"];
        if (stv.isString()) {
            const QString st = stv.toString();
            // 含换行/空格/「LAYER|COLOR|STYLE|GRAYSCALE」关键字 → 视为 CTB 文本，否则预设名
            const bool looksCtb = st.contains(QLatin1Char('\n')) ||
                                  st.contains(QLatin1String("LAYER"), Qt::CaseInsensitive) ||
                                  st.contains(QLatin1String("COLOR"), Qt::CaseInsensitive) ||
                                  st.contains(QLatin1String("STYLE"), Qt::CaseInsensitive) ||
                                  st.contains(QLatin1String("GRAYSCALE"), Qt::CaseInsensitive);
            if (looksCtb) t.ctbText = st; else t.preset = st;
        }
        if (t.preset.isEmpty() && o["preset"].isString())
            t.preset = o["preset"].toString();
        tasks.append(t);
    }
    return runTasks(tasks, logPath);
}

} // namespace comauto
} // namespace SketchCAD
