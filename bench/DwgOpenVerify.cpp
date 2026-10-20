// DwgOpenVerify —— DWG 打开核验工具（离屏渲染 PNG，供与正确 CAD 截图逐图对比）
// ----------------------------------------------------------------------------
// 背景：C0层.dwg 在 SketchCAD 中打开后图形残缺错乱。根因是交互读档的 DWG 分支
// 只把图元放进模型，**没有注册 BLOCKS 段解析出的块定义**，导致 160 处 INSERT
// 引用全部悬空、块内容（标题栏/机柜/铜排/WAGO/*U## 动态块）整片不渲染。
//
// 本工具走与 MainWindow::readDrawingImpl 完全相同的读档链：
//   fromDwg → $INSUNITS 单位换算 → addBlockDef 注册块定义 → filterElements
//   → addElementsQuiet → GraphicsView 离屏渲染 PNG
// 用法：DwgOpenVerify <输入.dwg> [输出.png]
#include "core/CadFormats.h"
#include "core/SketchModel.h"
#include "core/ElementValidate.h"
#include "core/Geometry.h"
#include "core/Element.h"
#include "core/Block.h"
#include "core/Units.h"
#include "ui/GraphicsView.h"
#include <QApplication>
#include <QFile>
#include <QImage>
#include <QPainter>
#include <QString>
#include <algorithm>
#include <cstdio>

