#include "ai/AppContextProvider.h"

#include "core/SketchModel.h"
#include <QJsonObject>
#include <QJsonDocument>

namespace SketchCAD {

AppContextProvider::AppContextProvider(SketchModel* model, QObject* parent)
    : QObject(parent), model_(model) {}

QString AppContextProvider::selectionSummary() const {
    if (selectionProvider_) {
        const QString s = selectionProvider_();
        if (!s.isEmpty()) return s;
    }
    return QStringLiteral("（无选择集）");
}

QString AppContextProvider::layersSummary() const {
    if (layersProvider_) {
        const QString s = layersProvider_();
        if (!s.isEmpty()) return s;
    }
    return QStringLiteral("（无图层信息）");
}

QString AppContextProvider::contextJson() const {
    QJsonObject o;
    o["elementCount"] = model_ ? model_->elementCount() : 0;
    o["selection"] = selectionSummary();
    o["layers"] = layersSummary();
    o["recentCommands"] = recentProvider_ ? recentProvider_() : QStringLiteral("");
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

} // namespace SketchCAD
