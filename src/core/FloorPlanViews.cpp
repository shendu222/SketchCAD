// =====================================================================
// FloorPlanViews —— 参数化户型平面图（建筑装修专业）实现
// =====================================================================
// 绘制算法总览（详见 docs/户型平面图参数化绘制-算法与参数说明.md）：
//   1. 数据预处理：defaultLayout() 把附图 C 户型拓扑固化为「墙段运行表」——
//      每条墙是一个带厚度的轴对齐矩形（模型 mm），并携带洞口表（门/窗/推拉门，
//      以沿墙绝对坐标 + 宽度描述）。布局含附图的高差台阶：右上多功能间块
//      抬高 900（主体顶 10856 / 总高 11756）；
//   2. 墙体合成：WallBuilder 把每条墙按洞口切成「实墙段 + 洞口段」——
//      实墙段画双线轮廓，剪力墙再叠一层 Hatch（sectionExempt=SmallAreaSolid
//      → 渲染层走 SolidFill 涂黑通道）；洞口段按类型画窗线/门扇+开启弧/推拉扇；
//   3. 尺寸链：ChainBuilder 画建筑斜短线(45°)式**三级**连续标注（分格 →
//      分间 → 总尺寸），界线自墙面贯通至最外级链外 2mm，数字水平书写、
//      居中于尺寸线（横链在线上方、竖链在线上），节点取自墙角/洞口棱线，
//      对齐附图的密集分格风格；
//   4. 引线标注：addLeader() 箭头 + 折线 + 多行文字（附图左下降板说明）；
//   5. 家具图块：buildFurniture() 用直线/圆/椭圆/矩形组合床/沙发/桌椅/洁具/
//      衣柜（对角线交叉）/洗衣机等；
//   6. 标注与图签：房间名 Text（入户/厨房/多功能间/餐厅/客厅/卫生×2/主卧/
//      卧室/阳台/生活阳台）+ 右侧通高装饰图签（设计单位标识/全称、工程地址、
//      工程名称、项目名称、审核~图名六行签字栏、图纸名称大字、专业|设计阶段、
//      图号|比例、平面方案、快速搜索、出版图签、备注）；
//   7. 比例与排版：按 A3 可用区（扣除图签与三级链带宽）取标准比例 1:50，
//      平面图在可用区内居中，所有模型坐标经 X()/Y() 变换为图纸坐标。

#include "FloorPlanViews.h"
#include "Section.h"
#include "ElementFactory.h"
#include "Vec.h"
#include <QtMath>
#include <algorithm>
#include <cmath>

namespace SketchCAD {

// ----------------------------- 样式常量 -----------------------------
static const QColor C_INK   (26, 29, 40);    // 墙体/图签墨色
static const QColor C_FURN  (110, 115, 125); // 家具灰
static const QColor C_DIM   (60, 60, 60);    // 标注
static const QColor C_WIN   (26, 29, 40);

static const char* LY_WALL  = "户型-墙体";
static const char* LY_OPEN  = "户型-门窗";
static const char* LY_FURN  = "户型-家具";
static const char* LY_DIM   = "户型-标注";
static const char* LY_TEXT  = "户型-文字";
static const char* LY_TITLE = "户型-图签";

namespace {

// 建筑图常用比例分母（1:n，n 越大图越小）
const double kArchScales[] = {50.0, 100.0, 150.0, 200.0, 250.0, 500.0, 1000.0};

QString scaleText(double denom) {
    return QStringLiteral("1:%1").arg(denom == int(denom) ? QString::number(int(denom))
                                                          : QString::number(denom, 'g', 4));
}

// 数字文本宽粗估（数字/小数点 ≈ 0.62em）
double digitW(const QString& t, double h) { return t.size() * h * 0.62; }

// ----------------------------- 图元便捷构造 -----------------------------
Element ln(const Vec3& a, const Vec3& b, const SketchModel* m, const QString& layer,
           double w = 1.0, LineStyle st = LineStyle::Solid, const QColor& c = C_INK) {
    Element e = ElementFactory::createLine(a, b, m);
    e.layer = layer; e.color = c; e.lineStyle = st;
    e.lineWidthAuto = false; e.lineWidth = w;
    return e;
}
Element txt(const Vec3& p, const QString& s, double h, const SketchModel* m,
            const QString& layer, const QColor& c = C_INK) {
    Element e = ElementFactory::createText(p, s, h, c);
    e.layer = layer;
    Q_UNUSED(m);
    return e;
}
// 涂黑填充区（剪力墙）：Hatch + SmallAreaSolid → 渲染层 SolidFill 通道
Element pocheRect(double x0, double y0, double x1, double y1, const SketchModel* m) {
    Element e;
    e.type = ElementType::Hatch;
    e.name = QStringLiteral("剪力墙涂黑");
    e.layer = LY_WALL;
    e.color = C_INK;
    e.pts = { Vec3(x0, y0, 0), Vec3(x1, y0, 0), Vec3(x1, y1, 0), Vec3(x0, y1, 0) };
    e.sectionExempt = int(SectionExempt::SmallAreaSolid);
    e.lineWidthAuto = false; e.lineWidth = 1.4;      // 外轮廓粗实线
    Q_UNUSED(m);
    return e;
}
void addRect4(std::vector<Element>& out, double x0, double y0, double x1, double y1,
              const SketchModel* m, const QString& layer, double w, const QColor& c = C_INK) {
    out.push_back(ln({x0, y0, 0}, {x1, y0, 0}, m, layer, w, LineStyle::Solid, c));
    out.push_back(ln({x1, y0, 0}, {x1, y1, 0}, m, layer, w, LineStyle::Solid, c));
    out.push_back(ln({x1, y1, 0}, {x0, y1, 0}, m, layer, w, LineStyle::Solid, c));
    out.push_back(ln({x0, y1, 0}, {x0, y0, 0}, m, layer, w, LineStyle::Solid, c));
}

// ----------------------------- 洞口与墙段定义 -----------------------------
// 洞口类型
enum OpenType { OT_Window = 0, OT_Door = 1, OT_Slide = 2 };

struct OpeningSpec {
    double offset = 0;   // 沿墙起点的绝对模型坐标（mm）
    double width  = 0;   // 洞口宽
    int    type   = OT_Window;
    int    hinge  = 0;   // 门：0=铰位在洞口起点侧，1=在终点侧
    int    swing  = 1;   // 门：开启侧（墙局部坐标的垂直方向 +1/-1）
};

struct WallSpec {
    double x0 = 0, y0 = 0, x1 = 0, y1 = 0;  // 墙体矩形（厚度已含）
    bool   solid = false;                    // true=剪力墙（涂黑）
    std::vector<OpeningSpec> openings;       // 沿墙起点 → 终点排序
};

bool wallIsHorizontal(const WallSpec& w) { return (w.y1 - w.y0) <= (w.x1 - w.x0); }

// ----------------------------- 坐标变换 -----------------------------
// 模型(mm) → 纸面(mm)：paper = model * s + o。须先于墙体合成定义（WallBuilder 依赖）。
struct Transform {
    double s = 0.02, ox = 0, oy = 0;
    double X(double x) const { return ox + x * s; }
    double Y(double y) const { return oy + y * s; }
};

// ----------------------------- 墙体合成 -----------------------------
// 把一条墙按洞口切成段并生成图元（输入已是图纸坐标的墙矩形，洞口为模型偏移 → 先换算）。
struct WallBuilder {
    std::vector<Element>* out = nullptr;
    const SketchModel* model = nullptr;
    FloorPlanViewsResult* R = nullptr;

