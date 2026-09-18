#pragma once
#include <vector>
#include <QString>
#include <QRectF>
#include "Element.h"
#include "Vec.h"
#include "SheetSpec.h"

namespace SketchCAD {

class SketchModel;

// 箱体视图模板（参照用户提供的箱体零件图附图）参数化实现。
//
// 附图视图表达（本模板逐项对应）：
//   * 主视图——全剖：竖直圆筒形箱壁 + 底板，剖切面过轴线；剖面线由参数化
//     剖面绘制接口（SectionViewBuilder）生成；两侧画剖切符号 A-A；
//   * A—A 断面图——回转体横断面（环形），壁上均布 3×Ø7 小孔（makeRingSection 生成）；
//   * 俯视图 B——圆角矩形底板 + 4×M4-7H 螺孔 + 圆筒内外圆（视图名 B）；
//   * C 向视图——侧向凸台端面：Ø34 外圆、Ø26 孔、4×M4-7H（视图名 C）；
//   * 表面粗糙度符号（GB/T 131-2006）：腔底 Ra 1.6、底板上表面 Ra 6.3、右上角「其余」；
//   * 技术要求文本（未注圆角 R3 / 未注倒角 C1）。
//
// 几何结构（简化自附图、保持视图表达完整，单位 mm）：
//   * 竖直圆筒：外径 outerD（Ø82），内径 boreD（Ø64H8），坐于底板上、顶部开口；
//     筒腔底 = 底板顶面；轴线竖直。
//   * 底板：baseW(X) × baseL(Y) × baseThk，四角 4×M4-7H 螺孔（俯视图 B 表达）；
//     圆筒外径大于底板宽（盆形悬出，与附图主视图下部收窄一致）。
//   * 侧向凸台：右侧筒壁上、轴距底面 bossAxisH（附图 39）；外径 bossD（Ø34）、
//     轴向孔径 bossBoreD（Ø26）、端面 4×M4-7H（C 向视图表达）。
//
// 结构不变式（违反时 makeValid() 自动收敛）：
//   1. outerD > boreD > 0（壁厚为正）；
//   2. bossBoreD < bossD，且壁孔径 wallHoleD < 壁厚 − 2；
//   3. bossAxisH ∈ (bossD/2, overallH − bossD/2)（凸台完整落在筒身上）；
//   4. 底板螺孔内接于底板四角（边距 ≥ 4mm）。
struct HousingParams {
    // ---- 圆筒形箱壁 ----
    double outerD = 82.0;               // 筒体外径（附图 Ø82）
    double boreD  = 64.0;               // 主孔直径（附图 Ø64H8）
    QString boreTol = QStringLiteral("H8");  // 主孔配合代号（附图 H8；空=不带公差）

    // ---- 底板 ----
    double baseW   = 46.0;              // 底板宽（沿 X，附图 46）
    double baseL   = 70.0;              // 底板深（沿 Y，附图 70）
    double baseThk =  7.0;              // 底板厚（附图 7）
    double overallH = 70.0;             // 总高（附图 70）
    double cornerR =  3.0;              // 底板圆角 R3（技术要求 1）
    QString mountHoleSpec = QStringLiteral("4×M4-7H");   // 底板螺孔（附图 4×M4-7H）

    // ---- 侧向凸台（C 向）----
    double bossD      = 34.0;           // 凸台外径（附图 Ø34）
    double bossBoreD  = 26.0;           // 凸台轴向孔（附图 Ø26）
    double bossAxisH  = 39.0;           // 凸台轴距底面（附图 39）
    double bossLen    = 12.0;           // 凸台轴向伸出长
    double bossBoltCircle = 30.0;       // 凸台螺孔分布圆（介于 Ø26 与 Ø34 之间）
    QString bossHoleSpec = QStringLiteral("4×M4-7H");    // 凸台端面螺孔

    // ---- A—A 断面壁孔 ----
    int    wallHoleCount = 3;           // 壁上小孔数（附图 3×Ø7）
    double wallHoleD     =  7.0;        // 壁孔径

    // ---- 图纸布置 ----
    double sheetW = 420.0;              // 图纸宽（默认 A3 横向）
    double sheetH = 297.0;
    double marginL = 25.0;
    double marginC =  5.0;
    bool   autoScale = true;
    double manualScale = 1.0;
    double gap = 15.0;                  // 视图间最小间隙（mm）
    double dimTextHeight = 3.5;

    double titleBlockW = 180.0;         // 标题栏避让
    double titleBlockH = 56.0;

    static HousingParams forA3() { return HousingParams(); }

    // ---- 派生量（模型 mm）----
    double outerR() const { return outerD * 0.5; }
    double boreR()  const { return boreD  * 0.5; }
    double wallThk()const { return (outerD - boreD) * 0.5; }
    double bossLenMax() const { return wallThk() + bossLen; }  // 凸台总轴向长（含壁内段）

    void makeValid();
    QString validate() const;
    double viewScale() const;
    QRectF viewsBounds() const;
    QRectF availableArea() const;
};

struct HousingViewsResult {
    std::vector<Element> elems;         // 全部图元（图纸坐标 mm，Y 向上）
    double scale = 1.0;
    QString scaleLabel;
    bool   fits = false;
    QRectF bounds;
    QRectF area;
    // 视图锚点（图纸 mm，便于测试断言）
    double frontCx = 0, frontCy = 0;    // 主视图（全剖）圆筒轴心
    double topCx   = 0, topCy   = 0;    // 俯视图 B 中心
    double aaCx    = 0, aaCy    = 0;    // A—A 断面圆心
    double cCx     = 0, cCy     = 0;    // C 向视图圆心
    int    hatchCount = 0;              // 剖面区域数（主视图 + A—A）
    int    dimCount   = 0;              // 尺寸标注数
    int    roughnessCount = 0;          // 粗糙度符号图元组数（=1 组/处）
    int    cutSymbolCount = 0;          // 剖切符号图元数
};

// 生成箱体视图（图元未写入模型；坐标系与 buildSheetFrame() 一致，可直接叠加）。
HousingViewsResult buildHousingViews(const HousingParams& p, const SketchModel* model);

// 视图名（"主视图" / "俯视图 B" / "A—A 断面" / "C 向视图"）。
QString housingViewTitle(int index);

} // namespace SketchCAD
