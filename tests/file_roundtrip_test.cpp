// 图纸文件「新建 → 存盘 → 读回」端到端往返测试。
//
// 为什么需要这个测试：新建/打开/保存原先全靠模态对话框取参数，离屏测试无法驱动，
// 于是这层胶水长期零覆盖——一次重构中 MainWindow::saveFile 的写盘段落被整段截断，
// 没有任何测试变红（写盘段没了依然能编译通过），只靠人工读代码才发现。
// 现在把「参数已确定之后」那一段抽成 MainWindow::applyNewDrawing / writeDrawing /
// readDrawing 三个无对话框接缝，本测试直接驱动它们跑完整往返。
//
// 覆盖：四种格式（JSON/DXF/STEP/DWG）+ 未知扩展名兜底、比例随图往返、
//       标题栏比例联动、撤销粒度、以及各类失败路径必须返回 false。
#include <QApplication>
#include <QFileInfo>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <cmath>
#include <algorithm>
#include "ui/MainWindow.h"
#include "ui/GraphicsView.h"
#include "core/SketchModel.h"
#include "core/SheetSpec.h"
#include "core/ElementFactory.h"
#include "core/Geometry.h"
#include "core/CadFormats.h"

using namespace SketchCAD;

static int g_fail = 0;
static void check(bool ok, const QString& what, const QString& detail = QString()) {
    if (ok) { qInfo().noquote() << "  [PASS]" << what; }
    else    { qInfo().noquote() << "  [FAIL]" << what << (detail.isEmpty() ? QString() : "(" + detail + ")"); ++g_fail; }
}

static qint64 fileSize(const QString& p) { return QFileInfo(p).size(); }
static QByteArray readAll(const QString& p) {
    QFile f(p); return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}
