// 环形阵列中心点「双模式指定」回归测试：
//   * 手动输入坐标 (X, Y) 与 视图拾取 两种方式都收敛到同一对 X/Y 输入框（effectiveCenter 单一事实来源）；
//   * 两种模式可自由切换，切换时不丢失、不串扰中心点数值；
//   * 视图拾取 setCenter() 写入世界坐标并切到「视图拾取」显示态；手动 setManualCenter() 写入并切回手动；
//   * accept() 统一从输入框读取中心点（两种模式驱动的生成结果一致）。
// 说明：本测试只验证中心点双模式的分发与一致性（纯 UI 状态机），
//       实际「绕该中心生成环形阵列」的几何正确性由 array_test.cpp 覆盖。
#include <QApplication>
#include <cmath>
#include "ui/ArrayDialog.h"
#include "core/Vec.h"

using namespace SketchCAD;

static int g_fail = 0;
static void check(bool ok, const QString& what) {
    if (ok) qInfo().noquote() << "  [PASS]" << what;
    else    { qInfo().noquote() << "  [FAIL]" << what; ++g_fail; }
}
static bool near2(double a, double b, double eps = 1e-6) { return std::fabs(a - b) <= eps; }

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    const double eps = 1e-6;

    // ---------- 1) 默认：手动输入模式，中心 = 默认包围盒中心 ----------
    {
        ArrayDialog dlg(ArrayDialog::Polar, Vec3(1.0, 2.0, 0.0), nullptr);
        check(!dlg.isCenterPickMode(), "默认处于「手动输入」模式");
        Vec3 c = dlg.effectiveCenter();
        check(near2(c.x, 1.0, eps) && near2(c.y, 2.0, eps),
              "默认中心点 = 传入的默认中心 (1,2)");
    }

    // ---------- 2) 手动输入路径：键入 X/Y 直接驱动中心点 ----------
    {
        ArrayDialog dlg(ArrayDialog::Polar, Vec3(0.0, 0.0, 0.0), nullptr);
        dlg.setManualCenter(5.0, 7.0);
        check(!dlg.isCenterPickMode(), "setManualCenter 后回到手动输入模式");
        Vec3 c = dlg.effectiveCenter();
        check(near2(c.x, 5.0, eps) && near2(c.y, 7.0, eps),
              "手动输入写入中心 (5,7)");
    }

    // ---------- 3) 视图拾取路径：setCenter 写入世界坐标并切到拾取显示态 ----------
    {
        ArrayDialog dlg(ArrayDialog::Polar, Vec3(0.0, 0.0, 0.0), nullptr);
        dlg.setCenter(Vec3(3.0, 4.0, 0.0));
        check(dlg.isCenterPickMode(), "setCenter 后切到「视图拾取」模式");
        Vec3 c = dlg.effectiveCenter();
        check(near2(c.x, 3.0, eps) && near2(c.y, 4.0, eps),
              "视图拾取写入中心 (3,4)");
    }

    // ---------- 4) 自由切换：手动↔拾取 不丢值、不串扰 ----------
    {
        ArrayDialog dlg(ArrayDialog::Polar, Vec3(0.0, 0.0, 0.0), nullptr);
        dlg.setManualCenter(5.0, 7.0);
        check(near2(dlg.effectiveCenter().x, 5.0, eps) &&
              near2(dlg.effectiveCenter().y, 7.0, eps), "手动态中心 (5,7)");

        // 切到视图拾取（用 setCenter 模拟一次拾取）
        dlg.setCenter(Vec3(11.0, 12.0, 0.0));
        check(dlg.isCenterPickMode(), "已切到视图拾取");
        check(near2(dlg.effectiveCenter().x, 11.0, eps) &&
              near2(dlg.effectiveCenter().y, 12.0, eps), "拾取态中心 (11,12)");

        // 切回手动输入：中心点数值应保持（输入框为单一事实来源）
        dlg.setCenterMode(false);
        check(!dlg.isCenterPickMode(), "切回「手动输入」模式");
        check(near2(dlg.effectiveCenter().x, 11.0, eps) &&
              near2(dlg.effectiveCenter().y, 12.0, eps),
              "切回手动后中心点数值保持一致 (11,12)");

        // 再切到拾取：数值仍不变
        dlg.setCenterMode(true);
        check(dlg.isCenterPickMode(), "再次切到视图拾取");
        check(near2(dlg.effectiveCenter().x, 11.0, eps) &&
              near2(dlg.effectiveCenter().y, 12.0, eps),
              "再次拾取态中心仍一致 (11,12)");
    }

    // ---------- 5) accept() 统一从输入框读取中心点（两条路径驱动结果相同）----------
    {
        ArrayDialog dlg(ArrayDialog::Polar, Vec3(0.0, 0.0, 0.0), nullptr);
        dlg.setCenter(Vec3(9.0, -2.0, 0.0));   // 视图拾取路径确定的中心
        dlg.accept();                            // 通过校验（count=8, angle=360 合法）
        ArrayDialog::Result r = dlg.result();
        check(r.mode == ArrayDialog::Polar, "accept 后模式为 Polar");
        check(near2(r.center.x, 9.0, eps) && near2(r.center.y, -2.0, eps),
              "accept 后的中心点 = 视图拾取写入的 (9,-2)");
    }

    // ---------- 6) 矩形阵列对话框的中心点字段同样可用且默认 (0,0) ----------
    {
        ArrayDialog dlg(ArrayDialog::Rectangular, Vec3(0.0, 0.0, 0.0), nullptr);
        check(!dlg.isCenterPickMode(), "矩形阵列默认也处于手动输入模式");
        Vec3 c = dlg.effectiveCenter();
        check(near2(c.x, 0.0, eps) && near2(c.y, 0.0, eps),
              "矩形阵列中心点字段存在且默认 (0,0)");
    }

    if (g_fail) { qWarning() << "ArrayDialogTest FAILED:" << g_fail; return 1; }
    qInfo() << "ArrayDialogTest ALL PASS";
    return 0;
}