    void jamb(double x0, double y0, double x1, double y1) {   // 洞口端头封口线
        out->push_back(ln({x0, y0, 0}, {x1, y1, 0}, model, LY_WALL, 1.0));
    }

    // 实墙段：双线 + （剪力墙）涂黑
    void solidPiece(double x0, double y0, double x1, double y1, bool isSolid) {
        bool horiz = (y1 - y0) <= (x1 - x0);
        if (isSolid) {
            out->push_back(pocheRect(x0, y0, x1, y1, model));
            R->pocheCount++;
        }
        if (horiz) {
            out->push_back(ln({x0, y0, 0}, {x1, y0, 0}, model, LY_WALL, 1.6));
            out->push_back(ln({x0, y1, 0}, {x1, y1, 0}, model, LY_WALL, 1.6));
        } else {
            out->push_back(ln({x0, y0, 0}, {x0, y1, 0}, model, LY_WALL, 1.6));
            out->push_back(ln({x1, y0, 0}, {x1, y1, 0}, model, LY_WALL, 1.6));
        }
        R->wallRunCount++;
    }

    // 窗：洞口内两条中间分格线 + 端头封口
    void windowPiece(double x0, double y0, double x1, double y1) {
        bool horiz = (y1 - y0) <= (x1 - x0);
        const double t = horiz ? (y1 - y0) : (x1 - x0);
        if (horiz) {
            const double ym = 0.5 * (y0 + y1);
            out->push_back(ln({x0, y0, 0}, {x1, y0, 0}, model, LY_OPEN, 1.0, LineStyle::Solid, C_WIN));
            out->push_back(ln({x0, y1, 0}, {x1, y1, 0}, model, LY_OPEN, 1.0, LineStyle::Solid, C_WIN));
            out->push_back(ln({x0, ym - t / 6, 0}, {x1, ym - t / 6, 0}, model, LY_OPEN, 1.0, LineStyle::Solid, C_WIN));
            out->push_back(ln({x0, ym + t / 6, 0}, {x1, ym + t / 6, 0}, model, LY_OPEN, 1.0, LineStyle::Solid, C_WIN));
            jamb(x0, y0, x0, y1); jamb(x1, y0, x1, y1);
        } else {
            const double xm = 0.5 * (x0 + x1);
            out->push_back(ln({x0, y0, 0}, {x0, y1, 0}, model, LY_OPEN, 1.0, LineStyle::Solid, C_WIN));
            out->push_back(ln({x1, y0, 0}, {x1, y1, 0}, model, LY_OPEN, 1.0, LineStyle::Solid, C_WIN));
            out->push_back(ln({xm - t / 6, y0, 0}, {xm - t / 6, y1, 0}, model, LY_OPEN, 1.0, LineStyle::Solid, C_WIN));
            out->push_back(ln({xm + t / 6, y0, 0}, {xm + t / 6, y1, 0}, model, LY_OPEN, 1.0, LineStyle::Solid, C_WIN));
            jamb(x0, y0, x1, y0); jamb(x0, y1, x1, y1);
        }
        R->windowCount++;
    }

    // 门：门扇 + 90° 开启弧（半径 = 洞口宽）
    void doorPiece(double x0, double y0, double x1, double y1, const OpeningSpec& op) {
        bool horiz = (y1 - y0) <= (x1 - x0);
        jamb(x0, y0, x0, y1);
        jamb(x1, y0, x1, y1);
        const double w = op.width;
        Vec3 center, leafEnd;
        double a0 = 0, a1 = 90;
        if (horiz) {
            const double ym = 0.5 * (y0 + y1);
            const double hx = op.hinge == 0 ? x0 : x1;
            const double dy = op.swing >= 0 ? w : -w;
            center  = Vec3(hx, ym, 0);
            leafEnd = Vec3(hx, ym + dy, 0);
            if (op.hinge == 0 && op.swing >= 0)      { a0 = 0;   a1 = 90;  }
            else if (op.hinge == 1 && op.swing >= 0) { a0 = 90;  a1 = 180; }
            else if (op.hinge == 0)                  { a0 = -90; a1 = 0;   }
            else                                     { a0 = 180; a1 = 270; }
        } else {
            const double xm = 0.5 * (x0 + x1);
            const double hy = op.hinge == 0 ? y0 : y1;
            const double dx = op.swing >= 0 ? w : -w;
            center  = Vec3(xm, hy, 0);
            leafEnd = Vec3(xm + dx, hy, 0);
            if (op.hinge == 0 && op.swing >= 0)      { a0 = 0;   a1 = 90;  }
            else if (op.hinge == 1 && op.swing >= 0) { a0 = -90; a1 = 0;   }
            else if (op.hinge == 0)                  { a0 = 90;  a1 = 180; }
            else                                     { a0 = 180; a1 = 270; }
        }
        out->push_back(ln(center, leafEnd, model, LY_OPEN, 1.2));     // 门扇
        Element arc = ElementFactory::createArc(center, w, a0, a1, model);
        arc.layer = LY_OPEN; arc.color = C_INK;
        arc.lineWidthAuto = false; arc.lineWidth = 0.6;
        out->push_back(arc);
        R->doorCount++;
    }

    // 推拉门：两片错位滑扇 + 端头封口
    void slidePiece(double x0, double y0, double x1, double y1, const OpeningSpec& op) {
        bool horiz = (y1 - y0) <= (x1 - x0);
        jamb(x0, y0, x0, y1);
        jamb(x1, y0, x1, y1);
        const double w = op.width;
        if (horiz) {
            const double ym = 0.5 * (y0 + y1);
            out->push_back(ln({x0, ym + (y1 - y0) * 0.25, 0}, {x0 + w * 0.5, ym + (y1 - y0) * 0.25, 0},
                              model, LY_OPEN, 1.2));
            out->push_back(ln({x1 - w * 0.5, ym - (y1 - y0) * 0.25, 0}, {x1, ym - (y1 - y0) * 0.25, 0},
                              model, LY_OPEN, 1.2));
        } else {
            const double xm = 0.5 * (x0 + x1);
            out->push_back(ln({xm + (x1 - x0) * 0.25, y0, 0}, {xm + (x1 - x0) * 0.25, y0 + w * 0.5, 0},
                              model, LY_OPEN, 1.2));
            out->push_back(ln({xm - (x1 - x0) * 0.25, y1 - w * 0.5, 0}, {xm - (x1 - x0) * 0.25, y1, 0},
                              model, LY_OPEN, 1.2));
        }
        R->doorCount++;
    }