// 标题栏「比例」栏当前文本
static QString tbScale(SketchModel* m) {
    for (const auto& e : m->elements())
        if (e.name == QStringLiteral("tb:field:scale")) return e.text;
    return QString();
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);

    MainWindow win;
    win.show();                       // offscreen 下不真开屏
    SketchModel* m = win.model();
    check(m != nullptr, QStringLiteral("MainWindow 暴露了模型（可测接缝）"));
    if (!m) { qInfo().noquote() << QStringLiteral("RESULT: FAIL"); return 1; }

    // ---- 1. 新建图纸：默认 1:1、图框与标题栏就位、整张图只占一次撤销 ----
    win.applyNewDrawing(SheetSpec::make(SheetFormat::A3, true), 1.0);
    const int frameCount = (int)m->elements().size();
    check(frameCount > 0, QStringLiteral("新建 A3 图纸后图框已生成"),
          QStringLiteral("%1 个图元").arg(frameCount));
    check(std::fabs(m->scaleRatio() - 1.0) < 1e-9 && m->scaleLabel() == QStringLiteral("1:1"),
          QStringLiteral("新建后比例为 1:1"), m->scaleLabel());
    check(tbScale(m) == QStringLiteral("1:1"),
          QStringLiteral("标题栏「比例」栏预填 1:1"), tbScale(m));
    m->undo();
    check(m->elements().empty(),
          QStringLiteral("一次撤销即可撤掉整张图框（图框只占一次撤销）"),
          QStringLiteral("剩余 %1").arg(m->elements().size()));

    // ---- 2. 按比例新建：1:2 时图框本身不缩放、标题栏写 1:2 ----
    win.applyNewDrawing(SheetSpec::make(SheetFormat::A3, true), 0.5);
    check(m->scaleLabel() == QStringLiteral("1:2"), QStringLiteral("按 1:2 新建后比例标签为 1:2"));
    check(tbScale(m) == QStringLiteral("1:2"),
          QStringLiteral("标题栏「比例」栏写 1:2"), tbScale(m));
    const int frameAtHalf = (int)m->elements().size();
    check(frameAtHalf == frameCount,
          QStringLiteral("图框/标题栏代表纸张，按比例新建时不改变其图元数"),
          QStringLiteral("%1 vs %2").arg(frameAtHalf).arg(frameCount));

    // 加一条直线 + 一个对齐标注（走模型的公开写接口）。
    // 注意语义：当前比例是 1:2，所以 pts 是「图纸坐标」——画 100 个图纸单位，
    // 对应实物尺寸 100 / 0.5 = 200。后面的标注数值断言就建立在这个换算上，
    // 这样一次断言同时守住了「几何按图纸坐标」与「标注按实物尺寸」两件事。
    m->beginEdit(QStringLiteral("测试图元"));
    {
        std::vector<Element> extra;
        extra.push_back(ElementFactory::createLine(Vec3(0, 0, 0), Vec3(100, 0, 0), nullptr));
        m->addElementsQuiet(extra);
        Element d = ElementFactory::create(ElementType::Dimension,
                                          QString::fromUtf8(dimensionTypeName(DimensionType::Aligned)), nullptr);
        d.dtype = DimensionType::Aligned;
        d.pts = {Vec3(0, 0, 0), Vec3(100, 0, 0)};
        m->addElementsQuiet({d});
    }
    check((int)m->elements().size() == frameAtHalf + 2,
          QStringLiteral("测试图元已加入（直线 + 对齐标注）"));

    // ---- 3. JSON 完整往返：图元数、比例、标注数值都要对 ----
    const QString jsonPath = QStringLiteral("build/rt_roundtrip.json");
    QString err;
    check(win.writeDrawing(jsonPath, &err), QStringLiteral("writeDrawing 写出 JSON 成功"), err);
    check(fileSize(jsonPath) > 0, QStringLiteral("JSON 文件非空（写盘段落没有被截断）"),
          QStringLiteral("%1 字节").arg(fileSize(jsonPath)));
    {
        // 直接验证落盘内容自洽：这是「写盘段被截断」这类缺陷的唯一可靠守卫
        const QJsonObject root = QJsonDocument::fromJson(readAll(jsonPath)).object();
        check(root.contains("elements") && root.contains("scaleRatio"),
              QStringLiteral("JSON 含 elements 与 scaleRatio 字段"));
        check(root["elements"].toArray().size() == (int)m->elements().size(),
              QStringLiteral("JSON 里的图元数与模型一致"),
              QStringLiteral("%1 vs %2").arg(root["elements"].toArray().size()).arg(m->elements().size()));
        check(std::fabs(root["scaleRatio"].toDouble() - 0.5) < 1e-9,
              QStringLiteral("JSON 里记下了当前比例 0.5"));
    }

    const int before = (int)m->elements().size();
    check(win.readDrawing(jsonPath, &err), QStringLiteral("readDrawing 读回 JSON 成功"), err);
    check((int)m->elements().size() == before,
          QStringLiteral("JSON 往返后图元数不变"),
          QStringLiteral("%1 vs %2").arg(m->elements().size()).arg(before));
    check(std::fabs(m->scaleRatio() - 0.5) < 1e-9,
          QStringLiteral("JSON 往返后绘图比例恢复为 1:2（不二次缩放）"), m->scaleLabel());
    check(tbScale(m) == QStringLiteral("1:2"),
          QStringLiteral("读档后标题栏比例栏同步为 1:2"), tbScale(m));

    // ---- 4. DXF：写出可读回，且标注数字是实物尺寸 ----
    const QString dxfPath = QStringLiteral("build/rt_roundtrip.dxf");
    check(win.writeDrawing(dxfPath, &err), QStringLiteral("writeDrawing 写出 DXF 成功"), err);
    const QByteArray dxfBytes = readAll(dxfPath);
    check(dxfBytes.startsWith("0\r\nSECTION"), QStringLiteral("DXF 以 SECTION 开头"));
    check(dxfBytes.contains("\r\nAC1009\r\n"), QStringLiteral("DXF 声明 R12 (AC1009)"));
    // 1:2 图上画了 100 图纸单位 → 标注数字必须是实物 200.00（不是 100、也不是 200 的图纸值）
    check(dxfBytes.contains("\r\n1\r\n200.00\r\n"),
          QStringLiteral("DXF 标注数字是实物尺寸 200.00（图纸 100 ÷ 0.5，比例随导出传入）"));
    check(win.readDrawing(dxfPath, &err), QStringLiteral("readDrawing 读回 DXF 成功"), err);
    check((int)m->elements().size() > 0, QStringLiteral("DXF 导入后有图元"),
          QStringLiteral("%1 个").arg(m->elements().size()));

    // ---- 5. STEP：只写；内容自洽（ISO 头 + 结束标记 + 括号平衡）----
    const QString stepPath = QStringLiteral("build/rt_roundtrip.step");
    check(win.writeDrawing(stepPath, &err), QStringLiteral("writeDrawing 写出 STEP 成功"), err);
    const QString stepTxt = QString::fromUtf8(readAll(stepPath));
    check(stepTxt.startsWith(QStringLiteral("ISO-10303-21;")),
          QStringLiteral("STEP 以 ISO-10303-21 开头"));
    check(stepTxt.trimmed().endsWith(QStringLiteral("END-ISO-10303-21;")),
          QStringLiteral("STEP 以 END-ISO-10303-21 结束（文件写完没有截断）"));
    check(stepTxt.count(QLatin1Char('(')) == stepTxt.count(QLatin1Char(')')),
          QStringLiteral("STEP 括号平衡"));
    check(!win.readDrawing(stepPath, &err), QStringLiteral("STEP 不支持读回（返回 false）"), err);

    // ---- 6. DWG：可写，但内容是 DXF 兼容（不伪造二进制头）----
    // 必须用「同一份模型状态」各写一次 DXF 和 DWG 才能逐字节比较：
    // 上面的 DXF 已经被 readDrawing 读回（标注按几何炸开），模型已不是当初那份。
    const QString dxfPath2 = QStringLiteral("build/rt_roundtrip2.dxf");
    check(win.writeDrawing(dxfPath2, &err), QStringLiteral("（同一模型状态下）再写一份 DXF"), err);
    const QByteArray dxfBytes2 = readAll(dxfPath2);

    const QString dwgPath = QStringLiteral("build/rt_roundtrip.dwg");
    check(win.writeDrawing(dwgPath, &err), QStringLiteral("writeDrawing 写出 DWG 成功"), err);
    const QByteArray dwgBytes = readAll(dwgPath);
    check(dwgBytes == dxfBytes2,
          QStringLiteral("同一模型下 DWG 与 DXF 内容完全一致（未链接 ODA，不做伪二进制头）"),
          QStringLiteral("dwg=%1B dxf=%2B").arg(dwgBytes.size()).arg(dxfBytes2.size()));
    check(dwgBytes.startsWith("0\r\nSECTION"),
          QStringLiteral("DWG 文件是文本 DXF 内容而非伪造的二进制头"));

    // ---- 7. 未知扩展名兜底为草图 JSON ----
    const QString oddPath = QStringLiteral("build/rt_roundtrip.notacad");
    check(win.writeDrawing(oddPath, &err), QStringLiteral("未知扩展名仍能保存（兜底 JSON）"), err);
    check(QJsonDocument::fromJson(readAll(oddPath)).isObject(),
          QStringLiteral("未知扩展名的落盘内容是可解析的 JSON"));

    // ---- 8. 打开外部来源的 DXF：图元导入 + 视图适配到内容包围盒 ----
    // 需求「文件存储支持 dxf」包含**打开**。这里读的是 ezdxf（第三方实现）写出的样例，
    // 且刻意把图形画在远离原点处（x≈200..240）——若打开后仍按原点 zoom=4 居中，
    // 画面就是一片空白，用户会以为"打不开"。故断言整幅图确实落在视口内。
    {
        QString err2;
        check(win.readDrawing(QStringLiteral("tests/data/foreign_ezdxf_r12.dxf"), &err2),
              QStringLiteral("能打开外部（ezdxf 写出）的 DXF"), err2);
        check(m->elements().size() == 6, QStringLiteral("外部 DXF 导入 6 个图元"),
              QStringLiteral("实际 %1").arg(m->elements().size()));

        QRectF bb;
        bool have = false;
        for (const auto& e : m->elements()) {
            const QRectF r = geo::elementBounds(e);
            bb = have ? bb.united(r) : r;
            have = true;
        }
        GraphicsView* v = win.view();
        check(v && v->viewport() && v->viewport()->width() > 0, QStringLiteral("视图可用"));
        if (v && v->viewport() && v->viewport()->width() > 0 && have) {
            const QPointF p0 = v->mapFromScene(bb.topLeft());
            const QPointF p1 = v->mapFromScene(bb.bottomRight());
            const double x0 = std::min(p0.x(), p1.x()), x1 = std::max(p0.x(), p1.x());
            const double y0 = std::min(p0.y(), p1.y()), y1 = std::max(p0.y(), p1.y());
            const double vw = v->viewport()->width(), vh = v->viewport()->height();
            qInfo().noquote() << QStringLiteral("  [INFO] 导入内容包围盒在屏幕 %1,%2 ~ %3,%4（视口 %5x%6）")
                                     .arg(x0, 0, 'f', 1).arg(y0, 0, 'f', 1)
                                     .arg(x1, 0, 'f', 1).arg(y1, 0, 'f', 1).arg(vw).arg(vh);
            check(x1 > 1 && y1 > 1 && x0 < vw - 1 && y0 < vh - 1,
                  QStringLiteral("打开外部 DXF 后内容可见（未被甩到视口外）"),
                  QStringLiteral("x[%1..%2] y[%3..%4] 视口 %5x%6")
                      .arg(x0, 0, 'f', 1).arg(x1, 0, 'f', 1)
                      .arg(y0, 0, 'f', 1).arg(y1, 0, 'f', 1).arg(vw).arg(vh));
            check(x0 >= -1 && y0 >= -1 && x1 <= vw + 1 && y1 <= vh + 1,
                  QStringLiteral("整幅图完整落入视口（fitToRect 到内容包围盒）"));
        }
    }

    // ---- 9. 失败路径 ----
    check(!win.readDrawing(QStringLiteral("build/definitely_missing_file.json"), &err),
          QStringLiteral("读取不存在的文件返回 false"));
    check(!win.writeDrawing(QStringLiteral("build"), &err),
          QStringLiteral("写到目录路径（不可写）返回 false"));

    qInfo().noquote() << (g_fail == 0 ? QStringLiteral("RESULT: PASS") : QStringLiteral("RESULT: FAIL"));
    return g_fail == 0 ? 0 : 1;
}
