// =====================================================================
// FloorPlanViews —— 参数化户型平面图（建筑装修专业）实现
// =====================================================================
// 绘制算法总览（详见 docs/户型平面图参数化绘制-算法与参数说明.md）：
//   1. 数据预处理：defaultLayout() 把附图户型拓扑固化为「墙段运行表」——
//      每条墙是一个带厚度的轴对齐矩形（模型 mm），并携带洞口表（门/窗/推拉门，
//      以沿墙起点偏移 + 宽度描述）；
//   2. 墙体合成：buildWall() 把每条墙按洞口切成「实墙段 + 洞口段」——
//      实墙段画双线轮廓，剪力墙再叠一层 Hatch（sectionExempt=SmallAreaSolid
//      → 渲染层走 SolidFill 涂黑通道）；洞口段按类型画窗线/门扇+开启弧/推拉扇；
//   3. 家具图块：buildFurniture() 用直线/圆/椭圆/矩形组合床/沙发/桌椅/洁具等；
//   4. 尺寸链：buildHChain()/buildVChain() 画建筑斜短线(45°)式连续标注
//      （界线 + 尺寸线 + 斜短线 + 数字），两级链（分格 + 总尺寸）；
//   5. 标注与图签：房间名 Text + 右侧装饰图签（栏目线 + 文字）；
//   6. 比例与排版：按 A3 可用区（扣除图签与尺寸链带宽）取标准比例 1:50，
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
    double offset = 0;   // 沿墙起点偏移（模型 mm）
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
        // 沿墙起点→终点的单位向量与长度（模型）
        const bool horiz = wallIsHorizontal(wall);
        const double startM = horiz ? wall.x0 : wall.y0;   // 墙起点（模型绝对坐标）
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
            const double o0 = std::clamp(op.offset - startM, 0.0, lenM);
            const double o1 = std::clamp(o0 + op.width, 0.0, lenM);
            emitPiece(cursor, o0, nullptr);                // 洞口前的实墙
            emitPiece(o0, o1, &op);                        // 洞口
            cursor = o1;
        }
        emitPiece(cursor, lenM, nullptr);                  // 末段实墙
    }
};