    void run(WallSpec& wall, const Transform& tf) {
        // 沿墙起点→终点的长度（模型）。注意：OpeningSpec.offset 在 defaultLayoutImpl
        // 中已按"相对本墙起点"的模型 mm 给出并随布局缩放，故这里直接使用，不可再减墙起点。
        const bool horiz = wallIsHorizontal(wall);
        const double lenM = horiz ? (wall.x1 - wall.x0) : (wall.y1 - wall.y0);
        // 洞口换算成纸面偏移并排序
        std::vector<OpeningSpec> ops = wall.openings;
        std::sort(ops.begin(), ops.end(),
                  [](const OpeningSpec& a, const OpeningSpec& b) { return a.offset < b.offset; });
        // 相邻洞口去重叠（保底：不合法输入收敛）
        for (size_t i = 1; i < ops.size(); ++i)
            if (ops[i].offset < ops[i - 1].offset + ops[i - 1].width)
                ops[i].offset = ops[i - 1].offset + ops[i - 1].width;

        double cursor = 0;    // 模型偏移
        auto piecePaper = [&](double m0, double m1, double& pa, double& pb) {
            if (horiz) {
                pa = tf.X(wall.x0 + m0); pb = tf.X(wall.x0 + m1);
            } else {
                pa = tf.Y(wall.y0 + m0); pb = tf.Y(wall.y0 + m1);
            }
        };
        auto emitPiece = [&](double m0, double m1, const OpeningSpec* op) {
            if (m1 - m0 < 1.0) return;                     // 长度过短跳过
            double pa, pb;
            piecePaper(m0, m1, pa, pb);
            if (!op) {                                     // 实墙段
                if (horiz) solidPiece(pa, tf.Y(wall.y0), pb, tf.Y(wall.y1), wall.solid);
                else       solidPiece(tf.X(wall.x0), pa, tf.X(wall.x1), pb, wall.solid);
                return;
            }
            // 洞口宽在布局里是模型 mm；门扇长度 / 开启弧半径须换算成纸面 mm。
            OpeningSpec opP = *op;
            opP.width = op->width * tf.s;
            switch (opP.type) {
                case OT_Window: {
                    if (horiz) windowPiece(pa, tf.Y(wall.y0), pb, tf.Y(wall.y1));
                    else       windowPiece(tf.X(wall.x0), pa, tf.X(wall.x1), pb);
                    break;
                }
                case OT_Door: {
                    if (horiz) doorPiece(pa, tf.Y(wall.y0), pb, tf.Y(wall.y1), opP);
                    else       doorPiece(tf.X(wall.x0), pa, tf.X(wall.x1), pb, opP);
                    break;
                }
                case OT_Slide: {
                    if (horiz) slidePiece(pa, tf.Y(wall.y0), pb, tf.Y(wall.y1), opP);
                    else       slidePiece(tf.X(wall.x0), pa, tf.X(wall.x1), pb, opP);
                    break;
                }
            }
        };
        for (const auto& op : ops) {
            const double o0 = std::clamp(op.offset, 0.0, lenM);
            const double o1 = std::clamp(o0 + op.width, 0.0, lenM);
            emitPiece(cursor, o0, nullptr);                // 洞口前的实墙
            emitPiece(o0, o1, &op);                        // 洞口
            cursor = o1;
        }
        emitPiece(cursor, lenM, nullptr);                  // 末段实墙
    }
};

// ----------------------------- 数据预处理：默认户型布局 -----------------------------
// 附图 C 户型拓扑（基准 9200 × 11756，随 outerW/outerH 等比缩放）。
// 高差台阶：主体顶 10856，右上多功能间块（x 4400~7600）抬高至 11756。
// 实现见本文件下方的 defaultLayoutImpl（需 X()/Y() 缩放与洞口表，统一在此完成）。

} // anonymous namespace

// ----------------------------- 默认布局 -----------------------------
// 基准 9200 × 11756；主体顶 10856；右上多功能间块抬高至 11756。
namespace {
std::vector<WallSpec> defaultLayoutImpl(const FloorPlanParams& p) {
    const double kW = 9200.0, kH = 11756.0;
    const double kx = p.outerW / kW, ky = p.outerH / kH;
    auto X = [kx](double v) { return v * kx; };
    auto Y = [ky](double v) { return v * ky; };

    std::vector<WallSpec> ws;
    auto addWall = [&](double x0, double y0, double x1, double y1, bool solid,
                       std::vector<OpeningSpec> ops) {
        WallSpec w;
        w.x0 = X(x0); w.y0 = Y(y0); w.x1 = X(x1); w.y1 = Y(y1);
        w.solid = solid;
        for (const auto& op : ops) {
            OpeningSpec o = op;
            // 洞口沿墙坐标随对应轴向缩放
            o.offset = wallIsHorizontal(w) ? op.offset * kx : op.offset * ky;
            w.openings.push_back(o);
        }
        ws.push_back(w);
    };

    // =====================================================================
    // 外围剪力墙（不规则轮廓，附图量取，误差约 ±250mm）
    //   · 次卧西墙凸出至 X=0（其余西墙内退至 X=2550）→ 西侧不规则凸角
    //   · 阳台/生活阳台向南凸出至 Y=0（次卧/主卧底在 Y=1100）→ 南侧不规则凸角
    //   · 客卫东墙内退至 X=8950（主卧/主卫东墙在 X=9200）→ 东侧阶梯状凸角
    //   · 右上多功能间块抬高 900（Y 10856→11756）
    // =====================================================================
    // 西墙-下（次卧西，凸出至 X=0）
    addWall(0, 1100, 200, 5930, true, { { 1400, 1100, OT_Window } });
    // 西墙-上（玄关/厨房西，内退至 X=2550）：入户门 + 玄关高窗
    addWall(2550, 5930, 2750, 10656, true, { { 70, p.doorW, OT_Door, 0, 1 },
                                              { 2000, 1000, OT_Window } });
    // 西转角接墙（水平，连接两段西墙，形成凸角）
    addWall(0, 5730, 2750, 5930, true, {});
    // 南墙-次卧底
    addWall(0, 1100, 2900, 1300, true, {});
    // 阳台西墙（向南凸出）
    addWall(2900, 0, 3100, 1300, true, {});
    // 南墙-阳台（最南 Y=0）：阳台大窗
    addWall(2900, 0, 6430, 200, true, { { 1000, 1200, OT_Window }, { 3500, 1600, OT_Window } });
    // 阳台东墙
    addWall(6300, 0, 6500, 1300, true, {});
    // 南墙-主卧底：主卧窗
    addWall(6430, 1100, 9200, 1300, true, { { 6600, 1500, OT_Window } });
    // 东墙-主卧/主卫（X=9200）：主卧窗 / 主卫窗
    addWall(9000, 1100, 9200, 7755, true, { { 2200, 1100, OT_Window }, { 5600, 700, OT_Window } });
    // 东墙-客卫（内退 X=8950）：下接 / 客卫东窗 / 上接
    addWall(8950, 7655, 9200, 7755, true, {});
    addWall(8950, 7755, 9150, 9350, true, { { 300, 800, OT_Window } });
    addWall(8950, 9250, 9200, 9350, true, {});
    // 北墙-右段
    addWall(7600, 10656, 9200, 10856, true, {});
    // 多功能间抬高块：西墙 / 北墙（带窗）/ 东墙
    addWall(4400, 10856, 4600, 11756, true, {});
    addWall(4400, 11556, 7600, 11756, true, { { 500, 1400, OT_Window } });
    addWall(7400, 10856, 7600, 11756, true, {});
    // 北墙-左段（含西凹口上方）：厨房北窗
    addWall(0, 10656, 4400, 10856, true, { { 2700, 1000, OT_Window } });
    // 多功能间底部结构带：多功能间门（自餐厅进入）
    addWall(4400, 10656, 7600, 10856, true, { { 900, 900, OT_Door, 0, 1 } });

    // =====================================================================
    // 内隔墙（双线）：逐个房间闭合，消除中间墙体缺口
    //   左列：次卧(X0-2750) / 玄关(X2750-4400) / 厨房(X2750-4400)
    //   中列：餐厅(X2750-5600) / 客厅(X2750-5600) / 走廊(X5600-7000)
    //   右列：主卧(X5600-9000) / 主卫(X7000-9000) / 客卫(X7100-8950)
    //   顶中：家政间(X4400-7000) ；底中：阳台/生活阳台(X2900-6430)
    // =====================================================================
    // 次卧东墙（→客厅/玄关）：次卧门
    addWall(2650, 1300, 2750, 5730, false, { { 3800, 900, OT_Door, 0, -1 }, { 1000, 900, OT_Window } });
    // 玄关东墙（→餐厅）：玄关门
    addWall(4300, 5930, 4400, 8356, false, { { 700, 900, OT_Door, 0, -1 } });
    // 厨房东墙（→家政/客卫）：厨房后门
    addWall(4300, 8356, 4400, 10656, false, { { 700, 900, OT_Door, 0, 1 }, { 1200, 900, OT_Window } });
    // 厨房南墙：厨房门
    addWall(2750, 8356, 4400, 8456, false, { { 1500, 700, OT_Door, 0, 1 } });
    // 餐厅东墙（→走廊）：餐厅门
    addWall(5500, 4350, 5600, 8356, false, { { 1000, 900, OT_Door, 0, 1 } });
    // 客厅东墙（→走廊/主卧西）
    addWall(5500, 1300, 5600, 4450, false, {});
    // 餐厅底/客厅顶（敞开式，仍以墙分段界定）
    addWall(2750, 4250, 5600, 4350, false, {});
    // 客厅南墙（→阳台）：推拉门
    addWall(2900, 1300, 5600, 1400, false, { { 400, p.slideDoorW, OT_Slide } });
    // 主卧北墙（→走廊）：主卧门
    addWall(5600, 4350, 9000, 4450, false, { { 500, 900, OT_Door, 0, -1 } });
    // 主卫西墙（→走廊）：主卫门
    addWall(7000, 4450, 7100, 7700, false, { { 300, 700, OT_Door, 0, 1 } });
    // 客卫西墙（→走廊）：客卫门
    addWall(7000, 7700, 7100, 9350, false, { { 400, 900, OT_Door, 0, -1 } });
    // 家政间南墙（→客卫顶）
    addWall(4400, 9250, 7000, 9350, false, {});
    // 家政间西墙补足（X4400 上段，使顶中列闭合）
    addWall(4300, 8456, 4400, 9350, false, {});
    return ws;
}
} // anonymous namespace

// ----------------------------- 尺寸链（建筑斜短线式，三级） -----------------------------
// 一条边上的测量节点：pos = 沿边模型坐标；extFrom = 界线起点的垂直方向模型坐标
// （墙面/台阶棱角，界线自该点贯通至最外级链外 2mm，对应附图的通长界线风格）。
namespace {
struct DimSide {
    std::vector<double> pos;
    std::vector<double> extFrom;
};

struct ChainBuilder {
    std::vector<Element>* out = nullptr;
    const SketchModel* model = nullptr;
    FloorPlanViewsResult* R = nullptr;
    double textH = 2.5;
    double o1 = 5.0, o2 = 10.0, o3 = 15.0;   // 三级链距平面边缘的纸面偏移

