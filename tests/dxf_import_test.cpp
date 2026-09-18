// DXF **读入**路径的互操作测试。
//
// 为什么单独建这个测试：此前的 DXF 往返测试一直是「自己写、自己读」——那只验证了内部自洽。
// 而读入端至今只吃过自己写的文件，本程序写 DXF 时用了一批**自己顺手**的写法
// （坐标 6 位小数、CRLF、自定图层名、ARC 角度直接写度数、多段线用 bulge 表达弧段……），
// 真实世界的 DXF 由 AutoCAD / LibreCAD / ezdxf 写出，在这些维度上未必一致。
// 所以这里分两条线：
//   甲）自研往返：证明「存盘→打开」对**本程序自己的写法**是无损的；
//   乙）第三方互操作：读 ezdxf 写出的样例（CRLF 与 LF 两种换行、GBK 中文、句柄组码 5、
//       Z 组码 30 等本程序从不写的成分），证明读入端不依赖「自己写的才读得懂」。
//
// 样例由 tools/make_foreign_dxf.py 生成并提交进仓库，测试本身不依赖 Python。
#include <QApplication>
#include <QFile>
#include <QString>
#include <cstdio>
#include <cmath>
#include <vector>
#include "core/CadFormats.h"
#include "core/Element.h"
#include "core/Geometry.h"

using namespace SketchCAD;

static int g_fail = 0;
static void check(bool ok, const QString& what, const QString& detail = QString()) {
    printf("  [%s] %s%s\n", ok ? "PASS" : "FAIL", qPrintable(what),
           detail.isEmpty() ? "" : qPrintable(QStringLiteral(" (") + detail + QStringLiteral(")")));
    if (!ok) ++g_fail;
}
static void info(const QString& s) { printf("  [INFO] %s\n", qPrintable(s)); }

static QByteArray readFile(const QString& path, bool* ok = nullptr) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) { if (ok) *ok = false; return {}; }
    const QByteArray b = f.readAll();
    f.close();
    if (ok) *ok = true;
    return b;
}

static const Element* firstOf(const std::vector<Element>& v, ElementType t) {
    for (const auto& e : v) if (e.type == t) return &e;
    return nullptr;
}
static int countOf(const std::vector<Element>& v, ElementType t) {
    int n = 0; for (const auto& e : v) if (e.type == t) ++n; return n;
}
static bool near(double a, double b, double tol = 1e-6) { return std::fabs(a - b) < tol; }

