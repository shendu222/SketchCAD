#include "SheetSpec.h"
#include "Element.h"
#include "ElementFactory.h"
#include "ProjectionSymbol.h"
#include "TitleBlockLayout.h"   // fitCellText：标题栏单元格文字自适应（度量→收缩→定位）
#include "Vec.h"
#include <cmath>
#include <algorithm>
#include <QRectF>

namespace SketchCAD {

// ----------------------------- SheetSpec -----------------------------
struct SheetDim { double shortSide; double longSide; };
static const SheetDim kDims[] = {
    {841.0, 1189.0},   // A0
    {594.0,  841.0},   // A1
    {420.0,  594.0},   // A2
    {297.0,  420.0},   // A3
    {210.0,  297.0},   // A4
    {148.0,  210.0},   // A5
};

std::optional<SheetFormat> sheetFormatFromSize(double w, double h) {
    const double aw = std::fabs(w), ah = std::fabs(h);
    for (int i = 0; i < (int)(sizeof(kDims) / sizeof(kDims[0])); ++i) {
        const SheetDim& d = kDims[i];
        if ((std::fabs(aw - d.shortSide) < 1.0 && std::fabs(ah - d.longSide) < 1.0) ||
            (std::fabs(aw - d.longSide)  < 1.0 && std::fabs(ah - d.shortSide) < 1.0))
            return (SheetFormat)i;
    }
    return std::nullopt;
}

SheetSpec SheetSpec::make(SheetFormat f, bool landscape) {
    const SheetDim& d = kDims[(int)f];
    SheetSpec s;
    s.format = f;
    s.landscape = landscape;
    s.width  = landscape ? d.longSide  : d.shortSide;
    s.height = landscape ? d.shortSide : d.longSide;
    return s;
}

QString SheetSpec::formatName() const {
    static const char* names[] = {"A0", "A1", "A2", "A3", "A4", "A5"};
    return QStringLiteral("%1 (%2×%3)")
        .arg(names[(int)format]).arg(width, 0, 'f', 0).arg(height, 0, 'f', 0);
}

double standardTextHeightMm(SheetFormat f, bool chinese) {
    // GB/T 14665-2012 表3：A0/A1 一档，A2/A3/A4（A5 非基本幅面沿用 A4 档）一档。
    const bool big = (f == SheetFormat::A0 || f == SheetFormat::A1);
    if (big)   return chinese ? 7.0 : 5.0;
    return chinese ? 5.0 : 3.5;
}

// 进程内「当前图纸幅面」：默认 A3（A2/A3/A4 同档）。由「新建图纸」写入。
static SheetFormat g_currentSheetFormat = SheetFormat::A3;
SheetFormat currentSheetFormat() { return g_currentSheetFormat; }
void        setCurrentSheetFormat(SheetFormat f) { g_currentSheetFormat = f; }

bool isStandardDrawingScale(double ratio) {
    if (ratio <= 0.0) return false;
    // 优先系列与允许系列的图形:实物 比值（含缩小与放大）
    static const double kStd[] = {
        1.0, 0.5, 0.2, 0.1, 0.05, 0.02, 0.01, 0.005, 0.002, 0.001,  // 缩小
        2.0, 5.0, 10.0, 20.0, 50.0,                                   // 放大
        1.0 / 1.5, 1.0 / 2.5, 1.0 / 3.0, 1.0 / 4.0, 1.0 / 6.0,       // 允许(缩小)
        2.5, 4.0                                                      // 允许(放大)
    };
    for (double s : kStd)
        if (std::fabs(ratio - s) < 1e-6) return true;
    return false;
}

// ----------------------------- 标题栏栏目定义 -----------------------------
const QVector<TitleBlockField>& titleBlockFields() {
    static const QVector<TitleBlockField> kFields = {
        {QStringLiteral("drawingName"),   QStringLiteral("图纸名称")},
        {QStringLiteral("drawingNumber"), QStringLiteral("图号")},
        {QStringLiteral("scale"),         QStringLiteral("比例")},
        {QStringLiteral("material"),      QStringLiteral("材料")},
        {QStringLiteral("organization"),  QStringLiteral("单位名称")},
        {QStringLiteral("design"),        QStringLiteral("设计")},
        {QStringLiteral("designDate"),    QStringLiteral("设计日期")},
        {QStringLiteral("review"),        QStringLiteral("审核")},
        {QStringLiteral("reviewDate"),    QStringLiteral("审核日期")},
        {QStringLiteral("approve"),       QStringLiteral("批准")},
        {QStringLiteral("approveDate"),   QStringLiteral("批准日期")},
        {QStringLiteral("sheets"),        QStringLiteral("共 张 第 张")},
    };
    return kFields;
}

// ----------------------------- 加长幅面 / 幅面派生辅助 -----------------------------
// 加长幅面「大一号」：用于判定图框周边 c/e 的基本幅面（GB/T 14689：加长幅面图框按大一号确定）
static SheetFormat oneLargerFormat(SheetFormat f) {
    int i = (int)f;
    if (i > 0) --i;          // A5→A4 … A1→A0；A0 已是最大，保持 A0
    return (SheetFormat)i;
}
// 自定义幅面：按最长边挑「不超过该边长的最大基本幅面」用于判定周边 c/e
static SheetFormat basicFormatByLongest(double longest) {
    SheetFormat best = SheetFormat::A5;
    for (int i = 0; i < (int)(sizeof(kDims) / sizeof(kDims[0])); ++i) {
        if (kDims[i].longSide <= longest + 1e-6) best = (SheetFormat)i;
    }
    return best;
}
static QString fmtShortName(SheetFormat f) {
    static const char* n[] = {"A0", "A1", "A2", "A3", "A4", "A5"};
    return QString::fromLatin1(n[(int)f]);
}

// 加长幅面预设（GB/T 14689 第二选择 + 第三选择），供【新建图框】下拉展示
const QVector<ExtendedSheetPreset>& extendedSheetPresets() {
    static QVector<ExtendedSheetPreset> k;
    if (k.isEmpty()) {
        // {label, base, factor} 集中定义；长/短边由 kDims 派生（短边×倍数=新长边，原长边=新短边）
        static const QPair<QString, QPair<SheetFormat,int>> kDefs[] = {
            {QStringLiteral("A3×3"), {SheetFormat::A3, 3}},
            {QStringLiteral("A3×4"), {SheetFormat::A3, 4}},
            {QStringLiteral("A4×3"), {SheetFormat::A4, 3}},
            {QStringLiteral("A4×4"), {SheetFormat::A4, 4}},
            {QStringLiteral("A4×5"), {SheetFormat::A4, 5}},
            {QStringLiteral("A0×2"), {SheetFormat::A0, 2}},
            {QStringLiteral("A0×3"), {SheetFormat::A0, 3}},
            {QStringLiteral("A1×3"), {SheetFormat::A1, 3}},
            {QStringLiteral("A1×4"), {SheetFormat::A1, 4}},
            {QStringLiteral("A2×3"), {SheetFormat::A2, 3}},
            {QStringLiteral("A2×4"), {SheetFormat::A2, 4}},
            {QStringLiteral("A2×5"), {SheetFormat::A2, 5}},
            {QStringLiteral("A3×5"), {SheetFormat::A3, 5}},
            {QStringLiteral("A3×6"), {SheetFormat::A3, 6}},
            {QStringLiteral("A3×7"), {SheetFormat::A3, 7}},
            {QStringLiteral("A4×6"), {SheetFormat::A4, 6}},
            {QStringLiteral("A4×7"), {SheetFormat::A4, 7}},
            {QStringLiteral("A4×8"), {SheetFormat::A4, 8}},
            {QStringLiteral("A4×9"), {SheetFormat::A4, 9}},
        };
        for (const auto& d : kDefs) {
            const SheetDim& base = kDims[(int)d.second.first];
            ExtendedSheetPreset p;
            p.label = d.first;
            p.base = d.second.first;
            p.factor = d.second.second;
            p.shortSide = base.longSide;                 // 新短边 = 基础幅面长边
            p.longSide  = base.shortSide * p.factor;    // 新长边 = 基础幅面短边 × 倍数
            k.append(p);
        }
    }
    return k;
}

// ----------------------------- 【新建图框】系统栏预设注册表（需求 10/11/12）-----------------------------
// ⚠️ MinGW 13.1 巨型聚合初始化会静默崩溃（QStringList 等非平凡成员嵌套花括号），
// 一律 add() lambda 逐条 builder（CommandCatalog 范本），绝不写大静态聚合表。

const QVector<AdditionalBarPreset>& additionalBarPresets() {
    static QVector<AdditionalBarPreset> k;
    if (k.isEmpty()) {
        QVector<AdditionalBarPreset> tmp;   // 局部 builder，避免 lambda 捕获静态量（-Wextra 零警告）
        auto add = [&tmp](const QString& key, const QString& label,
                          std::initializer_list<const char*> cols, int rows) {
            AdditionalBarPreset p;
            p.key = key; p.label = label; p.rows = rows;
            for (const char* c : cols) p.columns << QString::fromUtf8(c);
            tmp.append(p);
        };
        add("revision",  "更改记录栏",
            {"更改标记", "处数", "更改单号", "签名", "日期"}, 2);
        add("tracing",   "描图栏",
            {"描图", "描校", "旧底图总号"}, 1);
        add("notation",  "会签栏",
            {"专业", "姓名", "日期"}, 3);
        k = tmp;
    }
    return k;
}

const QVector<CodeBarPreset>& codeBarPresets() {
    static QVector<CodeBarPreset> k;
    if (k.isEmpty()) {
        QVector<CodeBarPreset> tmp;
        auto add = [&tmp](const QString& key, const QString& label,
                          const QString& caption, const QString& fieldKey) {
            CodeBarPreset p;
            p.key = key; p.label = label; p.caption = caption; p.fieldKey = fieldKey;
            tmp.append(p);
        };
        add("Tydh", "Tydh（图样代号）", "图样代号", "drawingNumber");
        add("Tth",  "Tth（图号）",      "图号",     "drawingNumber");
        add("Tdw",  "Tdw（图样代号·单位）", "图样代号", "drawingNumber");
        k = tmp;
    }
    return k;
}

const QVector<ParamBarPreset>& paramBarPresets() {
    static QVector<ParamBarPreset> k;
    if (k.isEmpty()) {
        QVector<ParamBarPreset> tmp;
        auto add = [&tmp](const QString& key, const QString& label,
                          std::initializer_list<const char*> cols, int rows) {
            ParamBarPreset p;
            p.key = key; p.label = label; p.rows = rows;
            for (const char* c : cols) p.columns << QString::fromUtf8(c);
            tmp.append(p);
        };
        add("bevelGear",  "直齿锥齿轮参数表",
            {"模数 m", "齿数 z", "压力角 α", "锥距 R", "精度等级"}, 3);
        add("sprocket",   "链轮参数表",
            {"节距 p", "齿数 z", "滚子直径 d1", "分度圆直径 d"}, 3);
        add("cylGear",    "渐开线圆柱齿轮参数表",
            {"法向模数 mn", "齿数 z", "齿形角 α", "螺旋角 β", "变位系数 x", "精度等级"}, 3);
        add("wormGear",   "蜗杆传动参数表",
            {"模数 m", "蜗杆头数 z1", "蜗轮齿数 z2", "导程角 γ"}, 3);
        add("spring",     "弹簧参数表",
            {"簧丝直径 d", "中径 D", "有效圈数 n", "旋向"}, 3);
        add("material",   "材料处理",
            {"材料", "热处理", "硬度"}, 2);
        k = tmp;
    }
    return k;
}

// 栏预设检索：key 未命中 → 返回 nullptr（调用方放弃绘制，绝不猜）
static const AdditionalBarPreset* findAdditionalBar(const QString& key) {
    for (const auto& p : additionalBarPresets())
        if (p.key == key) return &p;
    return nullptr;
}
static const CodeBarPreset* findCodeBar(const QString& key) {
    for (const auto& p : codeBarPresets())
        if (p.key == key) return &p;
    return nullptr;
}
static const ParamBarPreset* findParamBar(const QString& key) {
    for (const auto& p : paramBarPresets())
        if (p.key == key) return &p;
    return nullptr;
}

// ---- SheetFrameOptions 派生 ----
double SheetFrameOptions::widthMm() const {
    if (sizeMode == SizeMode::Custom) return customW;
    double s, l;
    if (sizeMode == SizeMode::Basic) {
        const SheetDim& d = kDims[(int)basicFormat]; s = d.shortSide; l = d.longSide;
    } else {
        const SheetDim& d = kDims[(int)extBase]; s = d.longSide; l = d.shortSide * extFactor;
    }
    return landscape ? l : s;
}
double SheetFrameOptions::heightMm() const {
    if (sizeMode == SizeMode::Custom) return customH;
    double s, l;
    if (sizeMode == SizeMode::Basic) {
        const SheetDim& d = kDims[(int)basicFormat]; s = d.shortSide; l = d.longSide;
    } else {
        const SheetDim& d = kDims[(int)extBase]; s = d.longSide; l = d.shortSide * extFactor;
    }
    return landscape ? s : l;
}
SheetFormat SheetFrameOptions::frameMarginFormat() const {
    if (sizeMode == SizeMode::Basic) return basicFormat;
    if (sizeMode == SizeMode::Extended) return oneLargerFormat(extBase);
    return basicFormatByLongest(std::max(customW, customH));
}
QString SheetFrameOptions::sizeLabel() const {
    if (sizeMode == SizeMode::Custom)
        return QStringLiteral("自定义 %1×%2").arg(customW, 0, 'f', 0).arg(customH, 0, 'f', 0);
    if (sizeMode == SizeMode::Extended) {
        const SheetDim& d = kDims[(int)extBase];
        const double sh = d.longSide, lo = d.shortSide * extFactor;
        return QStringLiteral("%1×%2 (%3×%4)").arg(fmtShortName(extBase)).arg(extFactor)
                                            .arg(sh, 0, 'f', 0).arg(lo, 0, 'f', 0);
    }
    return SheetSpec::make(basicFormat, landscape).formatName();
}
SheetSpec SheetFrameOptions::toSheetSpec() const {
    SheetSpec s;
    s.format = frameMarginFormat();
    s.landscape = (sizeMode == SizeMode::Custom) ? (customW >= customH) : landscape;
    s.width = widthMm();
    s.height = heightMm();
    s.projection = projection;
    return s;
}

// ----------------------------- 图框生成选项（【新建图框】核心）-----------------------------
// 标准标题栏（GB/T 10609.1）：与旧 buildSheetFrame 的标题栏绘制逐像素一致（并入同一 helper，
// 保证既有 t_titleBlockAux 等断言不变）。leftM/otherM 为图框到纸张各边的留边（装订/不留装订）。
static void appendStandardTitleBlock(std::vector<Element>& out,
                                     double W, double leftM, double otherM) {
    const QColor ink(26, 29, 40);
    const QColor gray(128, 134, 144);

    auto line = [&](double x1, double y1, double x2, double y2,
                    const QString& layer, double w) {
        Element e;
        e.type = ElementType::Line;
        e.pts = {Vec3(x1, y1, 0), Vec3(x2, y2, 0)};
        e.layer = layer; e.lineStyle = LineStyle::Solid;
        e.lineWidth = w; e.lineWidthAuto = false; e.color = ink;
        out.push_back(std::move(e));
    };
    auto rect = [&](double x1, double y1, double x2, double y2,
                    const QString& layer, double w) {
        Element e;
        e.type = ElementType::Rectangle;
        e.pts = {Vec3(x1, y1, 0), Vec3(x2, y2, 0)};
        e.closed = true;
        e.layer = layer; e.lineStyle = LineStyle::Solid;
        e.lineWidth = w; e.lineWidthAuto = false; e.color = ink;
        out.push_back(std::move(e));
    };
    auto label = [&](double x, double y, const QString& s, double h) {
        Element e;
        e.type = ElementType::Text; e.pts = {Vec3(x, y, 0)};
        e.text = s; e.radius = h;
        e.layer = QStringLiteral("标题栏"); e.color = gray;
        e.lineWidth = 1.0; e.lineWidthAuto = true; e.lineStyle = LineStyle::Solid;
        out.push_back(std::move(e));
    };
    auto field = [&](double x, double y, const QString& key, double h) {
        Element e;
        e.type = ElementType::Text; e.pts = {Vec3(x, y, 0)};
        e.text = QString();                 // 初始为空，由用户填写
        e.name = QStringLiteral("tb:field:") + key;
        e.radius = h;
        e.layer = QStringLiteral("标题栏"); e.color = ink;
        e.lineWidth = 1.0; e.lineWidthAuto = true; e.lineStyle = LineStyle::Solid;
        out.push_back(std::move(e));
    };

    // ---- 标题栏：右下角（与旧实现几何完全一致，仅把 a/c 泛化为 leftM/otherM）----
    const double tbH = 56.0;
    const double tbW = std::min(180.0, (W - otherM) - leftM - 10.0);
    const double bx0 = (W - otherM) - tbW;     // 左
    const double by0 = otherM;                // 下
    const double bx1 = W - otherM;            // 右
    const double by1 = otherM + tbH;          // 上

    const double sw = std::clamp(tbW * 0.35, 40.0, 70.0); // 签名栏宽度
    const double fw = tbW - sw;                       // 字段栏宽度
    const double xMid = bx1 - sw;                    // 字段栏 | 签名栏 分隔
    const double nameW = std::clamp(sw * 0.42, 15.0, 30.0); // 签名栏内 姓名列
    const double xNameDate = xMid + nameW;                  // 姓名 | 日期 分隔
    const double scaleW = std::clamp(tbW * 0.22, 22.0, 45.0); // 比例列宽度
    const double xNum = bx0 + fw - scaleW;                    // 图号 | 比例 分隔

    const double y1 = by0 + 12.0;         // 共X张/批准 带
    const double y2 = by0 + 24.0;         // 材料/审核 带
    const double y3 = by0 + 36.0;         // 图号比例/设计 带（其上为图纸名称/单位）

    auto labelInCell = [&](double left, double right, double bottom, double top,
                           const QString& s, double h) {
        const CellTextFit f = fitCellText(s, h, left, right, bottom, top, true);
        label(f.x, f.y, s, f.height);
    };
    auto fieldInCell = [&](double left, double right, double bottom, double top,
                           const QString& key, double h) {
        const CellTextFit f = fitCellText(QString(), h, left, right, bottom, top, false);
        field(f.x, f.y, key, f.height);
    };

    // 标题栏外框（粗实线）
    rect(bx0, by0, bx1, by1, QStringLiteral("标题栏"), 2.0);
    // 横向分隔线（细实线）
    line(bx0, y1, bx1, y1, QStringLiteral("标题栏"), 1.0);
    line(bx0, y2, bx1, y2, QStringLiteral("标题栏"), 1.0);
    line(bx0, y3, bx1, y3, QStringLiteral("标题栏"), 1.0);
    // 字段栏 | 签名栏 竖分隔
    line(xMid, by0, xMid, by1, QStringLiteral("标题栏"), 1.0);
    // 图号 | 比例 竖分隔（仅上方带）
    line(xNum, y2, xNum, y3, QStringLiteral("标题栏"), 1.0);
    // 姓名 | 日期 竖分隔（仅三行签名带）
    line(xNameDate, by0, xNameDate, y3, QStringLiteral("标题栏"), 1.0);

    // ---- 文本：标签（灰） + 可编辑字段（黑）----
    labelInCell(bx0, xMid, y3, by1, QStringLiteral("图纸名称"), 3.0);
    fieldInCell(bx0, xMid, y3, by1, QStringLiteral("drawingName"), 4.0);
    labelInCell(bx0, xNum, y2, y3, QStringLiteral("图号"), 3.0);
    fieldInCell(bx0, xNum, y2, y3, QStringLiteral("drawingNumber"), 3.5);
    labelInCell(xNum, xMid, y2, y3, QStringLiteral("比例"), 3.0);
    fieldInCell(xNum, xMid, y2, y3, QStringLiteral("scale"), 3.5);
    labelInCell(bx0, xMid, y1, y2, QStringLiteral("材料"), 3.0);
    fieldInCell(bx0, xMid, y1, y2, QStringLiteral("material"), 3.5);
    labelInCell(bx0, xMid, by0, y1, QStringLiteral("共 张 第 张"), 3.0);
    fieldInCell(bx0, xMid, by0, y1, QStringLiteral("sheets"), 3.5);
    labelInCell(xMid, bx1, y3, by1, QStringLiteral("单位名称"), 3.0);
    fieldInCell(xMid, bx1, y3, by1, QStringLiteral("organization"), 3.5);
    labelInCell(xMid, xNameDate, y2, y3, QStringLiteral("设计"), 3.0);
    fieldInCell(xMid, xNameDate, y2, y3, QStringLiteral("design"), 3.5);
    labelInCell(xNameDate, bx1, y2, y3, QStringLiteral("日期"), 3.0);
    fieldInCell(xNameDate, bx1, y2, y3, QStringLiteral("designDate"), 3.5);
    labelInCell(xMid, xNameDate, y1, y2, QStringLiteral("审核"), 3.0);
    fieldInCell(xMid, xNameDate, y1, y2, QStringLiteral("review"), 3.5);
    labelInCell(xNameDate, bx1, y1, y2, QStringLiteral("日期"), 3.0);
    fieldInCell(xNameDate, bx1, y1, y2, QStringLiteral("reviewDate"), 3.5);
    labelInCell(xMid, xNameDate, by0, y1, QStringLiteral("批准"), 3.0);
    fieldInCell(xMid, xNameDate, by0, y1, QStringLiteral("approve"), 3.5);
    labelInCell(xNameDate, bx1, by0, y1, QStringLiteral("日期"), 3.0);
    fieldInCell(xNameDate, bx1, by0, y1, QStringLiteral("approveDate"), 3.5);
}

// 对中符号（GB/T 14689 6.1）：图纸四边中点各一条自纸边界伸入图框的粗短线（线宽 0.5）。
static void appendCenterMarks(std::vector<Element>& out,
                              double W, double H, double leftM, double otherM) {
    const QColor ink(26, 29, 40);
    const double w = 0.5;
    auto seg = [&](double x1, double y1, double x2, double y2) {
        Element e;
        e.type = ElementType::Line;
        e.pts = {Vec3(x1, y1, 0), Vec3(x2, y2, 0)};
        e.layer = QStringLiteral("图框"); e.lineStyle = LineStyle::Solid;
        e.lineWidth = w; e.lineWidthAuto = false; e.color = ink;
        out.push_back(std::move(e));
    };
    const double cx = W / 2.0, cy = H / 2.0;
    seg(0.0, cy, leftM, cy);            // 左边中点 → 图框
    seg(W - otherM, cy, W, cy);         // 右边
    seg(cx, 0.0, cx, otherM);           // 下边
    seg(cx, H - otherM, cx, H);         // 上边
}

// 分区（GB/T 14689）：长度(X)/宽度(Y)方向偶数分区，网格线仅画在顶/左周边带内，
// 区内编号标注在周边带（数字沿顶、字母沿左），不穿越绘图区。
static void appendZones(std::vector<Element>& out,
                        double W, double H, double leftM, double otherM,
                        int nX, int nY, bool showLeft) {
    const QColor ink(26, 29, 40);
    const double innerL = leftM, innerR = W - otherM, innerB = otherM, innerT = H - otherM;
    const double bandTop = otherM;                       // 顶周边带高度
    const double bandLeft = leftM;                      // 左周边带宽度
    auto seg = [&](double x1, double y1, double x2, double y2) {
        Element e;
        e.type = ElementType::Line;
        e.pts = {Vec3(x1, y1, 0), Vec3(x2, y2, 0)};
        e.layer = QStringLiteral("图框"); e.lineStyle = LineStyle::Solid;
        e.lineWidth = 0.5; e.lineWidthAuto = false; e.color = ink;
        out.push_back(std::move(e));
    };
    auto txt = [&](double x, double y, const QString& s, double h) {
        Element e;
        e.type = ElementType::Text; e.pts = {Vec3(x, y, 0)};
        e.text = s; e.radius = h;
        e.layer = QStringLiteral("图框"); e.color = ink;
        e.lineWidth = 1.0; e.lineWidthAuto = true; e.lineStyle = LineStyle::Solid;
        out.push_back(std::move(e));
    };
    const double topLabelY = H - bandTop / 2.0;
    for (int i = 1; i < nX; ++i) {                      // 顶带竖分线
        const double x = innerL + (innerR - innerL) * i / nX;
        seg(x, H - bandTop, x, H);
    }
    const double labelH = std::clamp(bandTop * 0.5, 1.5, 3.0);
    for (int i = 0; i < nX; ++i) {                      // 顶带编号 1..nX
        const double xc = innerL + (innerR - innerL) * (i + 0.5) / nX;
        txt(xc, topLabelY, QString::number(i + 1), labelH);
    }
    if (showLeft) {
        const double leftLabelX = bandLeft / 2.0;
        for (int j = 1; j < nY; ++j) {                  // 左带横分线
            const double y = innerB + (innerT - innerB) * j / nY;
            seg(0.0, y, bandLeft, y);
        }
        for (int j = 0; j < nY; ++j) {                  // 左带字母 A..nY
            const double yc = innerB + (innerT - innerB) * (j + 0.5) / nY;
            txt(leftLabelX, yc, QString(QChar('A' + j)), labelH);
        }
    }
}

// 明细栏（GB/T 10609.2）：置于标题栏正上方，宽度与标题栏对齐；含表头（序号/代号/名称/
// 数量/材料/重量/备注）与若干空数据行，供后续填写。越出图框顶部时自动压缩行数。
static double appendBom(std::vector<Element>& out,
                        double bx0, double bx1, double titleTop,
                        int rows, double frameTop) {
    const QColor ink(26, 29, 40);
    const double rowH = 7.0;
    double bomTop = titleTop + (rows + 1) * rowH;
    if (bomTop > frameTop) {                            // 越顶压缩
        rows = std::max(1, (int)((frameTop - titleTop) / rowH) - 1);
        bomTop = titleTop + (rows + 1) * rowH;
    }
    const double bw = bx1 - bx0;
    // 列宽比例（GB/T 10609.2 推荐）：序号/代号/名称/数量/材料/重量/备注
    static const double kFrac[] = {0.12, 0.18, 0.28, 0.10, 0.15, 0.10, 0.07};
    QVector<double> cx; cx.reserve(8); cx.append(bx0);
    for (int i = 0; i < 7; ++i) cx.append(cx.last() + bw * kFrac[i]);

    auto seg = [&](double x1, double y1, double x2, double y2, double w) {
        Element e;
        e.type = ElementType::Line;
        e.pts = {Vec3(x1, y1, 0), Vec3(x2, y2, 0)};
        e.layer = QStringLiteral("标题栏"); e.lineStyle = LineStyle::Solid;
        e.lineWidth = w; e.lineWidthAuto = false; e.color = ink;
        out.push_back(std::move(e));
    };
    auto txt = [&](double x, double y, const QString& s, double h) {
        Element e;
        e.type = ElementType::Text; e.pts = {Vec3(x, y, 0)};
        e.text = s; e.radius = h;
        e.layer = QStringLiteral("标题栏"); e.color = ink;
        e.lineWidth = 1.0; e.lineWidthAuto = true; e.lineStyle = LineStyle::Solid;
        out.push_back(std::move(e));
    };
    // 外框
    seg(bx0, titleTop, bx1, titleTop, 2.0);
    seg(bx0, bomTop, bx1, bomTop, 2.0);
    seg(bx0, titleTop, bx0, bomTop, 2.0);
    seg(bx1, titleTop, bx1, bomTop, 2.0);
    // 行分隔
    for (int r = 1; r <= rows; ++r) seg(bx0, bomTop - r * rowH, bx1, bomTop - r * rowH, 1.0);
    // 列分隔（全高）
    for (int i = 1; i < 7; ++i) seg(cx[i], titleTop, cx[i], bomTop, 1.0);
    // 表头标签
    static const char* kHead[] = {"序号", "代号", "名称", "数量", "材料", "重量", "备注"};
    const double hH = std::clamp(rowH * 0.5, 1.5, 3.0);
    const double hy = bomTop - rowH / 2.0;
    for (int i = 0; i < 7; ++i)
        txt((cx[i] + cx[i + 1]) / 2.0, hy, QString::fromUtf8(kHead[i]), hH);
    return bomTop;
}

// ---- 通用栏网格（附加栏/参数栏共用）：在 [bottom, bottom+height] × [bx0,bx1] 画
// 表头 + 空数据行；越出 frameTop 时压缩行数（min 1），仍放不下则放弃（返回 bottom 不变）。
static double appendGridBar(std::vector<Element>& out,
                            double bx0, double bx1, double bottom, double frameTop,
                            const QStringList& columns, int rows) {
    const QColor ink(26, 29, 40);
    const double rowH = 7.0;
    const int nCol = std::max(1, (int)columns.size());
    int useRows = std::max(1, rows);
    while (useRows > 1 && bottom + (useRows + 1) * rowH > frameTop) --useRows;
    const double top = bottom + (useRows + 1) * rowH;
    if (bottom + (useRows + 1) * rowH > frameTop + 1e-6) return bottom;   // 一行也放不下：放弃

    auto seg = [&](double x1, double y1, double x2, double y2, double w) {
        Element e;
        e.type = ElementType::Line;
        e.pts = {Vec3(x1, y1, 0), Vec3(x2, y2, 0)};
        e.layer = QStringLiteral("标题栏"); e.lineStyle = LineStyle::Solid;
        e.lineWidth = w; e.lineWidthAuto = false; e.color = ink;
        out.push_back(std::move(e));
    };
    auto txt = [&](double x, double y, const QString& s, double h) {
        Element e;
        e.type = ElementType::Text; e.pts = {Vec3(x, y, 0)};
        e.text = s; e.radius = h;
        e.layer = QStringLiteral("标题栏"); e.color = ink;
        e.lineWidth = 1.0; e.lineWidthAuto = true; e.lineStyle = LineStyle::Solid;
        out.push_back(std::move(e));
    };

    // 外框（粗）+ 表头分隔（细）
    seg(bx0, bottom, bx1, bottom, 2.0);
    seg(bx0, top, bx1, top, 2.0);
    seg(bx0, bottom, bx0, top, 2.0);
    seg(bx1, bottom, bx1, top, 2.0);
    seg(bx0, top - rowH, bx1, top - rowH, 1.0);
    // 列分隔（全高，均匀分布）
    const double bw = (bx1 - bx0) / nCol;
    for (int i = 1; i < nCol; ++i)
        seg(bx0 + bw * i, bottom, bx0 + bw * i, top, 1.0);
    // 表头标签（列内居中）
    const double hH = std::clamp(rowH * 0.5, 1.5, 3.0);
    const double hy = top - rowH / 2.0;
    for (int i = 0; i < nCol; ++i)
        txt(bx0 + bw * (i + 0.5), hy, columns[i], hH);
    return top;
}

// 代号栏（需求 11）：图框左上角——上周边带内、自左图框边起的小矩形，
// 标签 + 字段；字段元素名 = "tb:field:<fieldKey>"，与标题栏同名字段共用名，
// 由 TitleBlockForm「同名全写回」实现自动关联（填图号 → 代号栏同步）。
static void appendCodeBar(std::vector<Element>& out,
                          double W, double H, double leftM, double otherM,
                          const CodeBarPreset& preset) {
    const QColor ink(26, 29, 40);
    const QColor gray(128, 134, 144);
    const double bandH = otherM;                    // 上周边带高度
    const double boxW = std::min(140.0, (W - otherM) - leftM);
    const double x0 = leftM, y0 = H - bandH, x1 = leftM + boxW, y1 = H;
    const double capW = std::min(40.0, boxW * 0.4); // 标签列宽

    auto seg = [&](double x1_, double y1_, double x2_, double y2_, double w) {
        Element e;
        e.type = ElementType::Line;
        e.pts = {Vec3(x1_, y1_, 0), Vec3(x2_, y2_, 0)};
        e.layer = QStringLiteral("图框"); e.lineStyle = LineStyle::Solid;
        e.lineWidth = w; e.lineWidthAuto = false; e.color = ink;
        out.push_back(std::move(e));
    };
    auto label = [&](double x, double y, const QString& s, double h) {
        Element e;
        e.type = ElementType::Text; e.pts = {Vec3(x, y, 0)};
        e.text = s; e.radius = h;
        e.layer = QStringLiteral("图框"); e.color = gray;
        e.lineWidth = 1.0; e.lineWidthAuto = true; e.lineStyle = LineStyle::Solid;
        out.push_back(std::move(e));
    };
    auto fld = [&](double x, double y, const QString& key, double h) {
        Element e;
        e.type = ElementType::Text; e.pts = {Vec3(x, y, 0)};
        e.text = QString();                 // 初始为空，随标题栏关联字段填写
        e.name = QStringLiteral("tb:field:") + key;
        e.radius = h;
        e.layer = QStringLiteral("图框"); e.color = ink;
        e.lineWidth = 1.0; e.lineWidthAuto = true; e.lineStyle = LineStyle::Solid;
        out.push_back(std::move(e));
    };

    // 外框（粗）+ 标签列分隔（细）
    seg(x0, y0, x1, y0, 2.0); seg(x0, y1, x1, y1, 2.0);
    seg(x0, y0, x0, y1, 2.0); seg(x1, y0, x1, y1, 2.0);
    seg(x0 + capW, y0, x0 + capW, y1, 1.0);

    // 文本：fitCellText 同一入口（标签贴顶 / 字段贴底 / 超宽降字高）
    const double hTxt = std::clamp(bandH * 0.45, 2.0, 3.5);
    {
        const CellTextFit f = fitCellText(preset.caption, hTxt, x0, x0 + capW, y0, y1, true);
        label(f.x, f.y, preset.caption, f.height);
    }
    {
        const CellTextFit f = fitCellText(QString(), hTxt, x0 + capW, x1, y0, y1, false);
        fld(f.x, f.y, preset.fieldKey, f.height);
    }
}

// 依据 GB/T 14689（图框）/ GB/T 10609.1（标题栏）/ GB/T 10609.2（明细栏）生成图框 + 选项叠加，
// 返回一组图元。默认构造的 SheetFrameOptions 与旧 buildSheetFrame(spec) 行为完全一致。
std::vector<Element> buildSheetFrameWithOptions(const SheetFrameOptions& opts) {
    std::vector<Element> out;
    const double W = opts.widthMm();
    const double H = opts.heightMm();
    const SheetFormat frameFmt = opts.frameMarginFormat();
    const bool tenClass = (frameFmt == SheetFormat::A0 ||
                           frameFmt == SheetFormat::A1 ||
                           frameFmt == SheetFormat::A2);
    const double c = tenClass ? 10.0 : 5.0;            // 留装订边：非装订三边
    const double e = tenClass ? 20.0 : 10.0;           // 不留装订边：四边
    const double bindW = std::clamp(opts.bindingWidth, 5.0, 100.0);
    const double leftM  = opts.bindingMargin ? bindW : e;
    const double otherM = opts.bindingMargin ? c : e;

    const QColor ink(26, 29, 40);

    auto rect = [&](double x1, double y1, double x2, double y2,
                    const QString& layer, double w) {
        Element el;
        el.type = ElementType::Rectangle;
        el.pts = {Vec3(x1, y1, 0), Vec3(x2, y2, 0)};
        el.closed = true;
        el.layer = layer; el.lineStyle = LineStyle::Solid;
        el.lineWidth = w; el.lineWidthAuto = false; el.color = ink;
        out.push_back(std::move(el));
    };

    // 纸张外边界（细实线）
    rect(0, 0, W, H, QStringLiteral("图框"), 1.0);
    // 图框（粗实线）
    rect(leftM, otherM, W - otherM, H - otherM, QStringLiteral("图框"), 2.0);

    // 对中符号（需求 8，可选）
    if (opts.centerMarks) appendCenterMarks(out, W, H, leftM, otherM);

    // 标题栏几何（始终计算，供明细栏/投影符号定位）
    const double tbH = 56.0;
    const double tbW = std::min(180.0, (W - otherM) - leftM - 10.0);
    const double bx0 = (W - otherM) - tbW;
    const double by0 = otherM;
    const double bx1 = W - otherM;
    const double by1 = otherM + tbH;

    // 标题栏上方自下而上堆叠：明细栏 → 附加栏 → 参数栏（需求 9/10/12）；
    // 代号栏独立置于图框左上角（需求 11）。各栏越顶自动压缩/放弃，绝不越出图框。
    double stackTop = by1;                              // 堆叠基准：标题栏顶
    if (opts.bomEnabled)
        stackTop = appendBom(out, bx0, bx1, by1, opts.bomRows, H - otherM);

    // 标题栏（需求 9，Standard 时绘制）
    if (opts.titleBlock == TitleBlockStyle::Standard)
        appendStandardTitleBlock(out, W, leftM, otherM);

    if (opts.additionalBarEnabled) {
        if (const AdditionalBarPreset* p = findAdditionalBar(opts.additionalBarKey))
            stackTop = appendGridBar(out, bx0, bx1, stackTop, H - otherM,
                                     p->columns, p->rows);
    }
    if (opts.paramBarEnabled) {
        if (const ParamBarPreset* p = findParamBar(opts.paramBarKey))
            stackTop = appendGridBar(out, bx0, bx1, stackTop, H - otherM,
                                     p->columns, p->rows);
    }

    // 代号栏（需求 11，可选）：图框左上角上周边带内
    if (opts.codeBarEnabled) {
        if (const CodeBarPreset* p = findCodeBar(opts.codeBarKey))
            appendCodeBar(out, W, H, leftM, otherM, *p);
    }

    // 分区（需求 6，可选）：仅画在顶/左周边带；左带受 zoneShowLeft 控制
    if (opts.zonesEnabled && opts.zoneCountX > 0 && opts.zoneCountY > 0)
        appendZones(out, W, H, leftM, otherM, opts.zoneCountX, opts.zoneCountY,
                    opts.zoneShowLeft);

    // 投影法识别符号（GB/T 14665-2012 第7章 表5）
    {
        const double r = 5.0;
        const double sx = std::max(leftM + r + 4.0, bx0 - r - 5.0);
        const double sy = by0 + tbH * 0.5;
        auto sym = buildProjectionSymbol(Vec3(sx, sy, 0), r, opts.projection);
        out.insert(out.end(), sym.begin(), sym.end());
    }

    return out;
}

// 向后兼容：旧 buildSheetFrame(spec) 等价于「默认选项 + 留装订边 + 标准标题栏」的生成结果。
std::vector<Element> buildSheetFrame(const SheetSpec& spec) {
    SheetFrameOptions o;
    o.sizeMode = SheetFrameOptions::SizeMode::Basic;
    o.basicFormat = spec.format;
    o.landscape = spec.landscape;
    o.bindingMargin = true;
    o.centerMarks = false;
    o.zonesEnabled = false;
    o.titleBlock = TitleBlockStyle::Standard;
    o.bomEnabled = false;
    o.projection = spec.projection;
    return buildSheetFrameWithOptions(o);
}

// ----------------------------- 插入标准标题栏（追加到当前图纸）-----------------------------
// 共享标题栏网格绘制的前向声明（定义见下方 appendTitleBlockGrid）
static void appendTitleBlockGrid(std::vector<Element>& out,
                                 double bx0, double by0, double bx1, double by1,
                                 const InsertedTitleBlock& data);

std::vector<Element> buildInsertedTitleBlock(const QRectF& contentBBox, const InsertedTitleBlock& data) {
    std::vector<Element> out;
    const QColor ink(26, 29, 40);

    // 本地构造辅助（仅边框用到，标题栏网格走 appendTitleBlockGrid 共享实现）
    auto rect = [&](double x1, double y1, double x2, double y2,
                    const QString& layer, double w) {
        Element e;
        e.type = ElementType::Rectangle;
        e.pts = {Vec3(x1, y1, 0), Vec3(x2, y2, 0)};
        e.closed = true;
        e.layer = layer; e.lineStyle = LineStyle::Solid;
        e.lineWidth = w; e.lineWidthAuto = false; e.color = ink;
        out.push_back(std::move(e));
    };

    // ---- 由内容包围盒推算图框（留边距；底部预留标题栏高度）----
    const double margin = 15.0;
    const double tbH = 56.0;
    double fx0, fy0, fx1, fy1;
    const bool had = contentBBox.isValid() && !contentBBox.isEmpty();
    if (had) {
        fx0 = contentBBox.left()   - margin;
        fy0 = contentBBox.top()    - margin - tbH;   // 标准 QRectF：top()=最小 Y，图框下边界给标题栏留带
        fx1 = contentBBox.right()  + margin;
        fy1 = contentBBox.bottom() + margin;         // bottom()=最大 Y，图框上边界
    } else {
        // 无内容：给一个默认区域（正坐标，便于直接观看）
        fx0 = 0.0;  fy0 = 0.0;
        fx1 = 280.0; fy1 = 140.0 + tbH;
    }
    // 最小幅面保护：避免出现过窄/过矮导致标题栏比例失调
    const double minW = 200.0, minH = 140.0 + tbH;
    if (fx1 - fx0 < minW) { const double c = (fx0 + fx1) / 2.0; fx0 = c - minW / 2.0; fx1 = c + minW / 2.0; }
    if (fy1 - fy0 < minH) { const double c = (fy0 + fy1) / 2.0; fy0 = c - minH / 2.0; fy1 = c + minH / 2.0; }

    // 图纸边框（粗实线）
    rect(fx0, fy0, fx1, fy1, QStringLiteral("图框"), 2.0);

    // ---- 标题栏：右下角（复用统一网格绘制，与 buildAutoFrameTitleBlock 一致）----
    const double tbW = std::min(180.0, (fx1 - fx0) * 0.6);
    const double bx1 = fx1;            // 右
    const double bx0 = bx1 - tbW;     // 左
    const double by0 = fy0;           // 下
    const double by1 = fy0 + tbH;     // 上
    appendTitleBlockGrid(out, bx0, by0, bx1, by1, data);

    return out;
}

// ----------------------------- 标题栏网格（共享绘制）-----------------------------
// 在给定矩形 [bx0,by0]~[bx1,by1] 内绘制标准标题栏（外框 + 分隔线 + 标签/字段文本）。
// 该矩形即"标题栏占用区"，由调用方（buildInsertedTitleBlock / buildAutoFrameTitleBlock）
// 负责定位与避让；本函数只负责把网格画进该矩形，不关心其是否覆盖内容。
static void appendTitleBlockGrid(std::vector<Element>& out,
                                 double bx0, double by0, double bx1, double by1,
                                 const InsertedTitleBlock& data) {
    const QColor ink(26, 29, 40);
    const QColor gray(128, 134, 144);

    auto line = [&](double x1, double y1, double x2, double y2,
                    const QString& layer, double w) {
        Element e;
        e.type = ElementType::Line;
        e.pts = {Vec3(x1, y1, 0), Vec3(x2, y2, 0)};
        e.layer = layer; e.lineStyle = LineStyle::Solid;
        e.lineWidth = w; e.lineWidthAuto = false; e.color = ink;
        out.push_back(std::move(e));
    };
    auto rect = [&](double x1, double y1, double x2, double y2,
                    const QString& layer, double w) {
        Element e;
        e.type = ElementType::Rectangle;
        e.pts = {Vec3(x1, y1, 0), Vec3(x2, y2, 0)};
        e.closed = true;
        e.layer = layer; e.lineStyle = LineStyle::Solid;
        e.lineWidth = w; e.lineWidthAuto = false; e.color = ink;
        out.push_back(std::move(e));
    };
    auto label = [&](double x, double y, const QString& s, double h) {
        Element e;
        e.type = ElementType::Text; e.pts = {Vec3(x, y, 0)};
        e.text = s; e.radius = h;
        e.layer = QStringLiteral("标题栏"); e.color = gray;
        e.lineWidth = 1.0; e.lineWidthAuto = true; e.lineStyle = LineStyle::Solid;
        out.push_back(std::move(e));
    };
    auto field = [&](double x, double y, const QString& key, const QString& value, double h) {
        Element e;
        e.type = ElementType::Text; e.pts = {Vec3(x, y, 0)};
        e.text = value; e.name = QStringLiteral("tb:field:") + key; e.radius = h;
        e.layer = QStringLiteral("标题栏"); e.color = ink;
        e.lineWidth = 1.0; e.lineWidthAuto = true; e.lineStyle = LineStyle::Solid;
        out.push_back(std::move(e));
    };

    const double tbW = bx1 - bx0;
    const double sw = std::clamp(tbW * 0.35, 40.0, 70.0);   // 右侧信息列宽
    const double xMid = bx1 - sw;                            // 左字段列 | 右列 分隔
    const double scaleW = std::clamp(tbW * 0.22, 22.0, 45.0);
    const double xNum = bx0 + (tbW - sw) - scaleW;           // 图号 | 比例 分隔

    // 横向三分隔（自下而上）
    const double y1 = by0 + 12.0;   // 设计/审核 带
    const double y2 = by0 + 24.0;   // 图号比例/绘制日期 带
    const double y3 = by0 + 36.0;   // 图纸名称 带（其上为高带）

    // 文本放置：与 buildSheetFrame 同一入口 fitCellText（标签贴顶 / 字段贴底 /
    // 超宽按比例降字高）。此处字段带 data 预填值，长图名在生成期即收缩适配。
    auto labelInCell = [&](double left, double right, double bottom, double top,
                           const QString& s, double h) {
        const CellTextFit f = fitCellText(s, h, left, right, bottom, top, true);
        label(f.x, f.y, s, f.height);
    };
    auto fieldInCell = [&](double left, double right, double bottom, double top,
                           const QString& key, const QString& val, double h) {
        const CellTextFit f = fitCellText(val, h, left, right, bottom, top, false);
        field(f.x, f.y, key, val, f.height);
    };

    // 标题栏外框（粗实线）
    rect(bx0, by0, bx1, by1, QStringLiteral("标题栏"), 2.0);
    // 横向分隔（细实线）
    line(bx0, y1, bx1, y1, QStringLiteral("标题栏"), 1.0);
    line(bx0, y2, bx1, y2, QStringLiteral("标题栏"), 1.0);
    line(bx0, y3, bx1, y3, QStringLiteral("标题栏"), 1.0);
    // 左字段列 | 右列 竖分隔
    line(xMid, by0, xMid, by1, QStringLiteral("标题栏"), 1.0);
    // 图号 | 比例 竖分隔（仅上两带）
    line(xNum, y2, xNum, y3, QStringLiteral("标题栏"), 1.0);

    // ---- 文本：标签（灰）+ 字段值（黑，按 data 填充）----
    // 图纸名称（高带）
    labelInCell(bx0, xMid, y3, by1, QStringLiteral("图纸名称"), 3.0);
    fieldInCell(bx0, xMid, y3, by1, QStringLiteral("drawingName"), data.drawingName, 4.0);
    // 图号 / 比例
    labelInCell(bx0, xNum, y2, y3, QStringLiteral("图号"), 3.0);
    fieldInCell(bx0, xNum, y2, y3, QStringLiteral("drawingNumber"), data.drawingNumber, 3.5);
    labelInCell(xNum, xMid, y2, y3, QStringLiteral("比例"), 3.0);
    fieldInCell(xNum, xMid, y2, y3, QStringLiteral("scale"), data.scale, 3.5);
    // 绘制日期 / 单位名称
    labelInCell(bx0, xMid, y1, y2, QStringLiteral("绘制日期"), 3.0);
    fieldInCell(bx0, xMid, y1, y2, QStringLiteral("designDate"), data.designDate, 3.5);
    labelInCell(xMid, bx1, y1, y2, QStringLiteral("单位名称"), 3.0);
    fieldInCell(xMid, bx1, y1, y2, QStringLiteral("organization"), QString(), 3.5);
    // 设计 / 审核（底部带）
    labelInCell(bx0, xMid, by0, y1, QStringLiteral("设计"), 3.0);
    fieldInCell(bx0, xMid, by0, y1, QStringLiteral("design"), QString(), 3.5);
    labelInCell(xMid, bx1, by0, y1, QStringLiteral("审核"), 3.0);
    fieldInCell(xMid, bx1, by0, y1, QStringLiteral("review"), QString(), 3.5);
}

// ----------------------------- 按幅面自动套框 + 标题栏（追加到当前图纸）-----------------------------
std::vector<Element> buildAutoFrameTitleBlock(const QRectF& contentBBox,
                                              const SheetSpec& spec,
                                              const InsertedTitleBlock& data) {
    std::vector<Element> out;
    const QColor ink(26, 29, 40);

    // 本地构造辅助（仅边框用到，标题栏网格走 appendTitleBlockGrid 共享实现）
    auto rect = [&](double x1, double y1, double x2, double y2,
                    const QString& layer, double w) {
        Element e;
        e.type = ElementType::Rectangle;
        e.pts = {Vec3(x1, y1, 0), Vec3(x2, y2, 0)};
        e.closed = true;
        e.layer = layer; e.lineStyle = LineStyle::Solid;
        e.lineWidth = w; e.lineWidthAuto = false; e.color = ink;
        out.push_back(std::move(e));
    };

    // ---- 1) 内容最小外接矩形（左下角原点，X 右 Y 上，单位 mm）----
    const double margin = 15.0;   // 内容到图框的安全边距
    const double tbH = 56.0;      // 标题栏高度（固定）
    const double g   = 8.0;       // 标题栏与内容的安全间距（避让判定）
    const bool had = contentBBox.isValid() && !contentBBox.isEmpty();
    double cx0 = had ? contentBBox.left()   : 0.0;
    double cy0 = had ? contentBBox.top()    : 0.0;   // 标准 QRectF：top()=最小 Y（图框下边界）
    double cx1 = had ? contentBBox.right()  : 200.0;
    double cy1 = had ? contentBBox.bottom() : 140.0;  // bottom()=最大 Y（图框上边界）

    // ---- 2) 基础图框：把内容按边距包含进去 ----
    double fx0 = cx0 - margin, fy0 = cy0 - margin, fx1 = cx1 + margin, fy1 = cy1 + margin;

    // ---- 3) 所选幅面作为最小尺寸：不足则在中心放大到该幅面 ----
    const double W0 = spec.width, H0 = spec.height;
    if (fx1 - fx0 < W0) { const double c = (fx0 + fx1) / 2.0; fx0 = c - W0 / 2.0; fx1 = c + W0 / 2.0; }
    if (fy1 - fy0 < H0) { const double c = (fy0 + fy1) / 2.0; fy0 = c - H0 / 2.0; fy1 = c + H0 / 2.0; }

    // ---- 4) 标题栏宽度（受幅面/图框约束），固定在图框右下角 ----
    double tbW = std::min(180.0, (fx1 - fx0) * 0.6);
    tbW = std::max(tbW, 120.0);                         // 保证栏内文字可读
    tbW = std::min(tbW, (fx1 - fx0) - 1.0);             // 不超过图框宽
    double bx1 = fx1, bx0 = bx1 - tbW;                 // 右
    double by0 = fy0, by1 = by0 + tbH;                 // 下

    // ---- 5) 自动避让：若右下角标题栏区与内容相交，则向"更省"的一侧外扩图框 ----
    // 只要标题栏整体在内容"右侧"(bx0>=cx1+g) 或 "下方"(by1<=cy0-g) 任一成立即不相交；
    // 若两者都不成立，则比较"右扩"与"下扩"的代价，选代价更小者，使标题栏整体移出内容。
    if (had) {
        const bool rightClear  = (bx0 >= cx1 + g);     // 标题栏整体位于内容右侧
        const bool bottomClear = (by1 <= cy0 - g);     // 标题栏整体位于内容下方
        if (!rightClear && !bottomClear) {
            const double costRight  = (cx1 + g + tbW) - fx1;   // 右扩所需增量
            const double costBottom = fy0 - (cy0 - g - tbH);   // 下扩所需增量
            if (costRight <= costBottom) {                     // 向右外扩：标题栏置于内容右侧
                fx1 = cx1 + g + tbW; bx1 = fx1; bx0 = bx1 - tbW;
            } else {                                           // 向下外扩：标题栏置于内容下方
                fy0 = cy0 - g - tbH; by0 = fy0; by1 = by0 + tbH;
            }
        }
    }

    // ---- 6) 绘制：图纸边框（粗实线）+ 标题栏（网格 + 文本）----
    rect(fx0, fy0, fx1, fy1, QStringLiteral("图框"), 2.0);
    appendTitleBlockGrid(out, bx0, by0, bx1, by1, data);

    return out;
}

} // namespace SketchCAD