// ----------------------------- 数据预处理：默认户型布局 -----------------------------
// 附图 C 户型拓扑（比例坐标 0~1，相对 outerW/outerH，可随总尺寸参数缩放）。
std::vector<WallSpec> defaultLayout(const FloorPlanParams& p) {
    const double W = p.outerW, H = p.outerH;
    const double tS = p.shearWallT, tP = p.partWallT;
    auto fx = [W](double v) { return v * W; };
    auto fy = [H](double v) { return v * H; };

    std::vector<WallSpec> ws;
    // ---- 外墙（剪力墙，涂黑）----
    {   // 左墙 y[0,H]，洞口自下而上
        WallSpec w; w.x0 = 0; w.y0 = 0; w.x1 = tS; w.y1 = H; w.solid = true;
        w.openings = {
            { fy(0.25), fy(0.15), OT_Window, 0, 1 },             // 次卧窗
            { fy(0.55), p.doorW,  OT_Door,   1, 1 },             // 入户门（铰位上端，向内开）
            { fy(0.66), fy(0.08), OT_Window, 0, 1 },             // 书房窗
        };
        ws.push_back(w);
    }
    {   // 下墙 x[0,W]
        WallSpec w; w.x0 = 0; w.y0 = 0; w.x1 = W; w.y1 = tS; w.solid = true;
        w.openings = {
            { fx(0.36), fx(0.14), OT_Window, 0, 1 },             // 生活阳台窗
            { fx(0.66), fx(0.16), OT_Window, 0, 1 },             // 主卧窗
        };
        ws.push_back(w);
    }
    {   // 右墙 y[0,H]
        WallSpec w; w.x0 = W - tS; w.y0 = 0; w.x1 = W; w.y1 = H; w.solid = true;
        w.openings = {
            { fy(0.12), fy(0.16), OT_Window, 0, 1 },             // 主卧窗
            { fy(0.53), fy(0.07), OT_Window, 0, 1 },            // 主卫窗
            { fy(0.86), fy(0.11), OT_Window, 0, 1 },            // 景观阳台窗
        };
        ws.push_back(w);
    }
    {   // 上墙 x[0,W]
        WallSpec w; w.x0 = 0; w.y0 = H - tS; w.x1 = W; w.y1 = H; w.solid = true;
        w.openings = {
            { fx(0.31), fx(0.11), OT_Window, 0, 1 },             // 厨房窗
            { fx(0.55), fx(0.15), OT_Window, 0, 1 },            // 景观阳台窗
        };
        ws.push_back(w);
    }

    // ---- 内隔墙（双线）----
    {   // 左户内墙 x=fx(0.28)，y[fy(0.17), fy(0.72)]：次卧门 + 书房门（向左开）
        WallSpec w; w.x0 = fx(0.28); w.y0 = fy(0.17); w.x1 = fx(0.28) + tP; w.y1 = fy(0.72);
        w.openings = {
            { fy(0.30), p.doorW, OT_Door, 0, -1 },
            { fy(0.56), p.doorW, OT_Door, 0, -1 },
        };
        ws.push_back(w);
    }
    {   // 厨房下墙 y=fy(0.72)，x[fx(0.28), fx(0.50)]：厨房门（向上开）
        WallSpec w; w.x0 = fx(0.28); w.y0 = fy(0.72); w.x1 = fx(0.50); w.y1 = fy(0.72) + tP;
        w.openings = { { fx(0.40), p.bathDoorW, OT_Door, 0, 1 } };
        ws.push_back(w);
    }
    {   // 厨房/景观阳台隔墙 x=fx(0.50)，y[fy(0.72), H]
        WallSpec w; w.x0 = fx(0.50); w.y0 = fy(0.72); w.x1 = fx(0.50) + tP; w.y1 = H;
        ws.push_back(w);
    }
    {   // 景观阳台/客卫隔墙 y=fy(0.83)，x[fx(0.50), fx(0.76)]
        WallSpec w; w.x0 = fx(0.50); w.y0 = fy(0.83); w.x1 = fx(0.76); w.y1 = fy(0.83) + tP;
        ws.push_back(w);
    }
    {   // 主卧左墙 x=fx(0.58)，y[0, fy(0.49)]：主卧门（向右开）
        WallSpec w; w.x0 = fx(0.58); w.y0 = 0; w.x1 = fx(0.58) + tP; w.y1 = fy(0.49);
        w.openings = { { fy(0.30), p.doorW, OT_Door, 0, 1 } };
        ws.push_back(w);
    }
    {   // 客卫/过道隔墙 x=fx(0.58)，y[fy(0.49), fy(0.83)]
        WallSpec w; w.x0 = fx(0.58); w.y0 = fy(0.49); w.x1 = fx(0.58) + tP; w.y1 = fy(0.83);
        ws.push_back(w);
    }
    {   // 客卫下墙 y=fy(0.61)，x[fx(0.58), fx(0.75)]：客卫门（向上开）
        WallSpec w; w.x0 = fx(0.58); w.y0 = fy(0.61); w.x1 = fx(0.75); w.y1 = fy(0.61) + tP;
        w.openings = { { fx(0.63), p.bathDoorW, OT_Door, 0, 1 } };
        ws.push_back(w);
    }
    {   // 主卫西墙 x=fx(0.75)，y[fy(0.49), fy(0.83)]：主卫门（向右开）
        WallSpec w; w.x0 = fx(0.75); w.y0 = fy(0.49); w.x1 = fx(0.75) + tP; w.y1 = fy(0.83);
        w.openings = { { fy(0.52), p.bathDoorW, OT_Door, 0, 1 } };
        ws.push_back(w);
    }
    {   // 主卧/主卫北墙 y=fy(0.49)，x[fx(0.58), fx(0.94)]
        WallSpec w; w.x0 = fx(0.58); w.y0 = fy(0.49); w.x1 = fx(0.94); w.y1 = fy(0.49) + tP;
        ws.push_back(w);
    }
    {   // 主卫北墙 y=fy(0.66)，x[fx(0.75), fx(0.94)]
        WallSpec w; w.x0 = fx(0.75); w.y0 = fy(0.66); w.x1 = fx(0.94); w.y1 = fy(0.66) + tP;
        ws.push_back(w);
    }
    {   // 主卫东墙 x=fx(0.94)，y[fy(0.49), fy(0.66)]
        WallSpec w; w.x0 = fx(0.94); w.y0 = fy(0.49); w.x1 = fx(0.94) + tP; w.y1 = fy(0.66);
        ws.push_back(w);
    }
    {   // 生活阳台北墙 y=fy(0.17)，x[fx(0.31), fx(0.58)]：推拉门
        WallSpec w; w.x0 = fx(0.31); w.y0 = fy(0.17); w.x1 = fx(0.58); w.y1 = fy(0.17) + tP;
        w.openings = { { fx(0.36), p.slideDoorW, OT_Slide, 0, 1 } };
        ws.push_back(w);
    }
    {   // 生活阳台西墙 x=fx(0.31)，y[0, fy(0.17)]
        WallSpec w; w.x0 = fx(0.31); w.y0 = 0; w.x1 = fx(0.31) + tP; w.y1 = fy(0.17);
        ws.push_back(w);
    }
    return ws;
}