using namespace SketchCAD;

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    if (argc < 2) {
        printf("用法: DwgOpenVerify <输入.dwg> [输出.png]\n");
        return 2;
    }
    const QString inPath = QString::fromLocal8Bit(argv[1]);
    const QString outPath = (argc > 2) ? QString::fromLocal8Bit(argv[2])
                                       : QStringLiteral("build/dwg_open_verify.png");

    QFile f(inPath);
    if (!f.open(QIODevice::ReadOnly)) {
        printf("[FAIL] 无法读取 %s\n", qPrintable(inPath));
        return 1;
    }
    const QByteArray raw = f.readAll();

    std::vector<Element> imported;
    QString err;
    cadfmt::DxfImportStats st;
    if (!cadfmt::fromDwg(raw, imported, &err, &st)) {
        printf("[FAIL] fromDwg 解析失败: %s\n", qPrintable(err));
        return 1;
    }

    SketchModel model;
    // —— 以下与 MainWindow 读档分支逐行同规则 ——
    const LengthUnit srcUnit = lengthUnitFromInsunits(st.insunits);
    const bool rescaled = lengthFactorToMm(srcUnit) != 1.0;
    if (rescaled) scaleElementsToUnit(imported, srcUnit);
    for (const auto& b : st.blocks) model.addBlockDef(b);   // 关键：注册块定义
    // STYLE 文字样式表随档注册（与 MainWindow 读档分支同规则）：重名且内建字体为空、
    // 导入字体非空 → updateTextStyle 覆盖（否则全图文字退回系统默认字体）
    for (const auto& s : st.styles) {
        if (!model.hasTextStyle(s.name)) { model.addTextStyle(s); continue; }
        const TextStyle* old = model.findTextStyle(s.name);
        if (old && old->fontFamily.isEmpty() && !s.fontFamily.isEmpty())
            model.updateTextStyle(s);
    }
    // LAYER 表随档注册（与 MainWindow 读档分支同规则）：缺注册则 byLayer 图元层色丢失
    for (const auto& l : st.layers) {
        if (model.hasLayer(l.name))
            model.setLayerStyle(l.name, &l.color, &l.lineWidth, &l.lineStyle);
        else
            model.addLayer(l.name, l.color, l.lineWidth, l.lineStyle);
    }
    int robSkip = 0, robFix = 0; QString robRep;
    const std::vector<Element> kept = filterElements(imported, &robSkip, &robFix, &robRep);
    model.beginEdit(QStringLiteral("导入 DWG"));
    model.addElementsQuiet(kept);

    // 统计：图元数 / 块定义数 / 悬空块引用（引用了未定义块名的 BlockRef）
    int blockRefs = 0;
    std::vector<QString> dangling;
    for (const auto& e : kept) {
        if (e.type != ElementType::BlockRef) continue;
        ++blockRefs;
        bool found = false;
        for (const auto& d : model.blockTable())
            if (d.name == e.blockName) { found = true; break; }
        if (!found && std::find(dangling.begin(), dangling.end(), e.blockName) == dangling.end())
            dangling.push_back(e.blockName);
    }
    printf("[INFO] 导入图元=%d  块定义=%d  BlockRef引用=%d  跳过=%d 修复=%d\n",
           (int)kept.size(), (int)model.blockTable().size(), blockRefs, robSkip, robFix);
    for (const auto& s : dangling)
        printf("[WARN] 悬空块引用（无定义，内容不渲染）: %s\n", qPrintable(s));

    // 块表名单（与 DXF BLOCKS 段对比，查缺失注册）——SKETCHCAD_VERIFY_VERBOSE=1 时打印
    const bool verbose = qEnvironmentVariableIsSet("SKETCHCAD_VERIFY_VERBOSE");
    if (verbose)
        for (const auto& d : model.blockTable())
            printf("[BLOCKDEF] %s  实体=%d\n", qPrintable(d.name), (int)d.entities.size());

    // 逐 BlockRef 展开诊断：展开叶子数为 0 或包围盒异常（过小/为空）= 该引用内容缺失。
    // 问题项常显打印；完整清单仅 verbose。
    for (const auto& el : kept) {
        if (el.type != ElementType::BlockRef) continue;
        const auto subs = block::expandRef(el, model.blockTable());
        QRectF rb; bool rh = false;
        int leaves = 0, nested = 0;
        for (const auto& s2 : subs) {
            if (s2.type == ElementType::BlockRef) { ++nested; continue; }
            ++leaves;
            const QRectF r = geo::elementBounds(s2);
            if (r.isNull()) continue;
            rb = rh ? rb.united(r) : r; rh = true;
        }
        const char* flag = "";
        if (leaves == 0) flag = "  <== 展开为空!";
        else if (rh && (rb.width() < 1.0 || rb.height() < 1.0)) flag = "  <== 包围盒退化!";
        if (!flag && !verbose) continue;
        printf("[BLKREF] %-36s pos=(%.1f,%.1f) 展开叶子=%d 嵌套Ref=%d bbox=%s%s\n",
               qPrintable(el.blockName),
               el.pts.empty() ? 0.0 : el.pts[0].x, el.pts.empty() ? 0.0 : el.pts[0].y,
               leaves, nested,
               rh ? QString::fromLatin1("(%1,%2)-(%3,%4)")
                      .arg(rb.left(),0,'f',1).arg(rb.top(),0,'f',1)
                      .arg(rb.right(),0,'f',1).arg(rb.bottom(),0,'f',1)
                      .toLocal8Bit().constData() : "无",
               flag);
    }

    // 包围盒（BlockRef 按展开后几何计，对齐真实可见范围；boundsOf 是私有成员，这里自行展开）
    QRectF bb; bool haveBox = false;
    auto grow = [&](const QRectF& r) {
        if (r.isNull()) return;
        bb = haveBox ? bb.united(r) : r;
        haveBox = true;
    };
    for (const auto& el : kept) {
        if (el.type == ElementType::BlockRef) {
            for (const auto& sub : block::expandRef(el, model.blockTable()))
                if (sub.type != ElementType::BlockRef) grow(geo::elementBounds(sub));
        } else {
            grow(geo::elementBounds(el));
        }
    }
    printf("[INFO] 可见包围盒 = (%.1f, %.1f) .. (%.1f, %.1f)  尺寸 %.1f x %.1f\n",
           bb.left(), bb.top(), bb.right(), bb.bottom(), bb.width(), bb.height());
    if (!haveBox) { printf("[FAIL] 没有任何可见几何\n"); return 1; }

    // 离屏渲染（注意：必须先 show 让视口拿到最终尺寸，再 fitToRect——
    // 否则 fitWorldRect 用默认视口尺寸算 zoom/pan，画面缩放错乱）
    // 可选放大核验：SKETCHCAD_VERIFY_RECT="x,y,w,h"（世界 mm，只适配该区域）
    //              SKETCHCAD_VERIFY_SIZE="宽x高"（视口像素，默认 1800x1200）
    QRectF fitRect = bb;
    const QString rectEnv = qEnvironmentVariable("SKETCHCAD_VERIFY_RECT");
    if (!rectEnv.isEmpty()) {
        const QStringList parts = rectEnv.split(QLatin1Char(','));
        if (parts.size() == 4) {
            bool ok[4] = {false, false, false, false};
            const double v[4] = { parts[0].toDouble(&ok[0]), parts[1].toDouble(&ok[1]),
                                  parts[2].toDouble(&ok[2]), parts[3].toDouble(&ok[3]) };
            if (ok[0] && ok[1] && ok[2] && ok[3] && v[2] > 0.0 && v[3] > 0.0) {
                fitRect = QRectF(v[0], v[1], v[2], v[3]);
                printf("[INFO] 区域放大核验 rect=(%.1f,%.1f) %.1fx%.1f\n", v[0], v[1], v[2], v[3]);
            } else {
                printf("[WARN] SKETCHCAD_VERIFY_RECT 格式非法，忽略\n");
            }
        }
    }
    int vw = 1800, vh = 1200;
    const QString sizeEnv = qEnvironmentVariable("SKETCHCAD_VERIFY_SIZE");
    if (!sizeEnv.isEmpty()) {
        const QStringList parts = sizeEnv.split(QLatin1Char('x'));
        if (parts.size() == 2) {
            bool okw = false, okh = false;
            const int w = parts[0].toInt(&okw), h = parts[1].toInt(&okh);
            if (okw && okh && w >= 100 && h >= 100) { vw = w; vh = h; }
        }
    }
    GraphicsView view(&model);
    view.resize(vw, vh);
    view.show();
    QApplication::processEvents();
    view.fitToRect(fitRect);
    QApplication::processEvents();
    printf("[INFO] 视口 = %dx%d\n", view.viewport()->width(), view.viewport()->height());
    QImage img(view.viewport()->size(), QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::white);
    { QPainter p(&img); view.viewport()->render(&p); }
    if (!img.save(outPath)) { printf("[FAIL] 无法写出 %s\n", qPrintable(outPath)); return 1; }
    printf("[OK] 渲染输出 = %s\n", qPrintable(outPath));
    return 0;
}