    void setOffsets(double band) {
        o3 = std::max(6.0, band - 4.0);
        o2 = o3 * 2.0 / 3.0;
        o1 = o3 / 3.0;
    }

    // 斜短线：过 (x,y) 的 45° 短粗线（纸面长 1.6mm）
    void tick(double x, double y) {
        const double h = 0.8;
        out->push_back(ln({x - h, y - h, 0}, {x + h, y + h, 0}, model, LY_DIM, 2.0));
    }

    // 水平链组（三级）：edgeY = 平面边缘纸面 Y，above = 链位于平面上方。
    // 数字水平书写，居中于尺寸线上方（对齐附图）。
    void hChainSet(const DimSide& s, const Transform& tf, double edgeY, bool above) {
        const int n = int(s.pos.size());
        if (n < 2) return;
        const double dir = above ? 1.0 : -1.0;
        std::vector<double> sx(n);
        for (int i = 0; i < n; ++i) sx[i] = tf.X(s.pos[i]);
        for (int i = 0; i < n; ++i)   // 界线：自墙面贯通至最外级链外 2mm
            out->push_back(ln({sx[i], tf.Y(s.extFrom[i]), 0},
                              {sx[i], edgeY + dir * (o3 + 2.0), 0}, model, LY_DIM, 0.6));
        const double offs[3] = { o1, o2, o3 };
        for (int k = 0; k < 3; ++k) {
            const double ly = edgeY + dir * offs[k];
            out->push_back(ln({sx.front(), ly, 0}, {sx.back(), ly, 0}, model, LY_DIM, 0.8));
            for (int i = 0; i < n; ++i) tick(sx[i], ly);
            for (int i = 1; i < n; ++i) {
                const double mid = 0.5 * (sx[i - 1] + sx[i]);
                const QString t = QString::number(int(std::lround(s.pos[i] - s.pos[i - 1])));
                out->push_back(txt({mid - digitW(t, textH) * 0.5, ly + 0.8, 0},
                                   t, textH, model, LY_TEXT, C_DIM));
                R->dimCount++;
            }
            R->dimChainCount++;
        }
    }