// ----------------------------- 尺寸链（建筑斜短线式） -----------------------------
struct ChainBuilder {
    std::vector<Element>* out = nullptr;
    const SketchModel* model = nullptr;
    FloorPlanViewsResult* R = nullptr;
    double textH = 2.5;

    // 斜短线：过 (x,y) 的 45° 短粗线（纸面长 1.6mm）
    void tick(double x, double y) {
        const double h = 0.8;   // 半长
        out->push_back(ln({x - h, y - h, 0}, {x + h, y + h, 0}, model, LY_DIM, 2.0));
    }
    // 水平链：nodes 为模型 X 值，sheetY 为尺寸线纸面 Y，extY0 为界线起点纸面 Y
    void hChain(const std::vector<double>& nodes, const Transform& tf, double sheetY, double extY0) {
        if (nodes.size() < 2) return;
        std::vector<double> sx;
        for (double v : nodes) sx.push_back(tf.X(v));
        for (double x : sx) {   // 界线（超出尺寸线 2mm）
            out->push_back(ln({x, extY0, 0}, {x, sheetY + 2.0, 0}, model, LY_DIM, 0.6));
            tick(x, sheetY);
        }
        out->push_back(ln({sx.front(), sheetY, 0}, {sx.back(), sheetY, 0}, model, LY_DIM, 0.8));
        for (size_t i = 1; i < sx.size(); ++i) {   // 分格数字（居中，位于尺寸线上方）
            const double mid = 0.5 * (sx[i - 1] + sx[i]);
            const double valMm = nodes[i] - nodes[i - 1];
            out->push_back(txt({mid, sheetY + 0.8, 0}, QString::number(int(std::lround(valMm))),
                               textH, model, LY_TEXT, C_DIM));
            R->dimCount++;
        }
        R->dimChainCount++;
    }
    // 竖直链：nodes 为模型 Y 值，sheetX 为尺寸线纸面 X
    // textDir = +1 数字写在尺寸线右侧（向左延伸：用于平面图左侧的链）；
    //           -1 数字写在尺寸线左侧（用于平面图右侧的链），保证文字不越出尺寸链带。
    void vChain(const std::vector<double>& nodes, const Transform& tf, double sheetX,
                double extX0, double textDir) {
        if (nodes.size() < 2) return;
        std::vector<double> sy;
        for (double v : nodes) sy.push_back(tf.Y(v));
        for (double y : sy) {
            out->push_back(ln({extX0, y, 0}, {sheetX + 2.0, y, 0}, model, LY_DIM, 0.6));
            tick(sheetX, y);
        }
        out->push_back(ln({sheetX, sy.front(), 0}, {sheetX, sy.back(), 0}, model, LY_DIM, 0.8));
        for (size_t i = 1; i < sy.size(); ++i) {   // 分格数字（居中，位于尺寸线一侧）
            const double mid = 0.5 * (sy[i - 1] + sy[i]);
            const double valMm = nodes[i] - nodes[i - 1];
            const QString t = QString::number(int(std::lround(valMm)));
            const double tw = t.size() * textH;              // 粗估文本宽
            const double tx = textDir >= 0 ? sheetX + 1.0 : sheetX - 1.0 - tw;
            out->push_back(txt({tx, mid, 0}, t, textH, model, LY_TEXT, C_DIM));
            R->dimCount++;
        }
        R->dimChainCount++;
    }
};

