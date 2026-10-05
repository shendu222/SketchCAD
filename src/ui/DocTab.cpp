#include "ui/DocTab.h"
#include "core/SketchModel.h"
#include <QFileInfo>

namespace SketchCAD {

// ---- DocSession -----------------------------------------------------------

DocSession::DocSession(QObject* parent) : QObject(parent) {
    // 每会话一份独立模型（撤销栈/选择集/UCS/活动工具随之独立）。
    model_ = new SketchModel(this);
    // 脏标记自动跟踪：任何模型变化都置脏（读档/新建的瞬态改动由外部 markClean 复位）。
    connect(model_, &SketchModel::modelChanged, this, [this] { setDirty(true); });
}

QString DocSession::title() const {
    const QString base = filePath.isEmpty()
                             ? QStringLiteral("绘图%1").arg(untitledIndex)
                             : QFileInfo(filePath).completeBaseName();
    return dirty_ ? base + QStringLiteral(" *") : base;
}

void DocSession::setDirty(bool d) {
    if (dirty_ == d) return;   // 幂等：不变不发信号
    dirty_ = d;
    emit dirtyChanged(this);
}

// ---- DocManager -----------------------------------------------------------

DocManager::DocManager(QObject* parent) : QObject(parent) {}

DocManager::~DocManager() { qDeleteAll(docs_); }

int DocManager::addDoc(DocSession* s) {
    if (!s) return -1;
    if (s->parent() == nullptr) s->setParent(this);   // 生命周期归管理器
    connect(s, &DocSession::dirtyChanged, this, [this](DocSession* ss) {
        emit dirtyChanged(docs_.indexOf(ss));   // 会话脏 → 下标脏
    });
    docs_.append(s);
    if (active_ < 0) active_ = 0;   // 首张自动成为活动
    emit docAdded(docs_.size() - 1);
    return docs_.size() - 1;
}

DocSession* DocManager::addNewDoc() {
    auto* s = new DocSession(this);
    s->untitledIndex = ++untitledSeq_;
    addDoc(s);
    return s;
}

void DocManager::setActive(int i) {
    if (i < 0 || i >= docs_.size() || i == active_) return;
    active_ = i;
    emit activeChanged(i);
}

int DocManager::cycle(int step) const {
    const int n = docs_.size();
    if (n <= 1) return active_;
    return ((active_ + step) % n + n) % n;   // 双向循环，负步长同样成立
}

void DocManager::reorder(int from, int to) {
    if (from < 0 || from >= docs_.size() || to < 0 || to >= docs_.size() || from == to)
        return;
    docs_.move(from, to);
    // 活动身份跟随会话，不随下标漂移（QList::move 的下标重映射）
    if (active_ == from)             active_ = to;
    else if (from < active_ && to >= active_) --active_;
    else if (from > active_ && to <= active_) ++active_;
    emit orderChanged();
}

void DocManager::removeDoc(int i) {
    if (i < 0 || i >= docs_.size()) return;
    DocSession* s = docs_.takeAt(i);
    const bool wasActive = (i == active_);
    delete s;   // 调用方必须已先销毁该会话的视图页面（视图引用模型，须先于模型析构）
    if (wasActive) {
        // 关闭的是活动文档 → 夹取到邻居（下一张优先，末张则前一张）
        active_ = docs_.isEmpty() ? -1 : qMin(i, docs_.size() - 1);
    } else if (i < active_) {
        --active_;   // 关闭的文档在活动之前 → 活动下标随删除左移（与标签条重排一致）
    }
    emit docRemoved(i);
    if (wasActive) emit activeChanged(active_);
}

bool DocManager::isDirty(int i) const {
    const DocSession* s = doc(i);
    return s && s->isDirty();
}

void DocManager::markClean(int i) {
    if (DocSession* s = doc(i)) s->markClean();
}

void DocManager::markDirty(int i) {
    if (DocSession* s = doc(i)) s->markDirty();
}

} // namespace SketchCAD