    // 竖直链组（三级）：edgeX = 平面边缘纸面 X，right = 链位于平面右侧。
    // 数字水平书写，居中于尺寸线上（对齐附图；三级链间距 5mm > 字高，互不侵占）。
    void vChainSet(const DimSide& s, const Transform& tf, double edgeX, bool right) {
        const int n = int(s.pos.size());
        if (n < 2) return;
        const double dir = right ? 1.0 : -1.0;
        std::vector<double> sy(n);
        for (int i = 0; i < n; ++i) sy[i] = tf.Y(s.pos[i]);
        for (int i = 0; i < n; ++i)
            out->push_back(ln({tf.X(s.extFrom[i]), sy[i], 0},
                              {edgeX + dir * (o3 + 2.0), sy[i], 0}, model, LY_DIM, 0.6));
        const double offs[3] = { o1, o2, o3 };
        for (int k = 0; k < 3; ++k) {
            const double lx = edgeX + dir * offs[k];
            out->push_back(ln({lx, sy.front(), 0}, {lx, sy.back(), 0}, model, LY_DIM, 0.8));
            for (int i = 0; i < n; ++i) tick(lx, sy[i]);
            for (int i = 1; i < n; ++i) {
                const double mid = 0.5 * (sy[i - 1] + sy[i]);
                const QString t = QString::number(int(std::lround(s.pos[i] - s.pos[i - 1])));
                out->push_back(txt({lx - digitW(t, textH) * 0.5, mid - textH * 0.35, 0},
                                   t, textH, model, LY_TEXT, C_DIM));
                R->dimCount++;
            }
            R->dimChainCount++;
        }
    }
};

// ----------------------------- 引线标注 -----------------------------
// 箭头 + 折线（斜段 → 水平尾线）+ 尾线下方多行文字（右对齐）。
// (tipMx,tipMy) 为箭头尖端（模型 mm）；(dx,dy) 为尖端到折角点的纸面偏移；
// tailLen 为水平尾线长（向左，纸面 mm）。对齐附图左下的引线说明形式。
void addLeader(std::vector<Element>& out, const Transform& tf, const SketchModel* m,
               double tipMx, double tipMy, double dx, double dy, double tailLen,
               const std::vector<QString>& lines, double textH) {
    const double tx = tf.X(tipMx), ty = tf.Y(tipMy);
    const double ex = tx + dx, ey = ty + dy;      // 折角点（尾线起点）
    const double ex2 = ex - tailLen;              // 尾线末端（文字右缘）
    out.push_back(ln({tx, ty, 0}, {ex, ey, 0}, m, LY_DIM, 0.8));
    out.push_back(ln({ex, ey, 0}, {ex2, ey, 0}, m, LY_DIM, 0.8));
    // 箭头：自尖端沿指向折角方向张开的两个短翼（±25°）
    const double ux = ex - tx, uy = ey - ty;
    const double L = std::sqrt(ux * ux + uy * uy);
    if (L > 1e-6) {
        const double nx = ux / L, ny = uy / L;
        const double al = 2.0, ca = std::cos(0.44), sa = std::sin(0.44);
        auto wing = [&](double sign) {
            const double wx = nx * ca - sign * ny * sa;
            const double wy = ny * ca + sign * nx * sa;
            out.push_back(ln({tx, ty, 0}, {tx + al * wx, ty + al * wy, 0}, m, LY_DIM, 1.2));
        };
        wing(1.0); wing(-1.0);
    }
    // 多行文字：右对齐于尾线末端，位于尾线下方
    double yy = ey - 1.2 - textH;
    for (const auto& s : lines) {
        const double w = s.size() * textH;        // CJK ≈ 1em/字（粗估）
        out.push_back(txt({ex2 - w, yy, 0}, s, textH, m, LY_TEXT, C_DIM));
        yy -= textH * 1.3;
    }
}

// ----------------------------- 家具图块 -----------------------------
// 约定：图块位置由模型坐标（随布局缩放）给出；零件尺寸为模型 mm × 比例。
struct FurnBuilder {
    std::vector<Element>* out = nullptr;
    const SketchModel* model = nullptr;
    FloorPlanViewsResult* R = nullptr;
    double w = 0.8;    // 线宽
    double s = 0.02;   // 比例（模型 → 纸面）
    double sm(double mm) const { return mm * s; }