// ----------------------------- 家具图块 -----------------------------
// 约定：所有图元坐标一律为【纸面 mm】；图块内部的零件尺寸（模型 mm）经 sm() 乘比例。
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
    // 双人床：床身 + 两个枕头 + 被沿折线
    void bed(double x0, double y0, double bw, double bh) {
        rect(x0, y0, x0 + bw, y0 + bh);
        const double pw = bw * 0.4, ph = bh * 0.12, headY = y0 + bh - ph - sm(60);
        rect(x0 + bw * 0.08, headY, x0 + bw * 0.08 + pw, headY + ph);
        rect(x0 + bw * 0.52, headY, x0 + bw * 0.52 + pw, headY + ph);
        line(x0, y0 + bh * 0.72, x0 + bw, y0 + bh * 0.72);
    }
    // 坐便器：水箱 + 椭圆坐圈（dirX 预留朝向）
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
    const double W = p.outerW, H = p.outerH;
    // 比例坐标(0~1) → 纸面坐标
    auto fxp = [tf, W](double v) { return tf.X(v * W); };
    auto fyp = [tf, H](double v) { return tf.Y(v * H); };
    const double s = tf.s;
    auto rw = [&](double mm) { return mm * s; };   // 模型长度 → 纸面
    const double yTop = tf.Y(H);                   // 平面图上边缘（纸面 Y）

    // 厨房：操作台 + 水盆 + 灶（两个圆）
    fb.rect(fxp(0.285), yTop - rw(650), fxp(0.495), yTop - rw(120));
    fb.ellipse(fxp(0.33), yTop - rw(385), rw(200), rw(150));
    fb.circle(fxp(0.435), yTop - rw(385), rw(140));
    fb.circle(fxp(0.465), yTop - rw(385), rw(140));

    // 餐厅：餐桌 + 4 椅
    fb.rect(fxp(0.43) - rw(800), fyp(0.63) - rw(450), fxp(0.43) + rw(800), fyp(0.63) + rw(450));
    for (double dx : { -350.0, 350.0 }) {
        fb.rect(fxp(0.43) + rw(dx) - rw(225), fyp(0.63) + rw(550), fxp(0.43) + rw(dx) + rw(225), fyp(0.63) + rw(1000));
        fb.rect(fxp(0.43) + rw(dx) - rw(225), fyp(0.63) - rw(1000), fxp(0.43) + rw(dx) + rw(225), fyp(0.63) - rw(550));
    }

    // 客厅：沙发 + 茶几 + 电视柜
    fb.rect(fxp(0.30), fyp(0.20), fxp(0.30) + rw(2200), fyp(0.20) + rw(850));
    fb.rect(fxp(0.36), fyp(0.20) + rw(1050), fxp(0.36) + rw(1200), fyp(0.20) + rw(1650));
    fb.rect(fxp(0.56) - rw(450), fyp(0.28), fxp(0.56) - rw(100), fyp(0.28) + rw(1500));

    // 次卧：双人床 + 衣柜
    fb.bed(fxp(0.03), fyp(0.28), rw(1500), rw(2000));
    fb.rect(fxp(0.05), fyp(0.49) - rw(600), fxp(0.22), fyp(0.49) - rw(100));

    // 主卧：双人床 + 衣柜
    fb.bed(fxp(0.66), fyp(0.08), rw(1800), rw(2000));
    fb.rect(fxp(0.58) + rw(80), fyp(0.05), fxp(0.58) + rw(680), fyp(0.05) + rw(1600));

    // 客卫：坐便 + 洗手盆
    fb.toilet(fxp(0.71), fyp(0.74), 1);
    fb.basin(fxp(0.615), fyp(0.635));

    // 主卫：坐便 + 洗手盆 + 淋浴房
    fb.toilet(fxp(0.90), fyp(0.53), 1);
    fb.basin(fxp(0.855), fyp(0.635));
    fb.rect(fxp(0.765), fyp(0.505), fxp(0.765) + rw(900), fyp(0.505) + rw(900));
    fb.circle(fxp(0.765) + rw(200), fyp(0.505) + rw(200), rw(80));

    // 生活阳台：洗衣机
    fb.rect(fxp(0.325), fyp(0.02), fxp(0.325) + rw(600), fyp(0.02) + rw(600));
    fb.circle(fxp(0.325) + rw(300), fyp(0.02) + rw(300), rw(210));
}

