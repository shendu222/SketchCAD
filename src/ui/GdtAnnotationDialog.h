#pragma once
#include <QDialog>
#include <QStringList>
#include <QTabWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>

namespace SketchCAD {

class SketchModel;

// GD&T 标注面板（标准知识库消费 · 第二个功能模块）
// ---------------------------------------------------------------------------
// 设计要点：roughness / geo_tol 字段一旦输入数值，面板立即实时刷新，自动呈现
// 对应的可选标准列表（来自 roughness / geo_tol 标准表），供用户直接点选赋值，
// 无需手动切换或额外刷新。放置动作复用既有 buildRoughnessSymbol / createFcf。
class GdtAnnotationDialog : public QDialog {
    Q_OBJECT
public:
    explicit GdtAnnotationDialog(SketchModel* model, QWidget* parent = nullptr);

private slots:
    // ---- 粗糙度 ----
    void onRoughTypeChanged(int);
    void onRoughTextChanged(const QString&);
    void onRoughListClicked(QListWidgetItem* item);
    void onRoughPlace();

    // ---- 几何公差 ----
    void onGeoFeatureChanged(int);
    void onGeoValueChanged(const QString&);
    void onGeoDatumChanged(const QString&);
    void onGeoModifierChanged(QListWidgetItem* item);
    void onGeoPlace();

private:
    void refreshRoughList();   // 重新载入标准系列并高亮最近值
    void refreshGeoOptions();  // 依特征重建标准选项（类别/修饰符）并刷新预览
    void updateGeoPreview();   // 仅刷新预览行（数值/修饰符变化时）

    SketchModel* model_ = nullptr;

    // 粗糙度
    QComboBox*  cbRoughType_ = nullptr;   // Ra / Rz
    QLineEdit*  leRough_     = nullptr;   // 数值输入
    QListWidget* listRough_  = nullptr;   // 标准系列（实时高亮最近值，点选赋值）

    // 几何公差
    QComboBox*  cbGeoFeature_ = nullptr;  // 特征（geo_tol.features）
    QLineEdit*  leGeoValue_   = nullptr;  // 公差值
    QLineEdit*  leGeoDatum_   = nullptr;  // 基准（空格/逗号分隔，可选）
    QListWidget* listGeo_     = nullptr;  // 标准选项：类别 / 特征 / 可用修饰符 / 预览
    QListWidget* listMod_     = nullptr;  // 修饰符勾选（geo_tol.modifiers）
    QLabel*     lblGeoSym_    = nullptr;  // 映射到的框格符号（未收录时提示）
};

} // namespace SketchCAD
