// 剖面填充「图形级」人工核验（离屏渲染 PNG）
// ----------------------------------------------------------------------------
// 目的：核心层断言只证明"数据正确"，用户看到的却是"画面"。本程序把用户真实场景
// 渲染成 PNG，直观看问题是否解决。
//
// 用户真实截图（2026-09-18 07:54 上传）是一个【梯形】：
//   左 边 竖直(x=0)，右 边 向右上方倾斜，上边微升、下边水平；
//   中间一条竖直分割线（中点处，贯穿上下边）。
// 本程序按该几何构造，用多种画法（四条 Line 围成 / 单个 Polygon）复现，
// 验证填左半时：只填左侧、右半留空、分割线保留不剪切。
#include "core/Section.h"
#include "core/SketchModel.h"
#include "core/Modify.h"
#include "core/Geometry.h"
#include "core/Element.h"
#include "core/Vec.h"
#include "ui/GraphicsView.h"
#include <QApplication>
#include <QImage>
#include <QPainter>
#include <QString>
#include <cstdio>
#include <cmath>

using namespace SketchCAD;

static Element makeLine(const Vec3& a, const Vec3& b, int id) {
    Element e; e.type = ElementType::Line; e.pts = { a, b }; e.id = id; return e;
}
static Element makePolygon(const std::vector<Vec3>& pts, int id) {
    Element e; e.type = ElementType::Polygon; e.closed = true; e.pts = pts; e.id = id; return e;
}
static QImage renderView(GraphicsView* v, const QString& dump) {
    QImage img(v->viewport()->size(), QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::white);
    { QPainter p(&img); v->viewport()->render(&p); }
    img.save(dump);
    return img;
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);

    printf("=== 剖面填充图形级核验(离屏渲染 PNG) ===\n");

    // 新截图(8:16)：分隔线是【斜线】(从上边中间斜到下边中间，端点落在上下边上)。
    // 用矩形 + 斜线分隔来复现这个此前未覆盖的情形。
    const Vec3 A(0, 0, 0), B(80, 0, 0), C(80, 40, 0), D(0, 40, 0);
    const auto sepDiag = makeLine(Vec3(30, 0, 0), Vec3(50, 40, 0), 65);  // 斜线，端点落在底/顶边

    const auto elems4line = std::vector<Element>{
        makeLine(A, B, 61), makeLine(B, C, 62),
        makeLine(C, D, 63), makeLine(D, A, 64),
        sepDiag
    };
    const auto elemsPoly = std::vector<Element>{
        makePolygon({ A, B, C, D }, 71),
        makeLine(Vec3(30, 0, 0), Vec3(50, 40, 0), 72)
    };
    // 关键假设：图形是【单个多边形、6个顶点】，分隔线两端点恰是多边形的两顶点
    // （下边中点和上边中点是多边形顶点）。此时端点判定会把切分误跳过去 → 整块=1环。
    const auto elemsHex = std::vector<Element>{
        makePolygon({ Vec3(0,0,0), Vec3(30,0,0), Vec3(80,0,0),
                      Vec3(80,40,0), Vec3(50,40,0), Vec3(0,40,0) }, 91),
        makeLine(Vec3(30, 0, 0), Vec3(50, 40, 0), 92)
    };

    const struct { const std::vector<Element>* ee; const char* nm; } modes[3] = {
        { &elems4line, "四条Line+斜线" },
        { &elemsPoly,  "单个Polygon+斜线" },
        { &elemsHex,   "六顶点多边形+端点重合" },
    };
    for (int mode = 0; mode < 3; ++mode) {
        const auto* ee = modes[mode].ee;
        const char* nm = modes[mode].nm;
        const auto set = buildSectionLoopSet(*ee, kDefaultClosureTol);
        printf("[%s] 闭环数 = %d\n", nm, (int)set.loops.size());
        for (size_t i = 0; i < set.loops.size(); ++i) {
            std::string ids;
            for (int id : set.loopSrcIds[i]) { if (!ids.empty()) ids += ","; ids += std::to_string(id); }
            printf("   环%02d area=%.1f srcIds=[%s]\n", (int)i, set.loopArea[i], ids.c_str());
        }
        const auto L = pickSectionRegionAt(*ee, Vec3(15, 20, 0));   // 左
        const auto R = pickSectionRegionAt(*ee, Vec3(65, 20, 0));   // 右
        printf("[%s] 左(15,20) area=%.1f ok=%d; 右(65,20) area=%.1f ok=%d\n",
               nm, L.area, (int)L.ok(), R.area, (int)R.ok());
    }

    const auto& elems = elemsHex;   // 用户截图(6控制点)对应画法

    auto dump = [](SketchModel& m, const QString& file) {
        GraphicsView view(&m);
        view.resize(900, 700);
        view.fitToRect(QRectF(-10, -10, 105, 62));
        view.show();
        QApplication::processEvents();
        renderView(&view, file);
    };

    auto fillLeftAt = [](SketchModel* m, const Vec3& p, const char* tag) {
        const std::vector<Element> srcs = m->elements();
        const auto pick = pickSectionRegionAt(srcs, p);
        printf("[INFO] %s 点(%.0f,%.0f) area=%.1f ok=%d\n", tag, p.x, p.y, pick.area, (int)pick.ok());
        const auto res = makeSectionChangeForPick(pick, srcs, SectionStyle());
        m->beginEdit(QStringLiteral("剖面填充"));
        modify::applyChanges(m, res.changes, QString());
        int rc = 0, lc = 0, hc = 0;
        for (const auto& e : m->elements()) {
            if (e.type == ElementType::Polygon) ++rc;
            else if (e.type == ElementType::Line) ++lc;
            else if (e.type == ElementType::Hatch) ++hc;
        }
        printf("[INFO] %s 后 poly=%d line=%d hatch=%d\n", tag, rc, lc, hc);
    };

    // 填充前
    {
        SketchModel m;
        for (const auto& e : elems) m.addElement(e);
        dump(m, QStringLiteral("build/section_verify_before.png"));
    }

    // 只填左侧
    {
        SketchModel m;
        for (const auto& e : elems) m.addElement(e);
        fillLeftAt(&m, Vec3(15, 20, 0), "填左");
        dump(m, QStringLiteral("build/section_verify_left.png"));
    }

    // 左侧填完后，右侧再独立填充（验证分割线仍在、右侧仍可填）
    {
        SketchModel m;
        for (const auto& e : elems) m.addElement(e);
        fillLeftAt(&m, Vec3(15, 20, 0), "填左");
        const std::vector<Element> after = m.elements();
        const auto pickR = pickSectionRegionAt(after, Vec3(65, 20, 0));
        printf("[INFO] 填左后点(48,16) area=%.1f ok=%d\n", pickR.area, (int)pickR.ok());
        if (pickR.ok()) {
            const auto resR = makeSectionChangeForPick(pickR, after, SectionStyle());
            m.beginEdit(QStringLiteral("剖面填充"));
            modify::applyChanges(&m, resR.changes, QString());
        }
        dump(m, QStringLiteral("build/section_verify_both.png"));
    }

    printf("已输出: build/section_verify_{before,left,both}.png\n");
    return 0;
}
