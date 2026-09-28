#pragma once
#include <QDialog>

class QListWidget;
class QListWidgetItem;
class QPushButton;
class QLabel;

namespace SketchCAD {

class SketchModel;

// 边界区域（REGION）管理对话框。
// 设计要点：REGION 不新增图元类型，而是「共享同一 name 的若干 Boundary 图元」构成的
// 命名分组——这样复用 Boundary 的全部既有字段（外环/内孔/角度/间距/材料/豁免），
// 无需引入新的数据结构。本对话框提供：
//   * 列出当前图面所有区域（按 name 分组；无 name 的边界归入「未命名边界」）；
//   * 新建区域：把当前选中的 Boundary 标记为同一 name（即归组为一个区域）；
//   * 填充区域：对该区域全部 Boundary 直接调用剖面填充（跳过自动边界追踪）；
//   * 全选区域：选中该区域全部 Boundary（便于整体平移/缩放/删除）；
//   * 重命名 / 删除区域。
// 复杂腔体只需「画一圈外框、再画几圈内孔」得到若干 Boundary，归为同一区域后一次填充即可，
// 无需人工逐段描边，也无需依赖剖面引擎的自动边界追踪。
class RegionManagerDialog : public QDialog {
    Q_OBJECT
public:
    explicit RegionManagerDialog(SketchModel* model, QWidget* parent = nullptr);

private slots:
    void refresh();
    void onNewRegion();
    void onFillRegion();
    void onSelectRegion();
    void onRenameRegion();
    void onDeleteRegion();

private:
    // 当前选中列表项对应的区域名（空字符串 = 未命名边界组）
    QString currentRegionName() const;
    // 该区域名对应的 Boundary id 列表
    std::vector<int> memberIds(const QString& name) const;

    SketchModel* model_ = nullptr;
    QListWidget* list_  = nullptr;
    QLabel*     info_   = nullptr;
    QPushButton* btnNew_    = nullptr;
    QPushButton* btnFill_   = nullptr;
    QPushButton* btnSelect_ = nullptr;
    QPushButton* btnRename_ = nullptr;
    QPushButton* btnDelete_ = nullptr;
};

} // namespace SketchCAD
