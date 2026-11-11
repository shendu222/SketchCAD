#pragma once
#include <QDialog>
#include <QStringList>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include "core/weld/WeldSymbol.h"   // WeldSpec / WeldType 等（对话框返回值类型须完整定义）

namespace SketchCAD {

class SketchModel;
class CommandInterpreter;

// 焊缝符号可视化编辑对话框（GB/T 324-2008 焊缝符号表示法）
// ---------------------------------------------------------------------------
// 设计要点：
//   * 类型 / 辅助符号 / 补充符号 / 箭头侧 下拉 + 尺寸(k,s,l,e,n) 输入，右侧实时预览
//     （WeldPreview 直接用 QPainter 绘制 buildWeldSymbol 输出的既有图元组合）。
//   * 确认时构造 WeldSpec → weldCommandString → CommandInterpreter::execute("WELD ...")，
//     由 WELD 命令落盘（interp_ 不可用时回落 model_->addElementsQuiet）。
//   * 不新增 ElementType：预览与落盘都复用 Line/Polyline/Circle/Text。
class WeldSymbolDialog : public QDialog {
    Q_OBJECT
public:
    explicit WeldSymbolDialog(SketchModel* model,
                              CommandInterpreter* interp = nullptr,
                              QWidget* parent = nullptr);

private slots:
    void onTypeChanged(int);
    void onAuxChanged(int);
    void onSuppChanged(int);
    void onSideChanged(int);
    void onParamChanged(const QString&);
    void onAccepted();

private:
    WeldSpec currentSpec() const;
    void rebuildPreview();

    SketchModel*       model_  = nullptr;
    CommandInterpreter* interp_ = nullptr;

    QComboBox* cbType_ = nullptr;
    QComboBox* cbAux_  = nullptr;
    QComboBox* cbSupp_ = nullptr;
    QComboBox* cbSide_ = nullptr;
    QLineEdit* leK_ = nullptr;   // 焊脚 k
    QLineEdit* leS_ = nullptr;   // 坡口深 s
    QLineEdit* leL_ = nullptr;   // 长度 l
    QLineEdit* leE_ = nullptr;   // 间距 e
    QLineEdit* leN_ = nullptr;   // 段数 n
    class WeldPreview* preview_ = nullptr;
};

} // namespace SketchCAD