// ----------------------------- 房间名标注 -----------------------------
void buildLabels(std::vector<Element>& out, const FloorPlanParams& p,
                 const Transform& tf, const SketchModel* m, FloorPlanViewsResult& R) {
    if (!p.showLabels) return;
    const double W = p.outerW, H = p.outerH;
    struct Lbl { double fx, fy; const char* name; };
    static const Lbl lbls[] = {
        { 0.42, 0.36, "客厅" }, { 0.38, 0.655, "餐厅" }, { 0.39, 0.88, "厨房" },
        { 0.80, 0.25, "主卧" }, { 0.13, 0.35, "次卧" }, { 0.13, 0.62, "书房" },
        { 0.665, 0.72, "卫生间" }, { 0.845, 0.575, "卫生间" },
        { 0.63, 0.905, "景观阳台" }, { 0.44, 0.07, "生活阳台" }, { 0.335, 0.565, "玄关" },
    };
    for (const auto& L : lbls) {
        // Text 的 radius 字段为字高(mm)；本模板图元坐标即纸面 mm，直接用纸面字高
        out.push_back(txt(Vec3(tf.X(L.fx * W), tf.Y(L.fy * H), 0), QString::fromUtf8(L.name),
                          p.labelTextH, m, LY_TEXT));
        R.labelCount++;
    }
}

