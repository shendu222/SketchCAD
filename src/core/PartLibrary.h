#pragma once
// P4（Ch13 机械零件设计）：零件库抽象
// ------------------------------------------------------------
// 把 FlangeViews / AircraftViews 的「参数对话框 → Elements」模式收敛为统一接口：
// 具体零件（齿轮/法兰/轴承…）实现 PartGenerator，由 PartLibrary 按 id 注册与出库。
// P5 将在此框架下扩展 GB 标准件 JSON 规格表驱动出库（checkout(partId, params) → Elements）。
#include <QString>
#include <QVariantMap>
#include <QVector>
#include <QMap>
#include <memory>
#include <vector>
#include "Element.h"

namespace SketchCAD {

struct BlockDef;   // 出库打包为块定义（checkout）—— 完整定义见 Block.h

// 零件生成器抽象基类：由参数表生成几何（不负责标注/图层策略，交由调用方）。
class PartGenerator {
public:
    virtual ~PartGenerator() = default;
    virtual QString id() const = 0;                 // 零件类型 id（如 "gear" / "bolt"）
    virtual QString displayName() const = 0;        // 展示名（面板/日志）
    // 由参数表生成几何；成功返回 true 并填充 out（图元集合）。
    // note（可空）：规格缺项回落默认等提示信息（调用方记日志）。
    virtual bool generate(const QVariantMap& params, std::vector<Element>& out, QString* note = nullptr) const = 0;
};

// 零件库：进程级单例，按 id 注册/查找/出库。
class PartLibrary {
public:
    static PartLibrary& instance();
    void registerGenerator(std::shared_ptr<PartGenerator> g);
    const PartGenerator* find(const QString& id) const;
    // 生成几何（不打包为块）。
    bool generate(const QString& id, const QVariantMap& params, std::vector<Element>& out, QString* note = nullptr) const;
    // 出库：参数化生成几何 + 属性 → 打包为 BlockDef（实例化引用式块），符合 IO_SPEC。
    // 成功返回 true，defOut 为可直接插入模型的块定义（含标准/公称/名称属性）。
    bool checkout(const QString& id, const QVariantMap& params, BlockDef& defOut, QString* note = nullptr) const;
    QVector<QString> ids() const;
private:
    PartLibrary();                                  // 构造时注册内建零件（齿轮/紧固件…）
    QMap<QString, std::shared_ptr<PartGenerator>> gens_;
};

} // namespace SketchCAD
