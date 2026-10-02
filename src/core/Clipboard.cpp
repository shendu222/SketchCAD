// 内部剪贴板实现 —— 详见 Clipboard.h 设计说明。
//
// 序列化只复用 elementToJson / blockDefToJson 两套既有实现（与存档同源），
// 本文件自身不出现任何字段级读写——字段增减只改 Element.cpp / Block.cpp 一处。
#include "Clipboard.h"
#include "Modify.h"
#include "SketchModel.h"

#include <QClipboard>
#include <QMimeData>
#include <QGuiApplication>
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QMap>

namespace SketchCAD {
namespace clipboard {

namespace {
// 无 GUI 会话（QCoreApplication 纯控制台环境，如无头自测）下 QClipboard 平台层
// 不可用——clipboard() 会返回一个非空、但 mimeData() 即段错误的壳对象。
// 统一门禁：应用实例不是 QGuiApplication → 视为剪贴板不可用（安全返回 false）。
bool clipboardAvailable() {
    return qobject_cast<QGuiApplication*>(QCoreApplication::instance()) != nullptr;
}
} // namespace

// ---- JSON 基点小工具（本文件唯一的字段级读写，收敛于此）----
static QJsonObject baseToJson(const Vec3& p) {
    QJsonObject o;
    o["x"] = p.x; o["y"] = p.y;
    return o;
}
static Vec3 baseFromJson(const QJsonObject& o) {
    return Vec3(o.value("x").toDouble(0.0), o.value("y").toDouble(0.0), 0.0);
}

QJsonObject toJson(const Payload& p) {
    QJsonObject o;
    o["v"] = kVersion;
    o["base"] = baseToJson(p.base);
    QJsonArray els;
    for (const auto& e : p.elements) els.append(elementToJson(e));
    o["elements"] = els;
    QJsonArray blks;
    for (const auto& d : p.blocks) blks.append(block::blockDefToJson(d));
    o["blocks"] = blks;
    return o;
}

bool fromJson(const QJsonObject& o, Payload* out, QString* err) {
    if (!out) return false;
    auto fail = [&](const QString& m) { if (err) *err = m; return false; };
    if (o.isEmpty())          return fail(QStringLiteral("剪贴板内容为空"));
    if (!o.contains("v"))     return fail(QStringLiteral("缺少版本号（非本应用内容）"));
    const int v = o.value("v").toInt(-1);
    if (v != kVersion)        return fail(QStringLiteral("版本不支持（v=%1，当前 v=%2）").arg(v).arg(kVersion));

    Payload p;
    p.base = baseFromJson(o.value("base").toObject());

    const QJsonArray els = o.value("elements").toArray();
    p.elements.reserve(els.size());
    for (const auto& ev : els) p.elements.push_back(elementFromJson(ev.toObject()));

    const QJsonArray blks = o.value("blocks").toArray();
    p.blocks.reserve(blks.size());
    for (const auto& bv : blks) p.blocks.push_back(block::blockDefFromJson(bv.toObject()));

    *out = std::move(p);
    return true;
}

void translate(Payload& p, const Vec3& delta) {
    for (auto& e : p.elements)
        for (auto& pt : e.pts) pt = pt + delta;
    p.base = p.base + delta;
}

void translateTo(Payload& p, const Vec3& to) {
    translate(p, to - p.base);
}

std::vector<modify::Change> makePasteChanges(const Payload& p) {
    std::vector<modify::Change> out;
    out.reserve(p.elements.size());
    for (const auto& e : p.elements) {
        modify::Change c;
        c.id = -1;                       // 全部新增（applyChanges 内重新分配 ID）
        Element ne = e;
        ne.id = -1;
        c.replacement = std::move(ne);
        c.note = QStringLiteral("剪贴板粘贴");
        out.push_back(std::move(c));
    }
    return out;
}

BlockDef makeBlockDefFromPayload(const Payload& p, const QString& name) {
    return block::makeDefFromEntities(name, p.base, p.elements,
                                      QStringLiteral("剪贴板粘贴为块"));
}

Element makeBlockRefElement(const BlockDef& def, const Vec3& insertPos) {
    // block::makeRef 依据 def.basePoint 对齐 insertPos：p' = insertPos + (p − base)，
    // 即「粘贴基点对齐落点」语义与 pasteclip 完全一致。
    // ⚠️ id 必须显式置 -1（新增语义）：makeRef 产出的 Element.id 默认 0，
    // 而 applyChanges 只认 id<0 为新增——id=0 会走 updateElementQuiet(0) 的
    // 「复用 ID 更新」分支，落空模型里空图元时静默失败（无报错、无落盘）。
    Element e = block::makeRef(def, insertPos, 0.0, 1.0, 1.0);
    e.id = -1;
    return e;
}

QString uniqueBlockName(const SketchModel& m, const QString& base) {
    const QString b = base.trimmed().isEmpty() ? QStringLiteral("粘贴块") : base.trimmed();
    if (!m.hasBlock(b)) return b;
    for (int i = 1; ; ++i) {
        const QString cand = QStringLiteral("%1_%2").arg(b).arg(i);
        if (!m.hasBlock(cand)) return cand;
    }
}

std::vector<BlockDef> collectBlockDeps(const SketchModel& m,
                                       const std::vector<Element>& elements) {
    std::vector<BlockDef> deps;
    auto collect = [&](const QString& name, auto&& self) -> void {
        if (name.isEmpty()) return;
        for (const BlockDef& d : m.blockTable()) {
            if (d.name != name) continue;
            bool dup = false;
            for (const auto& x : deps) if (x.name == d.name) { dup = true; break; }
            if (dup) break;
            deps.push_back(d);
            for (const Element& inner : d.entities)          // 嵌套块递归
                if (inner.type == ElementType::BlockRef) self(inner.blockName, self);
            break;
        }
    };
    for (const Element& e : elements)
        if (e.type == ElementType::BlockRef) collect(e.blockName, collect);
    return deps;
}

bool pasteInto(SketchModel* m, const Payload& p, const Vec3& to,
               bool asBlock, QString* msg) {
    if (!m) return false;
    if (p.elements.empty()) {
        if (msg) *msg = QStringLiteral("剪贴板中没有图元");
        return false;
    }

    Payload p2 = p;   // 全程在副本上操作，避免污染调用方
    const QString label = asBlock ? QStringLiteral("剪贴板-粘贴为块")
                                  : QStringLiteral("剪贴板-粘贴");

    // 1) 合并块表：与目标图同名时自动改名（name_1…），并同步更新引用名映射。
    //    ⚠️ 必须在 translate 之前做名字映射——改名按旧名查。
    QMap<QString, QString> renamed;   // 旧块名 → 新块名
    for (BlockDef& d : p2.blocks) {
        const QString nn = uniqueBlockName(*m, d.name);
        if (nn != d.name) renamed.insert(d.name, nn);
        d.name = nn;
        if (!m->addBlockDefQuiet(d)) {
            if (msg) *msg = QStringLiteral("块 %1 合并失败").arg(nn);
            return false;
        }
    }
    if (!renamed.isEmpty()) {
        for (Element& e : p2.elements) {
            if (e.type != ElementType::BlockRef) continue;
            auto it = renamed.find(e.blockName);
            if (it != renamed.end()) e.blockName = it.value();
        }
    }

    if (asBlock) {
        // 2a) 粘贴为块：整包收进一个块定义 + 放置一个块引用。
        //     注意「先 translateTo 再收块」会让 base 平移到 to，块内几何跟着动——
        //     这样引用展开后正好落在 to，与 makeBlockRefElement 的对齐语义二选一即可。
        //     这里保持块内几何 = 原世界坐标、base = 原基点，由引用变换对齐 to（AutoCAD 语义）。
        const QString defName = uniqueBlockName(*m, QStringLiteral("粘贴块"));
        BlockDef def = makeBlockDefFromPayload(p2, defName);
        if (!m->addBlockDefQuiet(def)) {
            if (msg) *msg = QStringLiteral("块 %1 创建失败").arg(defName);
            return false;
        }
        Element ref = makeBlockRefElement(def, to);
        std::vector<modify::Change> changes;
        modify::Change c; c.id = -1; c.replacement = std::move(ref);
        c.note = QStringLiteral("粘贴为块 %1").arg(defName);
        changes.push_back(std::move(c));
        modify::applyChanges(m, changes, QString());
        if (msg) *msg = QStringLiteral("已粘贴为块 %1（%2 个图元），插入点 (%3,%4)")
                            .arg(defName).arg(p2.elements.size())
                            .arg(to.x, 0, 'f', 2).arg(to.y, 0, 'f', 2);
        return true;
    }

    // 2b) 普通粘贴：平移对齐落点后全部新增。
    translateTo(p2, to);
    modify::applyChanges(m, makePasteChanges(p2), QString());
    if (msg) *msg = QStringLiteral("已粘贴 %1 个图元到 (%2,%3)")
                        .arg(p2.elements.size())
                        .arg(to.x, 0, 'f', 2).arg(to.y, 0, 'f', 2);
    return true;
}

bool putToClipboard(const Payload& p) {
    if (!clipboardAvailable()) return false;
    QClipboard* cb = QGuiApplication::clipboard();
    if (!cb) return false;
    auto* mime = new QMimeData();
    mime->setData(QString::fromLatin1(kMimeName),
                  QJsonDocument(toJson(p)).toJson(QJsonDocument::Compact));
    cb->setMimeData(mime);
    return true;
}

bool getFromClipboard(Payload* out, QString* err) {
    if (!clipboardAvailable()) {
        if (err) *err = QStringLiteral("当前无 GUI 会话，剪贴板不可用");
        return false;
    }
    QClipboard* cb = QGuiApplication::clipboard();
    if (!cb) { if (err) *err = QStringLiteral("剪贴板不可用"); return false; }
    const QMimeData* mime = cb->mimeData();
    if (!mime || !mime->hasFormat(QString::fromLatin1(kMimeName))) {
        if (err) *err = QStringLiteral("剪贴板中没有 SketchCAD 内容");
        return false;
    }
    const QByteArray raw = mime->data(QString::fromLatin1(kMimeName));
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &pe);
    if (doc.isNull()) {
        if (err) *err = QStringLiteral("剪贴板 JSON 解析失败：%1").arg(pe.errorString());
        return false;
    }
    return fromJson(doc.object(), out, err);
}

} // namespace clipboard
} // namespace SketchCAD
