#pragma once
#include <QDialog>
#include <QString>
#include <vector>
#include "core/Element.h"   // Element（collectEntities 返回 std::vector<Element>）

class QGraphicsScene;
class QGraphicsView;
class QLabel;
class QPushButton;

namespace SketchCAD {

class SketchModel;

// 块在位编辑器（BEDIT / BEEDIT）—— 在独立临时场景中编辑某个块定义的"块内图元"。
//
// 设计要点（与主视图解耦、不落盘语义）：
//   * 进入时经 SketchModel::getBlockEntities() 取出块内图元的**副本**放进本对话框的场景，
//     全程不碰主模型（引用式模型下改的是"一份定义"，保存时才写回）。
//   * 场景复用与正式视图相同的 ElementItem 渲染路径；可拖动（ItemIsMovable）+ 可选中，
//     用户直接拖动图元即修改块内几何。
//   * 「确定」→ 收集场景中各 ElementItem 的 el（叠加其 pos 偏移）写回
//     SketchModel::redefineBlockEntities()，此时所有引用经 expandBlockRef 实时同步刷新。
//   * 「取消」/ Esc → 直接丢弃，绝不调用写回 API，块定义与主模型均不变（不落盘、可重开）。
//   * attdefs / dynParams / basePoint 等定义字段由 redefineBlockEntities 保留，
//     与 dynset 动态块参数并存。
class BlockEditorDialog : public QDialog {
    Q_OBJECT
public:
    BlockEditorDialog(SketchModel* model, const QString& blockName,
                      QWidget* parent = nullptr);

private slots:
    void onAddLine();
    void onAddCircle();
    void onDeleteSelected();
    void onFit();

private:
    void loadFromModel();
    // 收集场景中的所有块内图元（含拖动偏移），id 归零
    std::vector<Element> collectEntities() const;

    SketchModel*    model_;
    QString        blockName_;
    QGraphicsScene* scene_ = nullptr;
    QGraphicsView*  view_  = nullptr;
    QLabel*         info_  = nullptr;
    QPushButton*    delBtn_ = nullptr;
};

} // namespace SketchCAD
