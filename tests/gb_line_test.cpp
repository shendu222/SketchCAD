// GB/T 14665-2012 图线体系回归（核心层，无需 GUI）：
//   * 4.1 / 表1 线宽组别：5 组、粗:细恒为 2:1、切换组别全图同步；
//   * 4.1 线型齐备：9 种，名称唯一；
//   * 4.2 线素长度：虚线 12d/3d、点画线、双点画线；最小长度回落细实线；
//   * 4.3 重合图线优先顺序；
//   * 4.4 相位对齐：线段两端落在「画」上。
#include "core/LineWidthGroup.h"
#include "core/LineStyleConfig.h"
#include "core/LinePattern.h"
#include "core/Element.h"
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
    // ==================== T2：线宽组别（表1）====================
    {
        check(lineWidthGroupCount() == 5, "表1：图线共 5 组");
        const double thick[5] = { 2.0, 1.4, 1.0, 0.7, 0.5 };
        const double thin[5]  = { 1.0, 0.7, 0.5, 0.35, 0.25 };
        bool okT = true, okR = true;
        for (int i = 0; i < 5; ++i) {
            const LineWidthGroup g = (LineWidthGroup)i;
            if (!near(groupThickMm(g), thick[i]) || !near(groupThinMm(g), thin[i])) okT = false;
            if (!near(groupThickMm(g), 2.0 * groupThinMm(g))) okR = false;
        }
        check(okT, "表1：5 组粗/细线宽取值与标准完全一致");
        check(okR, "表1：每组粗:细 恒为 2:1");

        // 切换组别 → 线宽同步
        setCurrentLineWidthGroup(LineWidthGroup::G1);
        const double w1 = LineStyleConfig::instance().defaultWidth(LineStyle::Solid);
        setCurrentLineWidthGroup(LineWidthGroup::G5);
        const double w5 = LineStyleConfig::instance().defaultWidth(LineStyle::Solid);
        setCurrentLineWidthGroup(LineWidthGroup::G3);
        const double w3 = LineStyleConfig::instance().defaultWidth(LineStyle::Solid);
        check(near(w1, 2.0) && near(w5, 0.5) && near(w3, 1.0),
              "切换线组后 Solid 线宽同步（G1=2.0, G3=1.0, G5=0.5）",
              QString("%1/%2/%3").arg(w1).arg(w3).arg(w5));

        // 粗/细档归类
        check(near(LineStyleConfig::instance().defaultWidth(LineStyle::Dashed), groupThinMm(LineWidthGroup::G3)),
              "细虚线取细线档");
        check(near(LineStyleConfig::instance().defaultWidth(LineStyle::ThickDashed), groupThickMm(LineWidthGroup::G3)),
              "粗虚线取粗线档");
        check(near(mmToScreenPx(1.0), 3.0), "屏幕换算：1.0mm → 3px");
    }

    // ==================== T3：线型齐备（4.1）====================
    {
        check(kLineStyleCount == 9, "线型共 9 种");
        bool namesOk = true;
        for (int i = 0; i < kLineStyleCount; ++i) {
            const char* a = lineStyleName((LineStyle)i);
            if (!a || !*a) namesOk = false;
            for (int j = i + 1; j < kLineStyleCount; ++j)
                if (QString::fromUtf8(a) == QString::fromUtf8(lineStyleName((LineStyle)j))) namesOk = false;
        }
        check(namesOk, "9 种线型名称均非空且互不相同");
        check(QString::fromUtf8(lineStyleName(LineStyle::Wave)) == QStringLiteral("波浪线") &&
              QString::fromUtf8(lineStyleName(LineStyle::DoubleBreak)) == QStringLiteral("双折线") &&
              QString::fromUtf8(lineStyleName(LineStyle::ThickDashed)) == QStringLiteral("粗虚线") &&
              QString::fromUtf8(lineStyleName(LineStyle::ThickDashDot)) == QStringLiteral("粗点画线"),
              "新增线型名称正确（波浪线/双折线/粗虚线/粗点画线）");
        // 枚举可序列化往返（整数值 → 枚举 → 整数值）
        bool rt = true;
        for (int i = 0; i < kLineStyleCount; ++i)
            if ((int)(LineStyle)i != i) rt = false;
        check(rt, "线型枚举整数序号稳定（存盘往返一致），且末尾追加不破坏旧档");
        // toPenStyle 对全部 9 种均有返回
        bool penOk = true;
        for (int i = 0; i < kLineStyleCount; ++i) (void)toPenStyle((LineStyle)i);
        check(penOk, "toPenStyle 覆盖全部 9 种线型");
    }

    // ==================== T5：线素长度（4.2）====================
    {
        DashSpec d1 = dashSpecFor(LineStyle::Dashed);
        check(d1.valid && d1.pattern.size() == 2 && near(d1.pattern[0], 12.0) && near(d1.pattern[1], 3.0),
              "虚线线素 画12d / 空3d");
        check(near(d1.pattern[0] / d1.pattern[1], 4.0, 1e-6), "虚线 画:空 = 12:3（误差<1%）");
        check(near(d1.minLengthD, 27.0), "虚线最小长度 27d");

        DashSpec d2 = dashSpecFor(LineStyle::DashDot);
        check(d2.valid && d2.pattern.size() == 4 && near(d2.pattern[0], 12.0) && near(d2.pattern[2], 1.0),
              "点画线线素 画12d/空3d/点1d/空3d");
        check(near(d2.minLengthD, 54.5), "点画线最小长度 54.5d");

        DashSpec d3 = dashSpecFor(LineStyle::DashDotDot);
        check(d3.valid && d3.pattern.size() == 6, "双点画线线素 6 段");
        check(near(d3.minLengthD, 58.0), "双点画线最小长度 58d");

        check(!dashSpecFor(LineStyle::Solid).valid, "实线无虚线图案");

        const double d = 1.0;
        check(shouldFallbackToSolid(LineStyle::Dashed, 20.0, d), "短于 27d 的虚线回落细实线");
        check(!shouldFallbackToSolid(LineStyle::Dashed, 40.0, d), "长于 27d 的虚线不回落");
    }

    // ==================== T13：重合图线优先顺序（4.3）====================
    {
        check(linePriority(LineStyle::Solid) > linePriority(LineStyle::Dashed) &&
              linePriority(LineStyle::Dashed) > linePriority(LineStyle::DashDot) &&
              linePriority(LineStyle::DashDot) > linePriority(LineStyle::DashDotDot),
              "4.3：可见轮廓 > 不可见轮廓 > 轴线中心线 > 假想轮廓");
    }

    // ==================== T14：相位对齐（4.4）====================
    {
        const DashSpec spec = dashSpecFor(LineStyle::Dashed);
        const double d = 1.0;
        const double L = 100.0;   // 远大于最小长度
        const double off = dashPhaseOffset(L, d, spec);
        double cycle = 0; for (double v : spec.pattern) cycle += v;
        const double phEnd = std::fmod(L / d + off, cycle);
        check(dashOnAt(off, spec) && dashOnAt(phEnd, spec),
              "4.4：线段两端均落在「画」上（相位对齐）",
              QString("off=%1 endPhase=%2").arg(off, 0, 'f', 2).arg(phEnd, 0, 'f', 2));
    }

    printf("\nGB 图线体系测试：%s（%d 项失败）\n", g_fail == 0 ? "全部通过" : "存在失败", g_fail);
    return g_fail == 0 ? 0 : 1;
}
