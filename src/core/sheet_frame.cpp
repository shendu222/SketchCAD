#include "SheetSpec.h"
#include "Element.h"
#include "ElementFactory.h"
#include "ProjectionSymbol.h"
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

// ----------------------------- 生成图框 + 标题栏 -----------------------------
std::vector<Element> buildSheetFrame(const SheetSpec& spec) {
    std::vector<Element> out;
    const double W = spec.width, H = spec.height;

    // 本地构造辅助：直线 / 矩形 / 文本，均直接建 Element，避免依赖 SketchModel 默认样式
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

    // ---- 图纸幅面边距（GB/T 14689 表4，留有装订边）----
    // a = 25（装订边，左侧）；c = 10（A0/A1/A2）、c = 5（A3/A4/A5），其余三边。
    // （旧实现把 A0~A3 取 5、A4/A5 取 10，与标准相反，已于 T1 纠正。）
    const double a = 25.0;
    const double c = (spec.format == SheetFormat::A0 ||
                      spec.format == SheetFormat::A1 ||
                      spec.format == SheetFormat::A2) ? 10.0 : 5.0;

    // 纸张外边界（细实线）
    rect(0, 0, W, H, QStringLiteral("图框"), 1.0);
    // 图框（粗实线）：左侧 a、其余 c
    rect(a, c, W - c, H - c, QStringLiteral("图框"), 2.0);

    // ---- 标题栏：右下角 ----
    // 标题栏外框高度固定 56mm；宽度优先 180mm，但在 A4/A5 等小幅面内自动收缩，
    // 并保证右侧与图框间距 c，左侧与装订边间距 a + 10mm 安全边距。
    const double tbH = 56.0;
    const double tbW = std::min(180.0, (W - c) - a - 10.0);
    const double bx0 = (W - c) - tbW;     // 左
    const double by0 = c;                 // 下
    const double bx1 = W - c;             // 右
    const double by1 = c + tbH;           // 上

    // 列宽：随标题栏宽度自适应；A4 及以上保持充裕，A5 等小幅面自动收窄。
    const double inset = 2.0;             // 文字距单元格左内边距
    const double sw = std::clamp(tbW * 0.35, 40.0, 70.0); // 签名栏宽度
    const double fw = tbW - sw;           // 字段栏宽度
    const double xMid = bx1 - sw;         // 字段栏 | 签名栏 分隔
    const double nameW = std::clamp(sw * 0.42, 15.0, 30.0); // 签名栏内 姓名列
    const double xNameDate = xMid + nameW;                  // 姓名 | 日期 分隔
    const double scaleW = std::clamp(tbW * 0.22, 22.0, 45.0); // 比例列宽度
    const double xNum = bx0 + fw - scaleW;                    // 图号 | 比例 分隔

    // 横向四等分（从下往上）：底部 12mm、中间两格 12mm、顶部高带 20mm
    const double y1 = by0 + 12.0;         // 共X张/批准 带
    const double y2 = by0 + 24.0;         // 材料/审核 带
    const double y3 = by0 + 36.0;         // 图号比例/设计 带（其上为图纸名称/单位）

    // 文本垂直定位：标签贴单元格顶部，字段贴单元格底部，二者不重叠
    const double labelOff = 2.0;          // 标签距单元格顶边
    const double fieldOff = 4.0;          // 字段距单元格底边

    auto labelInCell = [&](double left, double top, const QString& s, double h) {
        label(left + inset, top - labelOff, s, h);
    };
    auto fieldInCell = [&](double left, double bottom, const QString& key, double h) {
        field(left + inset, bottom + fieldOff, key, h);
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
    // 图纸名称（高带）
    labelInCell(bx0, by1, QStringLiteral("图纸名称"), 3.0);
    fieldInCell(bx0, y3, QStringLiteral("drawingName"), 4.0);
    // 图号 / 比例
    labelInCell(bx0, y3, QStringLiteral("图号"), 3.0);
    fieldInCell(bx0, y2, QStringLiteral("drawingNumber"), 3.5);
    labelInCell(xNum, y3, QStringLiteral("比例"), 3.0);
    fieldInCell(xNum, y2, QStringLiteral("scale"), 3.5);
    // 材料
    labelInCell(bx0, y2, QStringLiteral("材料"), 3.0);
    fieldInCell(bx0, y1, QStringLiteral("material"), 3.5);
    // 共X张第X张
    labelInCell(bx0, y1, QStringLiteral("共 张 第 张"), 3.0);
    fieldInCell(bx0, by0, QStringLiteral("sheets"), 3.5);
    // 单位名称（签名栏上方高带）
    labelInCell(xMid, by1, QStringLiteral("单位名称"), 3.0);
    fieldInCell(xMid, y3, QStringLiteral("organization"), 3.5);
    // 设计
    labelInCell(xMid, y3, QStringLiteral("设计"), 3.0);
    fieldInCell(xMid, y2, QStringLiteral("design"), 3.5);
    labelInCell(xNameDate, y3, QStringLiteral("日期"), 3.0);
    fieldInCell(xNameDate, y2, QStringLiteral("designDate"), 3.5);
    // 审核
    labelInCell(xMid, y2, QStringLiteral("审核"), 3.0);
    fieldInCell(xMid, y1, QStringLiteral("review"), 3.5);
    labelInCell(xNameDate, y2, QStringLiteral("日期"), 3.0);
    fieldInCell(xNameDate, y1, QStringLiteral("reviewDate"), 3.5);
    // 批准
    labelInCell(xMid, y1, QStringLiteral("批准"), 3.0);
    fieldInCell(xMid, by0, QStringLiteral("approve"), 3.5);
    labelInCell(xNameDate, y1, QStringLiteral("日期"), 3.0);
    fieldInCell(xNameDate, by0, QStringLiteral("approveDate"), 3.5);

    // ---- 对中符号（GB/T 14689 6.1）：位于【图纸各边中点】，用粗实线自纸边界画起、
    //      向图框内伸入约 5mm，线宽不小于 0.5mm ----
    {
        const double midX = W * 0.5;                 // 上/下边中点 X
        const double midY = H * 0.5;                 // 左/右边中点 Y
        const double in5  = 5.0;                     // 伸入图框内 5mm
        auto mark = [&](double x1, double y1, double x2, double y2) {
            line(x1, y1, x2, y2, QStringLiteral("图框"), 0.5);
        };
        mark(0.0, midY, a + in5, midY);                    // 左边（自 x=0 起，跨装订边）
        mark(W - c - in5, midY, W, midY);                  // 右边
        mark(midX, 0.0, midX, c + in5);                    // 下边
        mark(midX, H - c - in5, midX, H);                  // 上边
    }

    // ---- 投影法识别符号（GB/T 14665-2012 第7章 表5）----
    {
        const double r = 5.0;                        // 符号半径 5mm
        const double sx = std::max(a + r + 4.0, bx0 - r - 5.0);  // 优先置于标题栏左侧
        const double sy = by0 + tbH * 0.5;
        auto sym = buildProjectionSymbol(Vec3(sx, sy, 0), r, spec.projection);
        out.insert(out.end(), sym.begin(), sym.end());
    }

    (void)ink; (void)gray;
    return out;
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
    const double inset = 2.0;
    const double sw = std::clamp(tbW * 0.35, 40.0, 70.0);   // 右侧信息列宽
    const double xMid = bx1 - sw;                            // 左字段列 | 右列 分隔
    const double scaleW = std::clamp(tbW * 0.22, 22.0, 45.0);
    const double xNum = bx0 + (tbW - sw) - scaleW;           // 图号 | 比例 分隔

    // 横向三分隔（自下而上）
    const double y1 = by0 + 12.0;   // 设计/审核 带
    const double y2 = by0 + 24.0;   // 图号比例/绘制日期 带
    const double y3 = by0 + 36.0;   // 图纸名称 带（其上为高带）

    const double labelOff = 2.0;    // 标签距单元格顶边
    const double fieldOff = 4.0;    // 字段距单元格底边
    auto labelInCell = [&](double left, double top, const QString& s, double h) {
        label(left + inset, top - labelOff, s, h);
    };
    auto fieldInCell = [&](double left, double bottom, const QString& key, const QString& val, double h) {
        field(left + inset, bottom + fieldOff, key, val, h);
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
    // 图纸名称（高带，全宽）
    labelInCell(bx0, by1, QStringLiteral("图纸名称"), 3.0);
    fieldInCell(bx0, y3, QStringLiteral("drawingName"), data.drawingName, 4.0);
    // 图号 / 比例
    labelInCell(bx0, y3, QStringLiteral("图号"), 3.0);
    fieldInCell(bx0, y2, QStringLiteral("drawingNumber"), data.drawingNumber, 3.5);
    labelInCell(xNum, y3, QStringLiteral("比例"), 3.0);
    fieldInCell(xNum, y2, QStringLiteral("scale"), data.scale, 3.5);
    // 绘制日期 / 单位名称
    labelInCell(bx0, y2, QStringLiteral("绘制日期"), 3.0);
    fieldInCell(bx0, y1, QStringLiteral("designDate"), data.designDate, 3.5);
    labelInCell(xMid, y2, QStringLiteral("单位名称"), 3.0);
    fieldInCell(xMid, y1, QStringLiteral("organization"), QString(), 3.5);
    // 设计 / 审核（底部带）
    labelInCell(bx0, y1, QStringLiteral("设计"), 3.0);
    fieldInCell(bx0, by0, QStringLiteral("design"), QString(), 3.5);
    labelInCell(xMid, y1, QStringLiteral("审核"), 3.0);
    fieldInCell(xMid, by0, QStringLiteral("review"), QString(), 3.5);
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
