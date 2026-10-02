#pragma once
#include <vector>
#include "Element.h"
#include "Vec.h"

namespace SketchCAD {

class SketchModel;

// 前向声明：倒角/倒圆核心层结果（定义在 core/ChamferFillet.h）。
// 这里只引用，避免 Modify.h 直接包含整个几何实现。
namespace corner { struct CornerResult; }

namespace modify {

// ============================================================
// 图元修改引擎
// ------------------------------------------------------------
// 把"对一组图元做几何变换 / 编辑"这一类操作收在核心层。
// 与 trim 引擎同构：传入目标图元 + 参数，返回"删除 / 新增"的图元 ID 列表，
// 调用方（CLI / 菜单 / 工具）负责把这些 ID 写入模型并打日志。
//
// 所有算法都遵循：
//   * 不触碰 SketchModel，调用方按结果落盘（Undo 一致性）
//   * 修改完成后立刻可走 undo：调用方在改动前后 pushMemento("...")
//   * 输入是 const Element 引用 + 参数；输出仅是几何数据，样式/图层从源继承
//   * 多选场景下用单个操作包装所有目标，避免一次操作产生几十条撤销记录
// ============================================================

// 一组"动作描述"：操作名 + 目标 ID + 改动参数 + 计算结果。
// 由 ModifyCommand 在落盘前统一压一次撤销快照。
struct Change {
    int  id       = -1;          // 源图元 ID（无则表示"纯新增"）
    Element replacement;         // 替换 / 新增图元（id < 0 表示删除）
    bool remove   = false;       // 仅删除（不替换）
    QString note;                // 日志后缀（"旋转 90°" / "镜像 Y" / ...）
};

// ---------- 1. 删除 ----------
// 直接根据 ID 列表生成"全删除"动作。空列表返回空集合。
std::vector<Change> makeDelete(const std::vector<int>& ids);

// ---------- 2. 复制（Duplicate）----------
// 复制当前选中的图元，偏置 (dx, dy)（默认 (10, 0) mm），
// 重置 ID 为 -1 表示"新增"，原图元不变。
std::vector<Change> makeDuplicate(const std::vector<Element>& src, double dx = 10.0, double dy = 0.0);

// ---------- 3. 移动 / 复制粘贴式 Move-Copy ----------
// mode: false=移动（替换原图元），true=复制（新增）
std::vector<Change> makeMoveCopy(const std::vector<Element>& src,
                                 const Vec3& delta, bool copy);

// ---------- 4. 旋转 ----------
// 绕 center 旋转 angleDeg 度（顺时针为负、CCW 为正）。
std::vector<Change> makeRotate(const std::vector<Element>& src,
                               const Vec3& center, double angleDeg);

// ---------- 5. 缩放 ----------
// 绕 center 缩放 factor 倍（factor > 0，factor=1 等价恒等）。
// 半轴类图元（圆/弧 radius、椭圆 radius/radiusY）按 factor 倍数同步。
std::vector<Change> makeScale(const std::vector<Element>& src,
                              const Vec3& center, double factor);

// ---------- 6. 镜像（严格对称：点对称 / 轴对称）----------
//   isPointMirror == true  : 关于 base1 点中心对称（等价绕 base1 旋转 180°）；base2 忽略。
//   isPointMirror == false : 关于直线 base1→base2 轴对称。
// 行为：保留源图元，新增一份镜像副本（replacement.id = -1，由模型分配新 ID）；
//       除几何被严格反射外，图层 / 颜色 / 线宽 / 线型 / 文本等属性全部继承源图元。
// 角度处理：圆弧起止角、椭圆长轴角、剖面线方向角按反射公式 2β−θ 修正（β 为镜像轴世界角）；
//           点对称按「旋转 180°」处理（角度 +180°）。半径 / 半轴 / 字高保距不变。
std::vector<Change> makeMirror(const std::vector<Element>& src,
                               bool isPointMirror,
                               const Vec3& base1,
                               const Vec3& base2 = Vec3(1, 0, 0));

// ---------- 7. 反向 (Revert Direction) ----------
// 直线：交换 pts[0] / pts[1]；圆弧：startAngle / endAngle 互换 + 重新归一化到 (-180,180]
std::vector<Change> makeRevertDirection(const std::vector<Element>& src);

// ---------- 8. 延长 / 缩短 ----------
// 对每条直线：起点为 pts[0]，终点为 pts[1]；side 0=起点, 1=终点，delta 为延长量(mm，正=延长)
std::vector<Change> makeLengthen(const std::vector<Element>& src, int side, double delta);

// ---------- 9. 偏移 (Offset) ----------
// 距离 dist(mm)；方向：正值=沿线段(pts[0]→pts[1])左侧；dist=0 自动选正向一侧。
// 仅实现直线 / 圆 / 圆弧的偏移；其它图元跳过。
std::vector<Change> makeOffset(const std::vector<Element>& src, double dist);

// ---------- 10. 打断 / 等分 ----------
// kind=0: 在 cut 点（投影到线段上）打断为两条直线
// kind=1: 把线段 n 等分（产生 n+1 个端点、n 条新线段）
std::vector<Change> makeBreakDivide(const std::vector<Element>& src, int id,
                                    const Vec3& cut, int n = 2);

// ---------- 11. 圆角 / 倒角（Fillet / Bevel） ----------
// 仅作用于两条直线；radius > 0 = 圆角，radius = 0 = 倒角(交点)。
// 返回值：原 id1 / id2 标记删除 + 3 个新增（圆弧 + 两条裁短直线）。
std::vector<Change> makeFillet(int id1, int id2, double radius,
                               const std::vector<Element>& pool);

// ---------- 11b. 倒角 / 倒圆（基于 corner 核心层）----------
// 把 corner::CornerResult（裁短两线 + 倒角直线 / 倒圆圆弧）转换为 Change 列表：
//   * 删除原 id1 / id2；
//   * 新增 2 条裁短线（lineA/lineB，保留端不变、近端裁到裁断点）+ 1 条倒角/圆角段（corner）。
// 样式/图层/线宽从源图元继承（由 corner 层在完成几何的同时复制，这里只改 id 落盘）。
// res.ok == false 时返回空集合（调用方据此提示 res.msg）。
std::vector<Change> makeCornerChanges(int id1, int id2, const corner::CornerResult& res);

// ---------- 12. 分解 (Explode) ----------
// 把矩形/多边形/折线/样条拆成单段直线 / 单段圆弧。
std::vector<Change> makeExplode(const std::vector<Element>& src);

// ---------- 13. 对齐 (Align) ----------
// 把 src 中所有图元的「关键控制点」对齐到 ref 的同一关键控制点。
// kind: 0=左对齐（minX）, 1=中(水平, centerX), 2=右对齐（maxX）,
//       3=顶对齐（maxY）, 4=中(垂直, centerY), 5=底对齐（minY）
std::vector<Change> makeAlign(const std::vector<Element>& src,
                              const Element& ref, int kind);

// ---------- 14. 拉伸 (Stretch) ----------
// 仅移动 src 中"包含 basePt 端点"的图元的那一个端点到 toPoint。
// 其它图元不动；空 src 或 basePt 不在任何图元端点容差内 → 空变更。
std::vector<Change> makeStretch(const std::vector<Element>& src,
                                const Vec3& basePt, const Vec3& toPoint,
                                double tol = 1e-3);

// ---------- 15. 矩形阵列 (Rectangular Array) ----------
// 把 src 中每个图元在 (rows × cols) 的矩形网格里复制。
//   * 原图元不动；仅新增副本（id=-1）。副本 (r,c) 相对原图元平移
//     (c·colSpacing, r·rowSpacing)，r∈[0,rows) 行向上(+Y)、c∈[0,cols) 列向右(+X)。
//   * (r,c)==(0,0) 即原图元位置，跳过不生成（避免与原件重合的冗余副本）。
//   * 因此新增副本数 = rows·cols − 1（rows==cols==1 时为空）。
//   * 副本完整继承原图元的全部属性（图层/颜色/线宽/线型/文本…），只改几何平移。
std::vector<Change> makeRectangularArray(const std::vector<Element>& src,
                                         int rows, int cols,
                                         double rowSpacing, double colSpacing);

// ---------- 16. 环形阵列 (Polar Array) ----------
// 把 src 中每个图元绕 center 旋转复制 count 份，均匀分布在 angleRangeDeg 范围内。
//   * 原图元不动；仅新增副本（id=-1）。
//   * 第 k 份(k=1..count-1) 旋转角 = k·angleRangeDeg / count（k=0 即原图元位置，跳过）。
//     例：count=8、angleRangeDeg=360 → 副本在 45°、90°、…、315°（八等分整圆，无重合）。
//   * 圆弧(Arc) 起止角随旋转同步 +angle；椭圆(Ellipse) 长轴方向 rotation 同步 +angle；
//     圆/文本的几何随 pts 旋转，半径/字高不变。
//   * 副本完整继承原图元属性，只改几何。
std::vector<Change> makePolarArray(const std::vector<Element>& src,
                                   const Vec3& center, int count,
                                   double angleRangeDeg);

// ---------- 17. 路径阵列 (Path Array) ----------
// 把 src 中每个图元沿已离散的路径点列等距复制。
//   * pathPts：路径的密集采样点列（Polyline 用 geo::samplePolyline、Spline 用
//     geo::sampleSpline 离散；由调用方翻译，本函数只认点列——保持 Modify 层
//     「输入是几何数据、不解析图元语义」的边界）。
//   * spacing > 0  → Measure 语义：从路径起点起每 spacing 放一个项目；
//                    路径长不足 spacing 时放不下第二个项目 → 返回空（退化）。
//   * spacing <= 0 且 count >= 2 → Divide 语义：count 个项目均分全长
//                    （步长 = L/(count-1)，与 AutoCAD「项目数」方式一致）。
//   * 两者都不满足（路径退化/参数非法）→ 返回空。
//   * 位置 P0（路径起点）即原图元位置 → 跳过（与矩形/环形阵列同范式）；
//     其余位置生成平移副本（id=-1，属性完整继承）。
//   * 仅平移、不旋转（AutoCAD 的「对齐项目」切线旋转不在此实现，见设计文档）。
std::vector<Change> makePathArray(const std::vector<Element>& src,
                                  const std::vector<Vec3>& pathPts,
                                  double spacing, int count = 0);

// ---------- 18. 连合 (Join) ----------
// JOIN 端点容差的命令行/无头默认值（mm）。
// GUI 菜单入口应传视图捕捉容差 worldTol()（= 8px / 缩放），与 OSNAP 拾取手感一致；
// 命令行/脚本/自测没有视图缩放概念，用该固定值（约等于 1:1 视图下的捕捉容差量级）。
inline constexpr double kJoinDefaultTol = 0.5;

// 把 src 中「同层、端点在 tol 内首尾相接」的 Line/Arc 链式连合为开放 Polyline。
//   * 只认 Line / Arc，且只连合同一图层的图元（跨层不连）。
//   * 端点重合判定统一走 geo::almostEqual(p, q, tol)。
//   * Arc 参与连合时按弧长离散为折线顶点（每段 ≥8 点、步长 ≤2mm）并入——
//     Polyline 的弧段存储是「三相邻点外接圆」语义，无法无损承载任意独立弧；
//     离散保证 explode 可逆（全产出直线段）且形状等价（偏差 ≪ 视觉分辨极限）。
//   * 链内 ≥2 个图元才产出：删除链内全部源图元 + 新增 1 条 Polyline
//     （id=-1，继承链首图元属性；接缝处直接复用前一节点坐标，消除容差内缝隙）。
//   * 孤立图元（无邻居）与其它类型图元保留不动（不产 Change）。
//   * 无任何可连合链 → 返回空。
std::vector<Change> makeJoin(const std::vector<Element>& src, double tol);

// JOIN 内部：弧离散采样点数（弧长每 ~2mm 一点，夹在 [8, 64]）。
int joinArcSampleCount(double arcLenDeg, double radius);

// ============================================================
// 把 Change 列表写入 SketchModel 的便捷封装。
// 由命令 / 菜单统一调用，避免重复写"pushMemento → remove → add"三件套。
// ------------------------------------------------------------
//  1) 若有任何 remove 动作或 replacement.id<0，按理应先一次性备份；
//  2) 把 mark 为 remove 的 id 删除，把 replacement.id<0 的部分 addElement，
//     addElement 内部会重新分配正 ID；
//  3) 复用 id 的部分走 updateElement，保留图元 ID 不变（仅改几何），
//     这样属性面板/BOM 选中态不会跳。
//
// 返回新增的图元 ID 列表（供"复制"等场景记录）。
// ============================================================
std::vector<int> applyChanges(SketchModel* model, const std::vector<Change>& changes,
                              const QString& undoLabel);

} // namespace modify
} // namespace SketchCAD