// ----------------------------- 装饰图签 -----------------------------
void buildTitleBlock(std::vector<Element>& out, const FloorPlanParams& p,
                     const SketchModel* m, FloorPlanViewsResult& R) {
    const double x0 = p.sheetW - p.margin - p.titleBlockW;
    const double x1 = p.sheetW - p.margin;
    const double y0 = p.margin, y1 = p.sheetH - p.margin;
    const double th = p.titleTextH;
    auto frame = [&](double a0, double b0, double a1, double b1, double w) {
        addRect4(out, a0, b0, a1, b1, m, LY_TITLE, w);
    };
    frame(x0, y0, x1, y1, 2.0);                       // 图签外框

    // 行表：自顶向下（高度为图签净高的比例）
    const double hh = y1 - y0;
    struct Row { double h; const char* label; const char* text; bool big; };
    const Row rows[] = {
        { 0.11, "设计单位", "", true },
        { 0.075, "建设单位", "", false },
        { 0.075, "工程名称", "", false },
        { 0.075, "工程编号", "", false },
        { 0.055, "审  核", "日 期", false },
        { 0.055, "校  对", "日 期", false },
        { 0.055, "设  计", "日 期", false },
        { 0.055, "工程负责", "日 期", false },
        { 0.075, "图纸名称", "", true },
        { 0.06,  "专业", "", false },
        { 0.03,  "备  注", "", false },
    };
    int n = sizeof(rows) / sizeof(rows[0]);
    double yTop = y1;
    int field = 0;
    const double labX = x0 + 4, txtX = x0 + p.titleBlockW * 0.34;
    for (int i = 0; i < n; ++i) {
        const double rh = rows[i].h * hh;
        const double yb = yTop - rh;
        if (i > 0)
            out.push_back(ln({x0, yb, 0}, {x1, yb, 0}, m, LY_TITLE, 1.0));
        const double ty = yb + rh * 0.5 - th * 0.4;
        QString label = QString::fromUtf8(rows[i].label);
        QString text  = QString::fromUtf8(rows[i].text);
        if (rows[i].label == QStringLiteral("设计单位")) {
            // logo 方块 + 公司名
            addRect4(out, x0 + 6, yb + rh * 0.18, x0 + 6 + rh * 0.64, yb + rh * 0.82, m, LY_TITLE, 1.4);
            out.push_back(txt({x0 + p.titleBlockW * 0.22, ty, 0}, p.company, th * 1.15, m, LY_TITLE));
            field += 1;
        } else if (rows[i].label == QStringLiteral("建设单位")) {
            out.push_back(txt({labX, ty, 0}, QStringLiteral("建设单位："), th, m, LY_TITLE));
            out.push_back(txt({txtX, ty, 0}, p.client, th, m, LY_TITLE));
            field += 2;
        } else if (rows[i].label == QStringLiteral("工程名称")) {
            out.push_back(txt({labX, ty, 0}, QStringLiteral("工程名称："), th, m, LY_TITLE));
            out.push_back(txt({txtX, ty, 0}, p.projectName, th, m, LY_TITLE));
            field += 2;
        } else if (rows[i].label == QStringLiteral("工程编号")) {
            out.push_back(txt({labX, ty, 0}, QStringLiteral("工程编号："), th, m, LY_TITLE));
            field += 1;
        } else if (rows[i].big == false && text == QStringLiteral("日 期")) {
            // 签字行：标签 | 签名格 | 日期格
            const double vx = x0 + p.titleBlockW * 0.42;
            out.push_back(ln({vx, yb, 0}, {vx, yTop, 0}, m, LY_TITLE, 0.8));
            out.push_back(ln({x0 + p.titleBlockW * 0.72, yb, 0}, {x0 + p.titleBlockW * 0.72, yTop, 0}, m, LY_TITLE, 0.8));
            out.push_back(txt({labX, ty, 0}, label, th, m, LY_TITLE));
            out.push_back(txt({x0 + p.titleBlockW * 0.76, ty, 0}, QStringLiteral("日 期"), th, m, LY_TITLE));
            field += 2;
        } else if (rows[i].label == QStringLiteral("图纸名称")) {
            out.push_back(txt({labX, ty, 0}, QStringLiteral("图纸名称"), th * 0.8, m, LY_TITLE));
            out.push_back(txt({x0 + p.titleBlockW * 0.08, yb + rh * 0.16, 0}, p.drawingName,
                              th * 1.25, m, LY_TITLE));
            field += 2;
        } else if (rows[i].label == QStringLiteral("专业")) {
            out.push_back(txt({labX, ty, 0}, QStringLiteral("专业：装饰"), th, m, LY_TITLE));
            out.push_back(txt({x0 + p.titleBlockW * 0.46, ty, 0}, QStringLiteral("图号：01"), th, m, LY_TITLE));
            out.push_back(txt({x0 + p.titleBlockW * 0.74, ty, 0},
                              QStringLiteral("比例：") + p.drawScale, th, m, LY_TITLE));
            field += 3;
        } else if (rows[i].label == QStringLiteral("备  注")) {
            out.push_back(txt({labX, ty, 0}, QStringLiteral("本图尺寸以现场放线为准"), th * 0.85, m, LY_TITLE));
            field += 1;
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
    std::vector<WallSpec> walls = defaultLayout(p);
    WallBuilder wb; wb.out = &out; wb.model = model; wb.R = &R;
    for (auto& w : walls) wb.run(w, tf);

    // ---- 4. 家具 ----
    buildFurniture(out, p, tf, model, R);

    // ---- 5. 房间标注 ----
    buildLabels(out, p, tf, model, R);

    // ---- 6. 四周两级尺寸链 ----
    if (p.showDims) {
        ChainBuilder cb; cb.out = &out; cb.model = model; cb.R = &R; cb.textH = p.dimTextH;
        const double W = p.outerW, H = p.outerH;
        const double x1 = tf.X(W), y1 = tf.Y(H), x0 = tf.X(0), y0 = tf.Y(0);
        // 底部：分格链（窗/墙位节点）+ 总尺寸链
        cb.hChain({ 0, 0.36 * W, 0.50 * W, 0.58 * W, 0.66 * W, 0.82 * W, W },
                  tf, y0 - 8.0, y0 - 1.0);
        cb.hChain({ 0, W }, tf, y0 - 18.0, y0 - 1.0);
        // 顶部
        cb.hChain({ 0, 0.28 * W, 0.50 * W, 0.55 * W, 0.70 * W, W },
                  tf, y1 + 8.0, y1 + 1.0);
        cb.hChain({ 0, W }, tf, y1 + 18.0, y1 + 1.0);
        // 左侧（文字向内，即尺寸线右侧）
        cb.vChain({ 0, 0.17 * H, 0.30 * H, 0.49 * H, 0.61 * H, 0.83 * H, H },
                  tf, x0 - 8.0, x0 - 1.0, +1.0);
        cb.vChain({ 0, H }, tf, x0 - 18.0, x0 - 1.0, +1.0);
        // 右侧（文字向内，即尺寸线左侧）
        cb.vChain({ 0, 0.28 * H, 0.49 * H, 0.66 * H, 0.83 * H, H },
                  tf, x1 + 8.0, x1 + 1.0, -1.0);
        cb.vChain({ 0, H }, tf, x1 + 18.0, x1 + 1.0, -1.0);
    }

    // ---- 7. 包围盒与落框检查（在图框/图签前计算，只覆盖平面图内容）----
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

    // ---- 8. 图框 + 图签（幅面线/图框在图框外缘，不计入内容包围盒）----
    addRect4(out, 0.0, 0.0, p.sheetW, p.sheetH, model, LY_TITLE, 1.0);              // 幅面线
    addRect4(out, p.margin, p.margin, p.sheetW - p.margin, p.sheetH - p.margin,
             model, LY_TITLE, 2.0);                                                  // 图框
    buildTitleBlock(out, p, model, R);
    return R;
}

} // namespace SketchCAD
