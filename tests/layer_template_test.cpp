// GB/T 14665-2012 第 8 章 表6「图线的分层标识」回归（核心层）。
// 验证 standardLayers()：12 个标准层的标识号 01~12、标准名、建议线型，
// 以及与 SketchModel::addLayer 的协作（预置图层表）。
#include "core/Layer.h"
#include "core/SketchModel.h"
#include "core/LineWidthGroup.h"
#include <cstdio>
#include <cmath>

using namespace SketchCAD;

static int g_fail = 0;
static void check(bool ok, const char* what, const QString& detail = QString()) {
    if (ok) printf("  [PASS] %s\n", what);
    else { printf("  [FAIL] %s %s\n", what,
                  detail.isEmpty() ? "" : ("(" + detail + ")").toUtf8().constData()); ++g_fail; }
}
static bool near(double a, double b, double e = 1e-9) { return std::fabs(a - b) <= e; }

int main() {
    const auto ls = standardLayers();

    check(ls.size() == 12, "表6：标准图层模板含 12 层", QString::number((int)ls.size()));

    bool codesOk = true;
    for (int i = 0; i < (int)ls.size(); ++i) {
        const QString exp = QString("%1").arg(i + 1, 2, 10, QChar('0'));   // 01..12
        if (ls[i].code != exp) codesOk = false;
    }
    check(codesOk, "标识号依次为 01~12");

    bool descOk = true;
    for (const auto& l : ls)
        if (l.standardName.isEmpty() || l.name.isEmpty()) descOk = false;
    check(descOk, "每层均有非空标准名与图层名");

    check(ls[0].code == QStringLiteral("01") && ls[0].standardName.contains(QStringLiteral("粗实线")),
          "01 = 粗实线");
    check(ls[3].code == QStringLiteral("04") && ls[3].standardName.contains(QStringLiteral("细虚线")),
          "04 = 细虚线");
    check(ls[4].standardName.contains(QStringLiteral("细点画线")),
          "05 = 细点画线");
    check(ls[5].standardName.contains(QStringLiteral("粗点画线")),
          "06 = 粗点画线");
    check(ls[11].code == QStringLiteral("12") && ls[11].standardName.contains(QStringLiteral("汉字")),
          "12 = 文本（汉字）");

    // 建议线型：01 粗实线=实线、04 细虚线=虚线、05 细点画线=点划线
    check(ls[0].lineStyle == LineStyle::Solid && ls[3].lineStyle == LineStyle::Dashed &&
          ls[4].lineStyle == LineStyle::DashDot,
          "建议线型映射正确（01 实线 / 04 虚线 / 05 点划线）");

    // 线宽按「当前线组」派生（GB/T 14665-2012 表1，单位 mm）：粗层取粗线、细层取细线
    check(near(ls[0].lineWidth, groupThickMm(currentLineWidthGroup())) &&
          near(ls[3].lineWidth, groupThinMm(currentLineWidthGroup())) &&
          ls[0].lineWidth > ls[3].lineWidth,
          "线宽按当前线组派生（粗层=粗线 / 细层=细线，mm）",
          QString("粗=%1 细=%2").arg(ls[0].lineWidth).arg(ls[3].lineWidth));

    // 图层名唯一
    bool uniq = true;
    for (int i = 0; i < (int)ls.size(); ++i)
        for (int j = i + 1; j < (int)ls.size(); ++j)
            if (ls[i].name == ls[j].name) uniq = false;
    check(uniq, "图层名互不相同（可作唯一键）");

    // 与模型协作：预置后 0 + 12 = 13 层
    SketchModel m;
    for (const Layer& l : ls) m.addLayer(l.name, l.color, l.lineWidth, l.lineStyle);
    check(m.layerCount() == 13, "预置后图层数 = 13（默认层 0 + 12 标准层）",
          QString::number(m.layerCount()));
    check(m.hasLayer(ls[0].name) && m.hasLayer(ls[11].name), "标准层已写入模型");

    printf("\n标准图层模板测试：%s（%d 项失败）\n", g_fail == 0 ? "全部通过" : "存在失败", g_fail);
    return g_fail == 0 ? 0 : 1;
}