// ---------------------------------------------------------------------------
// 甲）自研往返：带圆弧段的多段线必须无损（弧段不能退化成直线段）
// ---------------------------------------------------------------------------
static void testOwnRoundTrip() {
    printf("\n--- 甲）自研往返：带圆弧段的多段线 ---\n");
    // 模型对弧段的表示法：arcFlags[i]=true 表示 pts[i]→pts[i+1] 是圆弧，
    // 圆由「三相邻点外接圆」决定（geo::arcThrough）。故 pts 三个点必须都在该圆上。
    // 下面这条：P0→P1 直线，P1→P2 圆弧（圆 = P0P1P2 外接圆）。这是本程序自己的惯用写法。
    Element pl;
    pl.type = ElementType::Polyline;
    pl.pts = { Vec3(0, 0, 0), Vec3(10, 0, 0), Vec3(10, 10, 0) };
    pl.arcFlags = { false, true };
    pl.closed = false;
    pl.visible = true;
    pl.layer = QStringLiteral("0");
    pl.name = QStringLiteral("圆角");

    Element ci; ci.type = ElementType::Circle; ci.pts = { Vec3(50, 50, 0) }; ci.radius = 20.0;
    ci.visible = true; ci.name = QStringLiteral("圆");
    Element ar; ar.type = ElementType::Arc; ar.pts = { Vec3(10, 10, 0) }; ar.radius = 5.0;
    ar.startAngle = 10.0; ar.endAngle = 80.0; ar.visible = true; ar.name = QStringLiteral("弧");
    Element tx; tx.type = ElementType::Text; tx.pts = { Vec3(0, -20, 0) };
    tx.text = QStringLiteral("内部样例"); tx.radius = 5.0; tx.visible = true; tx.name = QStringLiteral("字");

    const std::vector<Element> src = { pl, ci, ar, tx };
    const QByteArray dxf = cadfmt::toDxf(src);
    check(!dxf.isEmpty(), QStringLiteral("自研导出 DXF 非空"));

    // 导出端应把弧段写成 bulge（组码 42）——这是「保留真实弧段而不是离散成折线」的凭据
    const QString text = QString::fromLatin1(dxf);
    check(text.contains(QStringLiteral("\r\n42\r\n")), QStringLiteral("导出把弧段写成 bulge（组码 42）"));

    std::vector<Element> back;
    QString err;
    cadfmt::DxfImportStats st;
    check(cadfmt::fromDxf(dxf, back, &err, &st), QStringLiteral("能读回自研导出的 DXF"), err);

    const Element* p2 = firstOf(back, ElementType::Polyline);
    check(p2 != nullptr, QStringLiteral("读回得到多段线"));
    if (p2) {
        check(p2->pts.size() == 3, QStringLiteral("多段线顶点数保持 3"),
              QStringLiteral("实际 %1").arg(p2->pts.size()));
        check(near(p2->pts[1].x, 10) && near(p2->pts[1].y, 0) && near(p2->pts[2].y, 10),
              QStringLiteral("多段线顶点坐标保持"));
        // 关键断言：弧段必须还在。旧实现忽略组码 42 → 弧段退化成直线段（存盘再打开形状就变了）。
        const bool flagKept = p2->arcFlags.size() > 1 && p2->arcFlags[1];
        check(flagKept, QStringLiteral("弧段标记被还原（bulge → arcFlags，不能退化为直线）"),
              QStringLiteral("arcFlags.size=%1 [1]=%2")
                  .arg(p2->arcFlags.size())
                  .arg(p2->arcFlags.size() > 1 ? (int)p2->arcFlags[1] : -1));
        const geo::ArcSeg a0 = geo::arcThrough(src[0].pts, 1, false);
        const geo::ArcSeg a1 = geo::arcThrough(p2->pts, 1, false);
        info(QStringLiteral("弧段半径：原始 %1 → 读回 %2；sweep %3° → %4°")
                 .arg(a0.radius, 0, 'f', 3).arg(a1.radius, 0, 'f', 3)
                 .arg(a0.sweepDeg, 0, 'f', 3).arg(a1.sweepDeg, 0, 'f', 3));
        check(a1.valid && near(a1.radius, a0.radius, 1e-6) && near(a1.sweepDeg, a0.sweepDeg, 1e-6),
              QStringLiteral("弧段几何（半径/扫掠角）无损"));
    }
    const Element* c2 = firstOf(back, ElementType::Circle);
    check(c2 && near(c2->radius, 20.0) && near(c2->pts[0].x, 50.0), QStringLiteral("圆半径/圆心保持"));
    const Element* a2 = firstOf(back, ElementType::Arc);
    check(a2 && near(a2->radius, 5.0) && near(a2->startAngle, 10.0) && near(a2->endAngle, 80.0),
          QStringLiteral("圆弧半径/起止角保持（度制不换算）"));
    const Element* t2 = firstOf(back, ElementType::Text);
    check(t2 && t2->text == QStringLiteral("内部样例"), QStringLiteral("文本内容保持"),
          t2 ? t2->text : QStringLiteral("<无>"));
    check(st.arcsKept >= 1 && st.arcsDropped == 0,
          QStringLiteral("统计：弧段还原成功且无丢弃"),
          QStringLiteral("kept=%1 dropped=%2").arg(st.arcsKept).arg(st.arcsDropped));
}