    void rect(double x0, double y0, double x1, double y1) {
        addRect4(*out, x0, y0, x1, y1, model, LY_FURN, w, C_FURN);
        R->furnitureCount++;
    }
    void line(double x0, double y0, double x1, double y1) {
        out->push_back(ln({x0, y0, 0}, {x1, y1, 0}, model, LY_FURN, w, LineStyle::Solid, C_FURN));
    }
    void circle(double cx, double cy, double r) {
        Element e = ElementFactory::createCircle(Vec3(cx, cy, 0), r, model);
        e.layer = LY_FURN; e.color = C_FURN;
        e.lineWidthAuto = false; e.lineWidth = w;
        out->push_back(e);
    }
    void ellipse(double cx, double cy, double rx, double ry) {
        Element e = ElementFactory::createEllipse(Vec3(cx, cy, 0), rx, ry, 0, model);
        e.layer = LY_FURN; e.color = C_FURN;
        e.lineWidthAuto = false; e.lineWidth = w;
        out->push_back(e);
    }
    // 双人床：床身 + 两个枕头 + 被沿折线（床头在北）
    void bed(double x0, double y0, double bw, double bh) {
        rect(x0, y0, x0 + bw, y0 + bh);
        const double pw = bw * 0.4, ph = bh * 0.12, headY = y0 + bh - ph - sm(60);
        rect(x0 + bw * 0.08, headY, x0 + bw * 0.08 + pw, headY + ph);
        rect(x0 + bw * 0.52, headY, x0 + bw * 0.52 + pw, headY + ph);
        line(x0, y0 + bh * 0.72, x0 + bw, y0 + bh * 0.72);
    }
    // 坐便器：水箱 + 椭圆坐圈
    void toilet(double cx, double cy, double dirX) {
        const double th = sm(420);
        rect(cx - sm(100), cy + th - sm(100), cx + sm(100), cy + th + sm(100));
        ellipse(cx, cy + th * 0.15, sm(190), sm(240));
        Q_UNUSED(dirX);
    }
    // 洗手盆：台面矩形 + 椭圆盆
    void basin(double cx, double cy) {
        rect(cx - sm(250), cy - sm(180), cx + sm(250), cy + sm(180));
        ellipse(cx, cy, sm(150), sm(110));
    }
};

void buildFurniture(std::vector<Element>& out, const FloorPlanParams& p,
                    const Transform& tf, const SketchModel* m, FloorPlanViewsResult& R) {
    if (!p.showFurniture) return;
    FurnBuilder fb{ &out, m, &R, 0.8, tf.s };
    const double kW = 9200.0, kH = 11756.0;
    const double kx = p.outerW / kW, ky = p.outerH / kH;
    auto PX = [tf, kx](double v) { return tf.X(v * kx); };   // 模型基准坐标 → 纸面
    auto PY = [tf, ky](double v) { return tf.Y(v * ky); };
    auto rw = [tf](double mm) { return mm * tf.s; };          // 模型长度 → 纸面

    auto R4 = [&](double a, double b, double c, double d) { fb.rect(PX(a), PY(b), PX(c), PY(d)); };
    auto LN = [&](double a, double b, double c, double d) { fb.line(PX(a), PY(b), PX(c), PY(d)); };
    auto WD = [&](double a, double b, double c, double d) {   // 衣柜/储物：矩形 + 对角线交叉
        R4(a, b, c, d); LN(a, b, c, d); LN(a, d, c, b);
    };
    auto BED = [&](double a, double b, double wmm, double hmm) { fb.bed(PX(a), PY(b), rw(wmm), rw(hmm)); };

    // 厨房：北墙操作台 + 水盆 + 灶（两圆）+ 冰箱
    R4(2300, 10100, 4400, 10656);
    fb.ellipse(PX(3850), PY(10380), rw(300), rw(200));
    fb.circle(PX(2650), PY(10380), rw(150));
    fb.circle(PX(3000), PY(10380), rw(150));
    R4(2300, 8600, 2950, 9350);

    // 多功能间：两侧到顶衣柜（对角线交叉）
    WD(4650, 10900, 5450, 11500);
    WD(6550, 10900, 7350, 11500);

    // 餐厅：餐桌 + 4 椅
    R4(5000, 9000, 6200, 9900);
    R4(4700, 9050, 4950, 9450); R4(4700, 9550, 4950, 9950);
    R4(6250, 9050, 6500, 9450); R4(6250, 9550, 6500, 9950);

    // 客厅：沙发（南墙，背朝南）+ 茶几 + 电视柜（贴走廊墙）
    R4(2600, 2100, 4800, 2950);
    LN(2600, 2250, 4800, 2250);
    R4(3300, 3300, 4300, 3900);
    R4(5350, 4000, 5600, 5200);

    // 次卧：双人床 + 衣柜
    BED(400, 2500, 1500, 2000);
    WD(1800, 1200, 2150, 2400);

    // 主卧：双人床 + 衣柜
    BED(6600, 1500, 1700, 2000);
    WD(5800, 1300, 6450, 3300);

    // 主卫：坐便 + 洗手盆 + 淋浴房
    fb.toilet(PX(7450), PY(5600), 1);
    fb.basin(PX(7450), PY(6500));
    R4(8250, 5450, 8950, 6150);
    fb.circle(PX(8350), PY(5550), rw(80));

    // 客卫：坐便 + 洗手盆 + 洗衣机
    fb.toilet(PX(7500), PY(7900), 1);
    fb.basin(PX(8600), PY(8100));
    R4(7250, 8900, 7850, 9300);
    fb.circle(PX(7550), PY(9100), rw(210));

    // 生活阳台：洗衣机
    R4(4950, 300, 5550, 900);
    fb.circle(PX(5250), PY(600), rw(210));

    // 玄关/储物：通高储物柜（对角线交叉）
    WD(250, 6900, 850, 8300);
}

// ----------------------------- 房间名标注 -----------------------------
// 名称与位置对齐附图（「卫生」为附图对卫生间的简称写法）。
void buildLabels(std::vector<Element>& out, const FloorPlanParams& p,
                 const Transform& tf, const SketchModel* m, FloorPlanViewsResult& R) {
    if (!p.showLabels) return;
    const double kW = 9200.0, kH = 11756.0;
    const double kx = p.outerW / kW, ky = p.outerH / kH;
    struct Lbl { double x, y; const char* name; };
    static const Lbl lbls[] = {
        { 1200, 6100,  "入户" },
        { 3300, 9500,  "厨房" },
        { 6000, 11200, "多功能间" },
        { 5000, 8700,  "餐厅" },
        { 3800, 5000,  "客厅" },
        { 8100, 8600,  "卫生" },
        { 8100, 6150,  "卫生" },
        { 7300, 3000,  "主卧" },
        { 1200, 3000,  "卧室" },
        { 4200, 650,   "阳台" },
        { 5600, 650,   "生活阳台" },
    };
    for (const auto& L : lbls) {
        out.push_back(txt(Vec3(tf.X(L.x * kx), tf.Y(L.y * ky), 0), QString::fromUtf8(L.name),
                          p.labelTextH, m, LY_TEXT));
        R.labelCount++;
    }
}

// ----------------------------- 装饰图签（行序对齐附图） -----------------------------
void buildTitleBlock(std::vector<Element>& out, const FloorPlanParams& p,
                     const SketchModel* m, FloorPlanViewsResult& R) {
    const double x0 = p.sheetW - p.margin - p.titleBlockW;
    const double x1 = p.sheetW - p.margin;
    const double y0 = p.margin, y1 = p.sheetH - p.margin;
    const double hh = y1 - y0;
    const double w = x1 - x0;
    const double th = p.titleTextH;

    enum RowKind { R_Logo, R_Field, R_Sign, R_DwgName, R_TwoCol, R_Full, R_Note };
    struct Row { RowKind kind; double raw; const char* a; QString b; };
    const Row rows[] = {
        { R_Logo,    0.085, "设计单位：",   p.company },
        { R_Field,   0.042, "设计单位：",   p.companyFull },
        { R_Field,   0.042, "工程地址：",   p.projectSite },
        { R_Field,   0.042, "工程名称：",   p.projectName },
        { R_Field,   0.042, "项目名称：",   p.client },
        { R_Sign,    0.026, "审  核",       QString() },
        { R_Sign,    0.026, "校  对",       QString() },
        { R_Sign,    0.026, "批  准",       QString() },
        { R_Sign,    0.026, "专业负责人",   QString() },
        { R_Sign,    0.026, "复  计",       QString() },
        { R_Sign,    0.026, "图  名",       QString() },
        { R_DwgName, 0.060, "图纸名称：",   p.drawingName },
        { R_TwoCol,  0.032, "专    业",     QStringLiteral("设计阶段") },
        { R_TwoCol,  0.032, "图    号",     QStringLiteral("比    例") },
        { R_Full,    0.035, "平面方案",     QString() },
        { R_Full,    0.050, "快速搜索",     QString() },
        { R_Full,    0.050, "出版图签",     QString() },
        { R_Note,    0.045, "本图纸尺寸以现场放线为准", QString() },
    };
    const int n = int(sizeof(rows) / sizeof(rows[0]));
    double rawSum = 0;
    for (int i = 0; i < n; ++i) rawSum += rows[i].raw;

    double yTop = y1;
    int field = 0;
    for (int i = 0; i < n; ++i) {
        const double rh = rows[i].raw / rawSum * hh;
        const double yb = yTop - rh;
        if (i > 0)
            out.push_back(ln({x0, yb, 0}, {x1, yb, 0}, m, LY_TITLE, 1.0));
        const double ty = yb + rh * 0.5 - th * 0.35;
        switch (rows[i].kind) {
            case R_Logo: {
                out.push_back(txt({x0 + 3, yTop - th * 1.1, 0},
                                  QString::fromUtf8(rows[i].a), th * 0.85, m, LY_TITLE));
                // logo 方块 + 斜杠纹样 + 公司标识
                const double lx = x0 + w * 0.08, ly = yb + rh * 0.24, ls = rh * 0.52;
                addRect4(out, lx, ly, lx + ls, ly + ls, m, LY_TITLE, 1.4);
                out.push_back(ln({lx, ly, 0}, {lx + ls, ly + ls, 0}, m, LY_TITLE, 1.0));
                out.push_back(ln({lx, ly + ls, 0}, {lx + ls, ly, 0}, m, LY_TITLE, 1.0));
                out.push_back(txt({x0 + w * 0.30, yb + rh * 0.22, 0}, rows[i].b,
                                  th * 1.25, m, LY_TITLE));
                field += 2;
                break;
            }
            case R_Field:
                out.push_back(txt({x0 + 3, ty, 0}, QString::fromUtf8(rows[i].a),
                                  th * 0.9, m, LY_TITLE));
                out.push_back(txt({x0 + w * 0.30, ty, 0}, rows[i].b, th * 0.9, m, LY_TITLE));
                field += 2;
                break;
            case R_Sign: {
                // 三栏签字行：标签 | 签名格 | 日期格
                const double vx1 = x0 + w * 0.48, vx2 = x0 + w * 0.74;
                out.push_back(ln({vx1, yb, 0}, {vx1, yTop, 0}, m, LY_TITLE, 0.8));
                out.push_back(ln({vx2, yb, 0}, {vx2, yTop, 0}, m, LY_TITLE, 0.8));
                out.push_back(txt({x0 + 3, ty, 0}, QString::fromUtf8(rows[i].a),
                                  th * 0.85, m, LY_TITLE));
                out.push_back(txt({x0 + w * 0.77, ty, 0}, QStringLiteral("日 期"),
                                  th * 0.85, m, LY_TITLE));
                field += 2;
                break;
            }
            case R_DwgName:
                out.push_back(txt({x0 + 3, yTop - th * 1.1, 0},
                                  QString::fromUtf8(rows[i].a), th * 0.8, m, LY_TITLE));
                out.push_back(txt({x0 + w * 0.05, yb + rh * 0.16, 0}, rows[i].b,
                                  th * 1.35, m, LY_TITLE));
                field += 2;
                break;
            case R_TwoCol: {
                const double vm = x0 + w * 0.5;
                out.push_back(ln({vm, yb, 0}, {vm, yTop, 0}, m, LY_TITLE, 0.8));
                out.push_back(txt({x0 + w * 0.06, ty, 0}, QString::fromUtf8(rows[i].a),
                                  th * 0.9, m, LY_TITLE));
                out.push_back(txt({x0 + w * 0.56, ty, 0}, rows[i].b, th * 0.9, m, LY_TITLE));
                field += 2;
                break;
            }
            case R_Full:
                out.push_back(txt({x0 + w * 0.06, ty, 0}, QString::fromUtf8(rows[i].a),
                                  th, m, LY_TITLE));
                field += 1;
                break;
            case R_Note:
                out.push_back(txt({x0 + 3, ty, 0}, QString::fromUtf8(rows[i].a),
                                  th * 0.8, m, LY_TITLE));
                field += 1;
                break;
        }
        yTop = yb;
    }
    R.titleFieldCount = field;
}

} // anonymous namespace

// ----------------------------- 参数收敛与校验 -----------------------------
void FloorPlanParams::makeValid() {
    outerW = std::clamp(outerW, 1000.0, 100000.0);
    outerH = std::clamp(outerH, 1000.0, 100000.0);
    shearWallT = std::clamp(shearWallT, 50.0, 600.0);
    partWallT  = std::clamp(partWallT, 50.0, 400.0);
    doorW      = std::clamp(doorW, 600.0, 2000.0);
    bathDoorW  = std::clamp(bathDoorW, 600.0, 2000.0);
    slideDoorW = std::clamp(slideDoorW, 900.0, 4000.0);
    winDefaultW= std::clamp(winDefaultW, 600.0, 4000.0);
    labelTextH = std::clamp(labelTextH, 1.5, 10.0);
    dimTextH   = std::clamp(dimTextH, 1.2, 8.0);
    titleTextH = std::clamp(titleTextH, 1.5, 10.0);
    margin     = std::clamp(margin, 5.0, 40.0);
    titleBlockW= std::clamp(titleBlockW, 60.0, sheetW * 0.5);
    dimBand    = std::clamp(dimBand, 10.0, 60.0);
    manualScale= std::clamp(manualScale, 1.0, 2000.0);
}

QString FloorPlanParams::validate() const {
    if (outerW < 1000 || outerH < 1000) return QStringLiteral("户型总尺寸过小");
    if (shearWallT < 50 || shearWallT > 600) return QStringLiteral("剪力墙厚越界");
    if (partWallT < 50 || partWallT > 400)   return QStringLiteral("内墙厚越界");
    if (doorW < 600 || bathDoorW < 600)      return QStringLiteral("门宽过小");
    if (sheetW < margin * 2 + titleBlockW + 2 * dimBand + outerW / 500.0)
        return QStringLiteral("图纸放不下最小布置");
    if (sheetH < margin * 2 + 2 * dimBand + outerH / 1000.0)
        return QStringLiteral("图纸高度不足");
    if (labelTextH < 1.5 || dimTextH < 1.2 || titleTextH < 1.5) return QStringLiteral("字高过小");
    return QString();
}

// ----------------------------- 主入口 -----------------------------
FloorPlanViewsResult buildFloorPlanViews(const FloorPlanParams& p0, const SketchModel* model) {
    FloorPlanParams p = p0;
    p.makeValid();

    FloorPlanViewsResult R;
    R.area = QRectF(p.margin, p.margin,
                    p.sheetW - 2 * p.margin, p.sheetH - 2 * p.margin);

    // ---- 1. 比例：可用区扣除尺寸链带宽后取标准比例 ----
    const double availW = p.sheetW - 2 * p.margin - p.titleBlockW;
    const double availH = p.sheetH - 2 * p.margin;
    const double sMax = std::min((availW - 2 * p.dimBand) / p.outerW,
                                 (availH - 2 * p.dimBand) / p.outerH);
    double denom = p.autoScale ? 1e9 : p.manualScale;
    if (p.autoScale) {
        for (double cand : kArchScales)
            if (cand >= 1.0 / std::max(sMax, 1e-9)) { denom = cand; break; }
        if (denom > 1e8) denom = kArchScales[0];
    }
    R.scale = 1.0 / denom;
    R.scaleLabel = scaleText(denom);

    // ---- 2. 排版：平面图在可用区内居中 ----
    Transform tf;
    tf.s = R.scale;
    R.planW = p.outerW * R.scale;
    R.planH = p.outerH * R.scale;
    tf.ox = p.margin + (availW - R.planW) * 0.5;
    tf.oy = p.margin + (availH - R.planH) * 0.5;
    R.planX0 = tf.ox; R.planY0 = tf.oy;

    std::vector<Element>& out = R.elems;

    // ---- 3. 墙体（含洞口切分）----
    std::vector<WallSpec> walls = defaultLayoutImpl(p);
    WallBuilder wb; wb.out = &out; wb.model = model; wb.R = &R;
    for (auto& w : walls) wb.run(w, tf);

    // ---- 4. 家具 ----
    buildFurniture(out, p, tf, model, R);

    // ---- 5. 房间标注 ----
    buildLabels(out, p, tf, model, R);

    // ---- 6. 引线标注（附图左下降板说明）----
    if (p.showLabels) {
        addLeader(out, tf, model,
                  4300.0 * p.outerW / 9200.0, 900.0 * p.outerH / 11756.0,
                  -11.5, -6.5, 11.0,
                  { QStringLiteral("降板构造详见图集"), QStringLiteral("结构降板 -0.050") },
                  std::max(1.8, p.dimTextH));
    }

    // ---- 7. 四周三级尺寸链（分格 → 分间 → 总尺寸）----
    if (p.showDims) {
        ChainBuilder cb; cb.out = &out; cb.model = model; cb.R = &R;
        cb.textH = p.dimTextH;
        cb.setOffsets(p.dimBand);

        const double kW = 9200.0, kH0 = 11756.0;
        const double kx = p.outerW / kW, ky = p.outerH / kH0;
        auto mkSide = [](std::initializer_list<double> pos, double fp,
                         std::initializer_list<double> ext, double fe) {
            DimSide s;
            for (double v : pos) s.pos.push_back(v * fp);
            for (double v : ext) s.extFrom.push_back(v * fe);
            return s;
        };
        const double yTop = tf.Y(p.outerH), yBot = tf.Y(0.0);
        const double xLeft = tf.X(0.0), xRight = tf.X(p.outerW);
        const double fy = ky, fx = kx;

        // 顶部：分格（外墙洞口/台阶棱角）→ 分间 → 总尺寸
        cb.hChainSet(mkSide({ 0, 2700, 3700, 4400, 4900, 6300, 7600, 8100, 8900, 9200 }, fx,
                            { 10856, 10856, 10856, 11756, 11756, 11756, 11756, 10856, 10856, 10856 }, fy),
                     tf, yTop, true);
        cb.hChainSet(mkSide({ 0, 4400, 7600, 9200 }, fx,
                            { 10856, 11756, 11756, 10856 }, fy), tf, yTop, true);
        cb.hChainSet(mkSide({ 0, 9200 }, fx, { 10856, 10856 }, fy), tf, yTop, true);

        // 底部
        cb.hChainSet(mkSide({ 0, 700, 1800, 3300, 3500, 5100, 5600, 6600, 8100, 9200 }, fx,
                            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, fy), tf, yBot, false);
        cb.hChainSet(mkSide({ 0, 2200, 3300, 5600, 9200 }, fx,
                            { 0, 0, 0, 0, 0 }, fy), tf, yBot, false);
        cb.hChainSet(mkSide({ 0, 9200 }, fx, { 0, 0 }, fy), tf, yBot, false);

        // 左侧（总尺寸 11756 顶节点界线自多功能间块西北棱角引来）
        cb.vChainSet(mkSide({ 0, 1400, 2500, 4600, 5700, 6600, 7200, 8300, 10856 }, fy,
                            { 0, 0, 0, 0, 0, 0, 0, 0, 0 }, fx), tf, xLeft, false);
        cb.vChainSet(mkSide({ 0, 4600, 8356, 10856 }, fy,
                            { 0, 0, 0, 0 }, fx), tf, xLeft, false);
        cb.vChainSet(mkSide({ 0, 11756 }, fy, { 0, 4400 }, fx), tf, xLeft, false);

        // 右侧
        cb.vChainSet(mkSide({ 0, 2200, 3300, 4600, 5400, 5600, 6300, 6900, 7000, 7900, 8700, 10656, 10856 }, fy,
                            { 9200, 9200, 9200, 9200, 9200, 9200, 9200, 9200, 9200, 9200, 9200, 9200, 9200 }, fx),
                     tf, xRight, true);
        cb.vChainSet(mkSide({ 0, 4600, 7000, 10856 }, fy,
                            { 9200, 9200, 9200, 9200 }, fx), tf, xRight, true);
        cb.vChainSet(mkSide({ 0, 10856 }, fy, { 9200, 9200 }, fx), tf, xRight, true);
    }

    // ---- 8. 包围盒与落框检查（在图框/图签前计算，只覆盖平面图内容）----
    bool has = false;
    for (const auto& e : out) {
        if (e.type == ElementType::Text && !e.pts.empty()) {
            const double th = e.radius > 0.1 ? e.radius : 3.5;
            const double w = e.text.size() * th;   // 粗估文本宽（CJK ≈ 1em/字）
            const Vec3& a = e.pts[0];
            const QRectF tr(a.x, a.y - th * 0.3, w, th);
            if (!has) { R.bounds = tr; has = true; } else R.bounds = R.bounds.united(tr);
            continue;
        }
        for (const auto& v : e.pts) {
            if (!has) { R.bounds = QRectF(v.x, v.y, 0, 0); has = true; }
            else {
                if (v.x < R.bounds.left())   R.bounds.setLeft(v.x);
                if (v.x > R.bounds.right())  R.bounds.setRight(v.x);
                if (v.y < R.bounds.top())    R.bounds.setTop(v.y);
                if (v.y > R.bounds.bottom()) R.bounds.setBottom(v.y);
            }
        }
    }
    R.fits = R.bounds.left()   >= p.margin - 1e-6 &&
             R.bounds.right()  <= p.sheetW - p.margin + 1e-6 &&
             R.bounds.top()    >= p.margin - 1e-6 &&
             R.bounds.bottom() <= p.sheetH - p.margin + 1e-6;

    // ---- 9. 图框 + 图签（幅面线/图框在图框外缘，不计入内容包围盒）----
    addRect4(out, 0.0, 0.0, p.sheetW, p.sheetH, model, LY_TITLE, 1.0);              // 幅面线
    addRect4(out, p.margin, p.margin, p.sheetW - p.margin, p.sheetH - p.margin,
             model, LY_TITLE, 2.0);                                                  // 图框
    buildTitleBlock(out, p, model, R);
    return R;
}

} // namespace SketchCAD
