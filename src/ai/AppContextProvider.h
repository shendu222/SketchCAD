#pragma once
#include <QObject>
#include <QString>
#include <functional>

namespace SketchCAD {

class SketchModel;

// 文档状态采集：选择集 / 图层 / 视图范围 / 最近命令。
// 真实数据源（选择集、图层）由 MainWindow 注入；无头测试可不注入（返回占位文本）。
class AppContextProvider : public QObject {
    Q_OBJECT
public:
    explicit AppContextProvider(SketchModel* model, QObject* parent = nullptr);

    void setSelectionProvider(std::function<QString()> fn) { selectionProvider_ = std::move(fn); }
    void setLayersProvider(std::function<QString()> fn)     { layersProvider_   = std::move(fn); }
    void setRecentCommandsProvider(std::function<QString()> fn) { recentProvider_ = std::move(fn); }

    QString selectionSummary() const;
    QString layersSummary() const;
    QString contextJson() const;

    void setModel(SketchModel* m) { model_ = m; }   // 多文档切换时重绑活动模型

private:
    SketchModel* model_;
    std::function<QString()> selectionProvider_;
    std::function<QString()> layersProvider_;
    std::function<QString()> recentProvider_;
};

} // namespace SketchCAD