// ---------------------------------------------------------------------------
// 乙）第三方互操作：读 ezdxf 写出的样例
// ---------------------------------------------------------------------------
static void testForeign(const QString& path, bool lfOnly) {
    printf("\n--- 乙）外部来源样例（%s）---\n", qPrintable(path));
    bool ok = false;
    const QByteArray raw = readFile(path, &ok);
    check(ok && !raw.isEmpty(), QStringLiteral("样例文件可读"), path);
    if (!ok || raw.isEmpty()) return;

    // 外部文件的「非自研成分」自检：句柄组码 5、Z 组码 30、CRLF/LF 差异
    const QString t = QString::fromLatin1(raw);
    check(t.contains(QStringLiteral("\r\n5\r\n")) || t.contains(QStringLiteral("\n5\n")),
          QStringLiteral("样例含句柄组码 5（本程序从不写出，可验证读入端容错）"));
    if (lfOnly) check(!raw.contains("\r\n"), QStringLiteral("样例确为纯 LF 换行（对抗性）"));
    else        check(raw.contains("\r\n"),  QStringLiteral("样例确为 CRLF 换行（AutoCAD 习惯）"));

    std::vector<Element> v;
    QString err;
    cadfmt::DxfImportStats st;
    check(cadfmt::fromDxf(raw, v, &err, &st), QStringLiteral("能读入外部 DXF"), err);
    info(QStringLiteral("读入图元 %1 个（line=%2 circle=%3 arc=%4 polyline=%5 text=%6）")
             .arg(v.size()).arg(countOf(v, ElementType::Line)).arg(countOf(v, ElementType::Circle))
             .arg(countOf(v, ElementType::Arc)).arg(countOf(v, ElementType::Polyline))
             .arg(countOf(v, ElementType::Text)));
    check(v.size() == 6, QStringLiteral("图元数与样例一致（6）"),
          QStringLiteral("实际 %1").arg(v.size()));

    const Element* ln = firstOf(v, ElementType::Line);
    check(ln && near(ln->pts[0].x, 0) && near(ln->pts[1].x, 100), QStringLiteral("直线端点正确"),
          ln ? QStringLiteral("(%1,%2)-(%3,%4)").arg(ln->pts[0].x).arg(ln->pts[0].y)
                   .arg(ln->pts[1].x).arg(ln->pts[1].y) : QString());
    check(ln && ln->layer == QStringLiteral("OUTLINE"), QStringLiteral("直线图层名读入正确"),
          ln ? ln->layer : QString());

    const Element* ci = firstOf(v, ElementType::Circle);
    check(ci && near(ci->radius, 20.0) && near(ci->pts[0].x, 50.0) && near(ci->pts[0].y, 50.0),
          QStringLiteral("圆心/半径正确（CENTER 图层）"),
          ci ? QStringLiteral("c=(%1,%2) r=%3").arg(ci->pts[0].x).arg(ci->pts[0].y).arg(ci->radius) : QString());
    check(ci && ci->layer == QStringLiteral("CENTER"), QStringLiteral("圆图层名读入正确"));

    const Element* arc = firstOf(v, ElementType::Arc);
    check(arc && near(arc->radius, 5.0) && near(arc->startAngle, 10.0) && near(arc->endAngle, 80.0),
          QStringLiteral("圆弧半径与起止角正确"),
          arc ? QStringLiteral("r=%1 %2°→%3°").arg(arc->radius).arg(arc->startAngle).arg(arc->endAngle) : QString());

    // 中文文本：R12 靠 $DWGCODEPAGE=ANSI_936 + GBK 字节，读入端必须正确解码
    bool foundCjk = false, foundAscii = false;
    for (const auto& e : v) {
        if (e.type != ElementType::Text) continue;
        if (e.text == QStringLiteral("外部样例图")) foundCjk = true;
        if (e.text == QStringLiteral("R12 IMPORT")) foundAscii = true;
    }
    check(foundCjk, QStringLiteral("中文文本按 GBK 正确解码（无乱码）"));
    check(foundAscii, QStringLiteral("ASCII 文本保持"));

    const Element* pl = firstOf(v, ElementType::Polyline);
    check(pl && pl->pts.size() == 4 && pl->closed, QStringLiteral("多段线顶点数与闭合标志正确"),
          pl ? QStringLiteral("n=%1 closed=%2").arg(pl->pts.size()).arg((int)pl->closed) : QString());
    check(pl && near(pl->pts[0].x, 200) && near(pl->pts[1].x, 240) && near(pl->pts[2].y, 40),
          QStringLiteral("多段线顶点坐标正确"));

    // bulge 处理：本模型的弧段是「三相邻点定圆」，与外部 bulge 的「起点 + 凸度」参数化并不等价。
    // 样例（tools/make_foreign_dxf.py）在段 0 上给了 bulge=0.41421356（即 90° 圆弧）。
    // 经实测，该弧与本模型三相邻点定圆给出的是**镜像**关系（幅值同为 90°、方向相反），
    // 故本模型无法表达它。契约是：能一致还原才还原，否则**如实退回直线段**并计入 dropped
    // ——宁可少画一段弧，也绝不画出方向错误的圆弧（那比直线更糟）。
    if (pl) {
        constexpr double kBulge0 = 0.41421356;      // 与生成脚本一致
        const double wantDeg = 4.0 * std::atan(kBulge0) * 180.0 / M_PI;
        const int nSegs = pl->closed ? (int)pl->pts.size() : (int)pl->pts.size() - 1;
        // 不变式 1：凡被标为弧段的段，其几何必须与 bulge 一致（绝不产生错误圆弧）
        bool neverWrong = true;
        int flagged = 0;
        for (int i = 0; i < nSegs; ++i) {
            const bool f = i < (int)pl->arcFlags.size() && pl->arcFlags[i];
            if (!f) continue;
            ++flagged;
            const geo::ArcSeg as = geo::arcThrough(pl->pts, i, pl->closed);
            if (!as.valid || std::fabs(as.sweepDeg - wantDeg) > 0.5) neverWrong = false;
        }
        const geo::ArcSeg as0 = geo::arcThrough(pl->pts, 0, pl->closed);
        info(QStringLiteral("外部多段线 %1 段：标为弧段 %2 段；统计 kept=%3 dropped=%4")
                 .arg(nSegs).arg(flagged).arg(st.arcsKept).arg(st.arcsDropped));
        info(QStringLiteral("段 0：bulge 期望 sweep %1°；三相邻点定圆给出 %2°（方向相反 = 不等价）")
                 .arg(wantDeg, 0, 'f', 3).arg(as0.valid ? as0.sweepDeg : 0.0, 0, 'f', 3));
        check(neverWrong, QStringLiteral("凡被标为弧段的段，几何都与 bulge 一致（绝不画出错误圆弧）"));
        // 不变式 2：带 bulge 的段必须被如实计数（既不静默丢弃，也不虚报还原）
        check(st.arcsKept + st.arcsDropped == 1,
              QStringLiteral("含 bulge 的段数被如实计数（kept+dropped = 1）"),
              QStringLiteral("kept=%1 dropped=%2").arg(st.arcsKept).arg(st.arcsDropped));
        check(flagged == st.arcsKept, QStringLiteral("统计与实际标注的弧段数一致"));
        check((pl->arcFlags.empty() || !pl->arcFlags[0]),
              QStringLiteral("不等价的 bulge 已退回直线段（记录为 dropped，非静默丢弃）"));
    }
    check(st.entities == v.size(), QStringLiteral("统计的图元数与实际一致"),
          QStringLiteral("%1 vs %2").arg(st.entities).arg(v.size()));
}

