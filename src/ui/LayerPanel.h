#pragma once
#include <QWidget>
#include <QString>
#include <QListWidget>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>

namespace SketchCAD {

class SketchModel;
class Layer;

// 图层管理面板：列出图纸全部图层，支持
//   * 新建 / 删除 / 重命名图层；
//   * 切换图层显示(可见)与锁定状态；
//   * 编辑图层默认样式（颜色 / 线型 / 线宽）并"批量套用到本层图元"；
//   * 把当前选择集的图元一次性移入指定图层。
// 面板只经由 SketchModel 的图层 API 操作数据，所有"批量改属性 / 显示控制"都只作用于
// 本层图元，天然满足"仅影响该图层、其他图层互不干扰"。
class LayerPanel : public QWidget {
    Q_OBJECT
public:
    explicit LayerPanel(SketchModel* model, QWidget* parent = nullptr);

    // 依据模型重建列表与样式控件（图层表变化 / 图元变化 / 选择变化时调用）
    void refresh();

private slots:
    void onCurrentRowChanged(int row);
    void onCurrentLayerChanged(const QString& name);   // 活动层切换 → 刷新"●当前"标记
    void onVisibleToggled(const QString& layer, int state);
    void onLockToggled(const QString& layer, int state);
    void onColorClicked();
    void onLineStyleChanged(int idx);
    void onLineWidthChanged(int idx);
    void onApplyStyle();
    void onNewLayer();
    void onDeleteLayer();
    void onRenameLayer();
    void onAssignSelection();

private:
    SketchModel* model_;
    QListWidget*  list_;
    QPushButton*  colorBtn_;
    QComboBox*    lineStyleCombo_;
    QComboBox*    lineWidthCombo_;
    QPushButton*  applyStyleBtn_;
    QPushButton*  newBtn_;
    QPushButton*  delBtn_;
    QPushButton*  renameBtn_;
    QPushButton*  assignBtn_;
    QLabel*       hintLabel_;

    QString currentName_;   // 当前选中的图层名（refresh 后用于恢复选择）

    QString selectedLayer() const;          // 当前列表选中的图层名
    void    updateControls();               // 用选中图层的样式填充控件
    QWidget* makeRowWidget(const Layer& l); // 列表行内联控件（可见/锁定/色块/名称）
};

} // namespace SketchCAD
