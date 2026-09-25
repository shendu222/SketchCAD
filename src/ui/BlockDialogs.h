#pragma once
#include <QDialog>
#include "core/Vec.h"

class QListWidget;
class QGraphicsScene;
class QGraphicsView;
class QLabel;
class QDoubleSpinBox;
class QLineEdit;
class QRadioButton;
class QPushButton;
class QComboBox;

namespace SketchCAD {

class SketchModel;   // 前向声明必须位于 SketchCAD 命名空间内，与实际定义一致

// ============================================================================
// 插入块对话框（"插入块"选择对话框）
// ----------------------------------------------------------------------------
// 布局：左 = 块名列表（来自 model_->blockTable()），右 = 块几何实时预览，
//       下 = 插入参数（插入点 X/Y、旋转角、X/Y 缩放）。
// 确认后：model_->insertBlock(...) 生成一个块引用并选中它，外部可借 insertedId()
// 取回新引用 id（用于联动选中 / 后续属性编辑）。
// 预览使用与正式视图相同的 ElementItem 渲染路径（含嵌套块展开、剖面、标注），
// 所见即所得，与画布渲染完全一致。
// ============================================================================
class InsertBlockDialog : public QDialog {
    Q_OBJECT
public:
    explicit InsertBlockDialog(SketchModel* model, QWidget* parent = nullptr);

    int insertedId() const { return insertedId_; }

public slots:
    void accept() override;

protected:
    void resizeEvent(QResizeEvent* e) override;

private:
    void refreshList();                 // 用当前块表重建左侧列表
    void updatePreview();               // 列表选择变化时刷新右侧预览
    void fitPreview();                  // 把内容适配到预览视口（保持比例）

    SketchModel* model_ = nullptr;
    QListWidget*  list_ = nullptr;
    QGraphicsScene* scene_ = nullptr;
    QGraphicsView*  view_ = nullptr;
    QDoubleSpinBox* sbX_  = nullptr;
    QDoubleSpinBox* sbY_  = nullptr;
    QDoubleSpinBox* sbRot_ = nullptr;
    QDoubleSpinBox* sbSx_ = nullptr;
    QDoubleSpinBox* sbSy_ = nullptr;
    int insertedId_ = -1;
};

// ============================================================================
// 创建块对话框
// ----------------------------------------------------------------------------
// 把当前选择集收进一个新块定义：块名 + 基点 + 是否删除原图元。
// 需要非空选择集（model_->selection()）；重名将失败（由 model 层拦截并报错）。
// ============================================================================
class CreateBlockDialog : public QDialog {
    Q_OBJECT
public:
    explicit CreateBlockDialog(SketchModel* model, QWidget* parent = nullptr);

    QString blockName() const;
    Vec3    basePoint() const;
    bool    deleteSource() const;       // true = 删除原图元并放置块引用（BLOCK 语义）

public slots:
    void accept() override;

private:
    SketchModel* model_ = nullptr;
    QLineEdit*     nameEdit_ = nullptr;
    QDoubleSpinBox* sbX_ = nullptr;
    QDoubleSpinBox* sbY_ = nullptr;
    QRadioButton*  deleteRadio_ = nullptr;  // 删除原图元 + 放置引用
    QRadioButton*  keepRadio_   = nullptr;  // 仅建定义，保留原图元
};

// ============================================================================
// 附加外部参照对话框
// ----------------------------------------------------------------------------
// 对应 AutoCAD 的「附加参照」（XATTACH）：指定参照图形文件 + 路径类型
// （完整路径 / 相对路径 / 无路径）。插入点、缩放比例与旋转一律取默认值
// （原点 / 1.0 / 0°），因此确认后立即在原点以原尺寸放一个参照实例。
// 失败原因（文件不存在、格式不支持、已附加、块名被普通块占用）由模型层写日志，
// 此处只做统一的提示，不重复判定。
// ============================================================================
class AttachXrefDialog : public QDialog {
    Q_OBJECT
public:
    explicit AttachXrefDialog(SketchModel* model, QWidget* parent = nullptr);

    int attachedId() const { return attachedId_; }   // 成功为参照实例 id，失败为 -1

public slots:
    void accept() override;

private slots:
    void onBrowse();                    // 浏览选择参照图形文件
    void onInputChanged();              // 路径/类型变化 → 刷新提示信息

private:
    SketchModel* model_ = nullptr;
    QLineEdit*   pathEdit_  = nullptr;
    QComboBox*   typeCombo_ = nullptr;
    QLabel*      infoLabel_ = nullptr;
    int attachedId_ = -1;
};

// ============================================================================
// 块管理器对话框
// ----------------------------------------------------------------------------
// 集中管理已有块：列表 + 重命名 / 删除 / 用当前选择集重定义（redefine）。
// 覆盖命令行 block rename / delete / redefine / list / info 的可视化入口。
// ============================================================================
class BlockManagerDialog : public QDialog {
    Q_OBJECT
public:
    explicit BlockManagerDialog(SketchModel* model, QWidget* parent = nullptr);

private slots:
    void onSelectionChanged();          // 列表选中变化 -> 刷新信息区
    void onRename();                    // 重命名当前块
    void onDelete();                    // 删除当前块定义
    void onRedefine();                  // 用当前选择集重定义当前块
    void onRefresh();                   // 重新载入块表

private:
    void rebuildList();
    QString currentName() const;

    SketchModel* model_ = nullptr;
    QListWidget*  list_ = nullptr;
    QLabel*       infoLabel_ = nullptr;
    QPushButton*  renameBtn_ = nullptr;
    QPushButton*  deleteBtn_ = nullptr;
    QPushButton*  redefineBtn_ = nullptr;
};

} // namespace SketchCAD