// ---------------------------------------------------------------------------
// 丙）容错与失败路径
// ---------------------------------------------------------------------------
static void testRobustness() {
    printf("\n--- 丙）容错与失败路径 ---\n");
    // 极简文件：无 HEADER / 无 TABLES / 无句柄，只有一眼 ENTITIES
    const QByteArray minimal =
        "0\r\nSECTION\r\n2\r\nENTITIES\r\n0\r\nLINE\r\n8\r\n0\r\n10\r\n0.0\r\n20\r\n0.0\r\n"
        "11\r\n5.0\r\n21\r\n0.0\r\n0\r\nENDSEC\r\n0\r\nEOF\r\n";
    std::vector<Element> v; QString err;
    check(cadfmt::fromDxf(minimal, v, &err), QStringLiteral("极简 DXF（无 HEADER/TABLES）可读入"), err);
    check(v.size() == 1 && v[0].type == ElementType::Line, QStringLiteral("极简 DXF 读出 1 条直线"));

    std::vector<Element> v2;
    check(!cadfmt::fromDxf(QByteArray(), v2, &err), QStringLiteral("空数据返回失败"));
    check(!err.isEmpty(), QStringLiteral("空数据时给出错误说明"));
    std::vector<Element> v3;
    check(!cadfmt::fromDxf(QByteArray("这不是 DXF"), v3, &err), QStringLiteral("非 DXF 文本返回失败"));
    std::vector<Element> v4;
    const QByteArray noEnt = "0\r\nSECTION\r\n2\r\nHEADER\r\n0\r\nENDSEC\r\n0\r\nEOF\r\n";
    check(!cadfmt::fromDxf(noEnt, v4, &err), QStringLiteral("无 ENTITIES 段返回失败"), err);
    std::vector<Element> v5;
    const QByteArray emptyEnt = "0\r\nSECTION\r\n2\r\nENTITIES\r\n0\r\nENDSEC\r\n0\r\nEOF\r\n";
    check(!cadfmt::fromDxf(emptyEnt, v5, &err), QStringLiteral("ENTITIES 段为空返回失败"), err);
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    printf("=== DXF 读入互操作测试 ===\n");
    testOwnRoundTrip();
    testForeign(QStringLiteral("tests/data/foreign_ezdxf_r12.dxf"), false);

    // LF 与 CRLF 两种换行必须得到完全相同的结果（读入端不得依赖某种换行）
    {
        std::vector<Element> a, b; QString e1, e2;
        cadfmt::DxfImportStats s1, s2;
        const bool k1 = cadfmt::fromDxf(readFile(QStringLiteral("tests/data/foreign_ezdxf_r12.dxf")), a, &e1, &s1);
        const bool k2 = cadfmt::fromDxf(readFile(QStringLiteral("tests/data/foreign_lf_only.dxf")), b, &e2, &s2);
        printf("\n--- LF-only 与 CRLF 等价性 ---\n");
        check(k1 && k2 && a.size() == b.size() && s1.arcsKept == s2.arcsKept,
              QStringLiteral("LF-only 与 CRLF 文件读入结果一致"),
              QStringLiteral("%1/%2 个，kept %3/%4").arg(a.size()).arg(b.size())
                  .arg(s1.arcsKept).arg(s2.arcsKept));
        bool same = (a.size() == b.size());
        for (size_t i = 0; same && i < a.size(); ++i) {
            same = a[i].type == b[i].type && a[i].text == b[i].text &&
                   near(a[i].pts[0].x, b[i].pts[0].x) && near(a[i].pts[0].y, b[i].pts[0].y) &&
                   near(a[i].radius, b[i].radius) && a[i].layer == b[i].layer;
        }
        check(same, QStringLiteral("逐图元字段一致（类型/坐标/半径/图层/文本）"));
    }
    testRobustness();

    printf("\nRESULT: %s\n", g_fail == 0 ? "PASS" : "FAIL");
    return g_fail == 0 ? 0 : 1;
}
