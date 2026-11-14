#include "core/standards/ToleranceDB.h"

#include <cmath>
#include <functional>
#include <QHash>

namespace SketchCAD {

namespace {

// 等级序号：IT01=0 … IT18=19
inline int gradeIndex(ITGrade g) { return static_cast<int>(g); }

// 尺寸段几何平均值 D（mm）：基本尺寸分段 (下, 上]，含 1..3150mm。
// 经 builder lambda 逐段 add 构建，规避巨型静态聚合初始化。
double sizeStepGeometricMean(double D) {
    if (D <= 0.0) D = 1e-3;                 // 退化保护：视为最小段起点
    // 尺寸段下界（首段下界取 1，避免 D=0 几何平均退化为 0）
    struct Step { double lo, hi; };
    static const QVector<Step> steps = [] {
        QVector<Step> v;
        auto add = [&](double lo, double hi) { v.append({lo, hi}); };
        add(1.0,   3.0); add(3.0,   6.0); add(6.0,  10.0); add(10.0, 18.0);
        add(18.0, 30.0); add(30.0, 50.0); add(50.0, 80.0); add(80.0, 120.0);
        add(120.0, 180.0); add(180.0, 250.0); add(250.0, 315.0); add(315.0, 400.0);
        add(400.0, 500.0); add(500.0, 630.0); add(630.0, 800.0); add(800.0, 1000.0);
        add(1000.0, 1250.0); add(1250.0, 1600.0); add(1600.0, 2000.0);
        add(2000.0, 2500.0); add(2500.0, 3150.0);
        return v;
    }();
    for (const Step& s : steps) {
        if (D > s.lo && D <= s.hi) return std::sqrt(s.lo * s.hi);
    }
    // 超出 3150：用最大段均值（合理外推，单调不减）
    const Step& last = steps.last();
    return std::sqrt(last.lo * last.hi);
}

// 公差单位 i（μm）：D 为尺寸段几何平均(mm)。
// 注：≤500mm 用 ISO 标准式；>500mm 沿用同一式作外推（大尺寸专用式可在后续精确化）。
inline double toleranceUnitI(double D) {
    return 0.45 * std::pow(D, 1.0 / 3.0) + 0.001 * D;
}

// IT5~IT18 等级系数（a·i）：与枚举序 IT5=6 … IT18=19 对应
double itGradeFactor(int idx) {
    // idx 6..19 → 系数 7,10,16,25,40,64,100,160,250,400,640,1000,1600,2500
    static const double k[14] = {7, 10, 16, 25, 40, 64, 100, 160, 250, 400, 640, 1000, 1600, 2500};
    return k[idx - 6];
}

// 标准公差（μm，未取整）：D 为尺寸段几何平均(mm)
double standardToleranceUm(double D, ITGrade g) {
    const int idx = gradeIndex(g);
    const double i = toleranceUnitI(D);
    if (idx == 0) return 0.3 + 0.008 * D;            // IT01
    if (idx == 1) return 0.5 + 0.012 * D;            // IT0
    if (idx == 2) return 0.8 + 0.020 * D;            // IT1
    const double IT1 = 0.8 + 0.020 * D;
    const double IT5 = 7.0 * i;
    if (idx == 3) return IT1 * std::pow(IT5 / IT1, 1.0 / 4.0);   // IT2
    if (idx == 4) return IT1 * std::pow(IT5 / IT1, 2.0 / 4.0);   // IT3
    if (idx == 5) return IT1 * std::pow(IT5 / IT1, 3.0 / 4.0);   // IT4
    return itGradeFactor(idx) * i;                              // IT5..IT18
}

// 轴基本偏差（μm）：字母大写 key；D=尺寸段几何平均(mm)，i=公差单位(μm)，IT=本等级标准公差(μm)，gidx=等级序号。
// 以 builder lambda 注册「字母 → 公式」，避免巨型聚合初始化；公式取自 ISO 286-1 / GB/T 1800.1 基本偏差表。
// 约定：a~g/j 的公式返回轴「上偏差 es」（a~g 为负、j 近零）；k~zc 返回轴「下偏差 ei」（为正）。
using ShaftDevFn = std::function<double(double D, double i, double IT, int gidx)>;

const QHash<QString, ShaftDevFn>& shaftDeviationTable() {
    static const QHash<QString, ShaftDevFn> tbl = [] {
        QHash<QString, ShaftDevFn> t;
        auto add = [&](const QString& k, ShaftDevFn f) { t.insert(k, f); };
        // —— 间隙配合（a~g）：ei 为负 ——
        add(QStringLiteral("A"),  [](double D,double,double,int){ return -(265 + 1.3 * D); });
        add(QStringLiteral("B"),  [](double D,double,double,int){ return -(140 + 0.85 * D); });
        add(QStringLiteral("C"),  [](double D,double,double,int){ return (D <= 40.0) ? -(52 + 0.2 * D) : -(95 + 0.8 * D); });
        add(QStringLiteral("CD"), [](double D,double,double,int){ return -std::sqrt((52 + 0.2 * D) * (16 * std::pow(D, 0.44))); });
        add(QStringLiteral("D"),  [](double D,double,double,int){ return -(16 * std::pow(D, 0.44)); });
        add(QStringLiteral("E"),  [](double D,double,double,int){ return -(11 * std::pow(D, 0.41)); });
        add(QStringLiteral("EF"), [](double D,double,double,int){ return -std::sqrt((11 * std::pow(D, 0.41)) * (5.5 * std::pow(D, 0.41))); });
        add(QStringLiteral("F"),  [](double D,double,double,int){ return -(5.5 * std::pow(D, 0.41)); });
        add(QStringLiteral("FG"), [](double D,double,double,int){ return -std::sqrt((5.5 * std::pow(D, 0.41)) * (2.5 * std::pow(D, 0.34))); });
        add(QStringLiteral("G"),  [](double D,double,double,int){ return -(2.5 * std::pow(D, 0.34)); });
        // —— 基准零偏差 ——
        add(QStringLiteral("H"),  [](double,double,double,int){ return 0.0; });   // 走特判分支（孔 EI=0 / 轴 es=0）
        add(QStringLiteral("J"),  [](double,double,double,int){ return 0.0; });   // j 系列近似为零偏差（非对称特例，留特判扩展）
        // —— 过渡/过盈配合（k~zc）：ei 为正 ——
        add(QStringLiteral("K"),  [](double D,double,double,int g){  // IT4~IT7 取 +0.6·∛D，其余 0
            return (g >= 5 && g <= 8) ? (0.6 * std::pow(D, 1.0/3.0)) : 0.0; });
        add(QStringLiteral("M"),  [](double,double i,double,int){ (void)i; return 6.0 * i; });            // +(IT7−IT6)=6i
        add(QStringLiteral("N"),  [](double D,double,double,int){ return 5.0 * std::pow(D, 0.34); });
        add(QStringLiteral("P"),  [](double,double i,double,int){ return 16.0 * i; });                    // +IT7
        add(QStringLiteral("R"),  [](double,double i,double,int){ return 26.0 * i; });                    // +IT7+IT6
        add(QStringLiteral("RB"), [](double,double i,double,int){ return 32.0 * i; });                    // 轴承补充：近似 +IT7+IT8? 简化 +2·IT7
        add(QStringLiteral("RC"), [](double,double i,double,int){ return 48.0 * i; });                    // 轴承补充：近似 +3·IT7
        add(QStringLiteral("S"),  [](double D,double i,double,int){ return (D <= 50.0) ? 41.0 * i : (16.0 * i + 0.4 * D); });
        add(QStringLiteral("SB"), [](double D,double i,double,int){ double s=(D<=50.0)?41.0*i:(16.0*i+0.4*D); return s + 16.0 * i; });
        add(QStringLiteral("SC"), [](double D,double i,double,int){ double s=(D<=50.0)?41.0*i:(16.0*i+0.4*D); return s + 32.0 * i; });
        add(QStringLiteral("T"),  [](double D,double i,double,int){ return 16.0 * i + 0.63 * D; });
        add(QStringLiteral("TA"), [](double D,double i,double,int){ return 16.0 * i + 2.8 * D; });
        add(QStringLiteral("U"),  [](double D,double i,double,int){ return (D <= 3.0) ? (16.0 * i + D) : (16.0 * i + 0.8 * D); });
        add(QStringLiteral("V"),  [](double D,double i,double,int){ return 16.0 * i + 1.25 * D; });
        add(QStringLiteral("X"),  [](double D,double i,double,int){ return 16.0 * i + 1.6 * D; });
        add(QStringLiteral("Y"),  [](double D,double i,double,int){ return 16.0 * i + 2.0 * D; });
        add(QStringLiteral("Z"),  [](double D,double i,double,int){ return 16.0 * i + 2.5 * D; });
        add(QStringLiteral("ZA"), [](double D,double i,double,int){ return 25.0 * i + 3.15 * D; });
        add(QStringLiteral("ZB"), [](double D,double i,double,int){ return 40.0 * i + 4.0 * D; });
        add(QStringLiteral("ZC"), [](double D,double i,double,int){ return 64.0 * i + 5.0 * D; });
        return t;
    }();
    return tbl;
}

double shaftBaseDeviationUm(const QString& key, double D, double i, double IT, int gidx) {
    const QHash<QString, ShaftDevFn>& tbl = shaftDeviationTable();
    auto it = tbl.find(key);
    if (it == tbl.end()) return 0.0;   // 未收录字母 → 回落零偏差（按 h 基准），不崩溃
    return it.value()(D, i, IT, gidx);
}

// 轴基本偏差的定义类型：a~g 与 j 的基本偏差定义为轴的「上偏差 es」（a~g 为负、j 近零）；
// 其余 k~zc 为「下偏差 ei」（为正）。此判定决定 es/ei 的换算方向（ISO 286-1 规定）。
bool shaftBasicIsUpperDev(const QString& key) {
    return key == QStringLiteral("A")  || key == QStringLiteral("B")  || key == QStringLiteral("C")  ||
           key == QStringLiteral("CD") || key == QStringLiteral("D")  || key == QStringLiteral("E")  ||
           key == QStringLiteral("EF") || key == QStringLiteral("F")  || key == QStringLiteral("FG") ||
           key == QStringLiteral("G")  || key == QStringLiteral("J");
}

} // namespace

double ToleranceDB::standardTolerance(double basicSizeMm, ITGrade grade) {
    const double D = sizeStepGeometricMean(basicSizeMm);
    double um = standardToleranceUm(D, grade);
    const int idx = gradeIndex(grade);
    // 取整规则对齐手册数表：IT01~IT1 取 0.1μm，IT2~IT18 取整 μm
    um = (idx <= 2) ? std::round(um * 10.0) / 10.0 : std::round(um);
    return um / 1000.0;   // μm → mm
}

Deviation ToleranceDB::deviation(const QString& letter, double basicSizeMm,
                                 ITGrade grade, FitMember member) {
    Deviation z{0.0, 0.0};
    const double D = sizeStepGeometricMean(basicSizeMm);
    const double i = toleranceUnitI(D);
    // 标准公差取「查表取整值」(mm→μm)：ISO 基本偏差公式中 es/ei 与 IT 同源取整，故须用 rounded IT。
    const double ITmm = ToleranceDB::standardTolerance(basicSizeMm, grade);
    const double ITum = ITmm * 1000.0;   // μm（取整后）
    const int gidx = gradeIndex(grade);

    const QString key = letter.trimmed().toUpper();

    // 对称配合 JS / js：±IT/2
    if (key == QStringLiteral("JS")) {
        const double h = ITum / 2.0;
        z.upper = +h / 1000.0;
        z.lower = -h / 1000.0;
        return z;
    }
    // H 孔：下偏差 EI = 0（上偏差 ES = +IT）
    if (key == QStringLiteral("H") && member == FitMember::Hole) {
        z.upper = +ITum / 1000.0;
        z.lower = 0.0;
        return z;
    }
    // h 轴：上偏差 es = 0（下偏差 ei = −IT）
    if (key == QStringLiteral("H") && member == FitMember::Shaft) {
        z.upper = 0.0;
        z.lower = -ITum / 1000.0;
        return z;
    }

    // 未知字母（未收录）→ 零偏差，不崩溃
    if (!shaftDeviationTable().contains(key)) return z;

    // 其余字母：轴基本偏差 base（μm）。按 ISO 286-1：
    //   a~g/j 的 base 为轴「上偏差 es」（a~g 为负，j 近零）；k~zc 的 base 为轴「下偏差 ei」（为正）。
    const double base = shaftBaseDeviationUm(key, D, i, ITum, gidx);   // μm
    double esShaft, eiShaft;
    if (shaftBasicIsUpperDev(key)) {
        esShaft = base;                 // 轴基本偏差 = 上偏差 es
        eiShaft = esShaft - ITum;       // ei = es − IT
    } else {
        eiShaft = base;                 // 轴基本偏差 = 下偏差 ei
        esShaft = eiShaft + ITum;       // es = ei + IT
    }
    if (member == FitMember::Hole) {
        // 孔 = 轴镜像：ES = −ei(轴)，EI = −es(轴)（ISO 286-1 孔轴基本偏差反号对称）
        z.upper = (-eiShaft) / 1000.0;
        z.lower = (-esShaft) / 1000.0;
    } else {
        z.upper = esShaft / 1000.0;
        z.lower = eiShaft / 1000.0;
    }
    return z;
}

} // namespace SketchCAD
