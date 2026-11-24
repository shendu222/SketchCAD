// =============================================================================
// SketchCAD Hosting ABI 实现 —— 让 SketchCAD 本体作为动态库被外部宿主加载
// -----------------------------------------------------------------------------
// 嵌入铁律（与 OpenMBD Studio 集成时逐条验证）：
//   * 不构造 QApplication / 不调用 src/main.cpp 的任何启动逻辑；
//   * 复用宿主的 QApplication 与事件循环，回写回调由宿主再投递一层 singleShot；
//   * 所有异常在 C 边界内吞掉并转成状态码 + last_error，绝不跨 ABI 抛出；
//   * 句柄由插件自己分配/释放，跨模块不传 STL / Qt 容器。
// =============================================================================
#include "SketchCAD/hosting_abi.h"

#include "core/AutoSave.h"
#include "core/CadFormats.h"
#include "core/CommandInterpreter.h"
#include "core/DwgBridge.h"
#include "core/Element.h"
#include "core/ElementValidate.h"
#include "core/SheetSpec.h"
#include "core/SketchModel.h"
#include "core/Vec.h"
#include "ui/GraphicsView.h"

#include <QByteArray>
#include <QColor>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QObject>
#include <QString>
#include <QTextStream>
#include <QVariant>
#include <QWidget>
#include <QtGlobal>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <vector>

namespace {

struct Session {
    ::SketchCAD::SketchModel* model = nullptr;
    ::SketchCAD::CommandInterpreter* interp = nullptr;
    ::SketchCAD::AutoSave* autoSave = nullptr;
    QWidget* editor = nullptr;
    OmbdSkWritebackCallback writeback = nullptr;
    void* userData = nullptr;
    bool suppressWriteback = false;
};

thread_local QByteArray t_lastError;

void setError(const QString& msg) {
    t_lastError = msg.toUtf8();
    if (t_lastError.isEmpty()) t_lastError = QByteArray("unknown error");
}

void clearError() { t_lastError.clear(); }

void logLine(const OmbdSkHostEnv* env, int level, const QString& msg) {
  if (!env || !env->log) return;
  const QByteArray bytes = msg.toUtf8();
  env->log(level, bytes.constData(), env->userData);
}

// ---- v2 交换 ABI 的纯 Qt/sketchCAD 辅助层 ---------------------------------
using ScElement = ::SketchCAD::Element;
using ScVec = ::SketchCAD::Vec3;

constexpr double kPi = 3.14159265358979323846;
constexpr const char* kBridgePropsKey = "ombd.bridge";

uint8_t* allocRaw(const QByteArray& bytes) {
  auto* buf = static_cast<uint8_t*>(std::malloc(bytes.isEmpty() ? 1
                                                               : static_cast<size_t>(bytes.size())));
  if (!buf) return nullptr;
  if (!bytes.isEmpty())
    std::memcpy(buf, bytes.constData(), static_cast<size_t>(bytes.size()));
  return buf;
}

int32_t writeBuffer(const QByteArray& bytes, uint8_t** outBytes, int32_t* outLen) {
  if (!outBytes || !outLen) return OMBD_SK_ERR_INVALID_ARG;
  *outBytes = nullptr;
  *outLen = 0;
  uint8_t* raw = allocRaw(bytes);
  if (!raw) {
    setError(QStringLiteral("导出缓冲分配失败"));
    return OMBD_SK_ERR_INTERNAL;
  }
  *outBytes = raw;
  *outLen = static_cast<int32_t>(bytes.size());
  return OMBD_SK_OK;
}

QJsonObject objectFromBytes(const uint8_t* bytes, int32_t len, QString* err) {
  if (!bytes || len <= 0) {
    if (err) *err = QStringLiteral("输入为空");
    return QJsonObject();
  }
  QJsonParseError pe;
  const QJsonDocument d = QJsonDocument::fromJson(
      QByteArray(reinterpret_cast<const char*>(bytes), len), &pe);
  if (pe.error != QJsonParseError::NoError || !d.isObject()) {
    if (err) *err = QStringLiteral("JSON 解析失败：%1").arg(pe.errorString());
    return QJsonObject();
  }
  return d.object();
}

QJsonArray vecToJson(double x, double y, double z = 0.0) {
  QJsonArray a;
  a << x << y << z;
  return a;
}

std::vector<ScVec> pointsFromJson(const QJsonValue& v) {
  std::vector<ScVec> out;
  if (!v.isArray()) return out;
  const QJsonArray a = v.toArray();
  out.reserve(static_cast<size_t>(a.size()));
  for (const QJsonValue& p : a) {
    const QJsonArray q = p.toArray();
    if (q.size() < 2) continue;
    out.emplace_back(q.at(0).toDouble(), q.at(1).toDouble(),
                     q.size() >= 3 ? q.at(2).toDouble() : 0.0);
  }
  return out;
}

QJsonArray pointsToJson(const std::vector<ScVec>& pts) {
  QJsonArray a;
  for (const auto& p : pts) a << vecToJson(p.x, p.y, p.z);
  return a;
}

QString scTypeName(::SketchCAD::ElementType t) {
  using T = ::SketchCAD::ElementType;
  switch (t) {
    case T::Line: return QStringLiteral("Line");
    case T::Circle: return QStringLiteral("Circle");
    case T::Arc: return QStringLiteral("Arc");
    case T::Rectangle: return QStringLiteral("Rectangle");
    case T::Polygon: return QStringLiteral("Polygon");
    case T::Spline: return QStringLiteral("Spline");
    case T::Constraint: return QStringLiteral("Constraint");
    case T::Dimension: return QStringLiteral("Dimension");
    case T::Ellipse: return QStringLiteral("Ellipse");
    case T::Polyline: return QStringLiteral("Polyline");
    case T::Text: return QStringLiteral("Text");
    case T::Hatch: return QStringLiteral("Hatch");
    case T::BlockRef: return QStringLiteral("BlockRef");
    case T::Xline: return QStringLiteral("Xline");
    case T::Boundary: return QStringLiteral("Boundary");
    case T::MText: return QStringLiteral("MText");
    case T::Table: return QStringLiteral("Table");
    case T::Point: return QStringLiteral("Point");
    case T::MLeader: return QStringLiteral("MLeader");
    case T::Fcf: return QStringLiteral("Fcf");
  }
  return QStringLiteral("Unknown");
}

QJsonArray toJsonInts(const std::vector<int>& v) {
  QJsonArray a;
  for (int x : v) a << x;
  return a;
}

QJsonArray toJsonStrings(const std::vector<QString>& v) {
  QJsonArray a;
  for (const QString& s : v) a << s;
  return a;
}

QJsonObject elementMetaJson(const ScElement& e) {
  using T = ::SketchCAD::ElementType;
  QJsonObject m;
  m.insert(QStringLiteral("id"), e.id);
  m.insert(QStringLiteral("type"), scTypeName(e.type));
  m.insert(QStringLiteral("name"), e.name);
  m.insert(QStringLiteral("layer"), e.layer);
  m.insert(QStringLiteral("text"), e.text);
  m.insert(QStringLiteral("dimValue"), e.dimValue);
  m.insert(QStringLiteral("dimDriven"), e.dimDriven);
  m.insert(QStringLiteral("dimParam"), e.dimParam);
  m.insert(QStringLiteral("dimLinked"), e.dimLinked);
  m.insert(QStringLiteral("ctype"), static_cast<int>(e.ctype));
  m.insert(QStringLiteral("dtype"), static_cast<int>(e.dtype));
  m.insert(QStringLiteral("tolUpper"), e.tolUpper);
  m.insert(QStringLiteral("tolLower"), e.tolLower);
  m.insert(QStringLiteral("fitCode"), e.fitCode);
  m.insert(QStringLiteral("refA"), e.refA);
  m.insert(QStringLiteral("refB"), e.refB);
  m.insert(QStringLiteral("radius"), e.radius);
  m.insert(QStringLiteral("startAngle"), e.startAngle);
  m.insert(QStringLiteral("endAngle"), e.endAngle);
  m.insert(QStringLiteral("rotation"), e.rotation);
  m.insert(QStringLiteral("splineSubType"), e.splineSubType);
  m.insert(QStringLiteral("visible"), e.visible);
  switch (e.type) {
    case T::Hatch:
      m.insert(QStringLiteral("sectionMaterial"), e.sectionMaterial);
      m.insert(QStringLiteral("sectionExempt"), e.sectionExempt);
      m.insert(QStringLiteral("hatchPattern"), e.hatchPattern);
      m.insert(QStringLiteral("hatchScale"), e.hatchScale);
      m.insert(QStringLiteral("hatchSrcIds"), toJsonInts(e.hatchSrcIds));
      break;
    case T::Table:
      m.insert(QStringLiteral("tableRows"), e.tableRows);
      m.insert(QStringLiteral("tableCols"), e.tableCols);
      m.insert(QStringLiteral("rowHeight"), e.rowHeight);
      m.insert(QStringLiteral("tableCells"), toJsonStrings(e.tableCells));
      break;
    case T::BlockRef:
      m.insert(QStringLiteral("blockName"), e.blockName);
      m.insert(QStringLiteral("scaleX"), e.scaleX);
      m.insert(QStringLiteral("scaleY"), e.scaleY);
      break;
    case T::Fcf:
      m.insert(QStringLiteral("fcfSymbol"), e.fcfSymbol);
      m.insert(QStringLiteral("fcfModifier"), e.fcfModifier);
      m.insert(QStringLiteral("fcfTolerance"), e.fcfTolerance);
      m.insert(QStringLiteral("fcfDatums"), toJsonStrings(e.fcfDatums));
      break;
    case T::MText:
      m.insert(QStringLiteral("widthFactor"), e.widthFactor);
      m.insert(QStringLiteral("styleName"), e.styleName);
      break;
    case T::MLeader:
      m.insert(QStringLiteral("mleaderLanding"), e.mleaderLanding);
      m.insert(QStringLiteral("mleaderArrowLen"), e.mleaderArrowLen);
      break;
    default:
      break;
  }
  return m;
}

ScElement polyToElement(const QJsonObject& p, const QString& fallbackLayer) {
  ScElement e;
  e.type = ::SketchCAD::ElementType::Polyline;
  e.layer = p.value(QStringLiteral("layer")).toString(fallbackLayer);
  if (e.layer.isEmpty()) e.layer = QStringLiteral("visible");
  e.name = QStringLiteral("ombd:view:");
  e.closed = p.value(QStringLiteral("closed")).toBool(false);
  e.pts = pointsFromJson(p.value(QStringLiteral("pts")));
  const int rgba = p.value(QStringLiteral("strokeRgb")).toInt(0);
  if (rgba != 0) {
    e.color = QColor((rgba >> 16) & 0xff, (rgba >> 8) & 0xff, rgba & 0xff);
    e.byLayer = false;
  }
  const double width = p.value(QStringLiteral("widthMm")).toDouble(0.0);
  if (width > 0.0) {
    e.lineWidthAuto = false;
    e.lineWidth = width;
  }
  switch (p.value(QStringLiteral("lineStyle")).toInt(0)) {
    case 1: e.lineStyle = ::SketchCAD::LineStyle::Dashed; e.byLayer = false; break;
    case 2: e.lineStyle = ::SketchCAD::LineStyle::DashDot; e.byLayer = false; break;
    default: break;
  }
  return e;
}

ScElement dimToElement(const QJsonObject& d) {
  ScElement e;
  e.type = ::SketchCAD::ElementType::Dimension;
  e.layer = QStringLiteral("dimension");
  e.name = QStringLiteral("ombd:pmi:") + d.value(QStringLiteral("sourceId")).toString();
  e.text = d.value(QStringLiteral("text")).toString();
  e.dimValue = d.value(QStringLiteral("nominal")).toDouble(0.0);
  e.dimDriven = true;
  e.dimLinked = false;
  const int dtype = d.value(QStringLiteral("dtype")).toInt(0);
  e.dtype = dtype == 2 ? ::SketchCAD::DimensionType::Leader
          : dtype == 1 ? ::SketchCAD::DimensionType::Vertical
                       : ::SketchCAD::DimensionType::Distance;
  e.pts = pointsFromJson(d.value(QStringLiteral("pts")));
  e.radius = d.value(QStringLiteral("textHeight")).toDouble(3.5);
  if (!(e.radius > 0.0)) e.radius = 3.5;
  return e;
}

std::vector<ScVec> sampleEllipseByDeviation(const ScVec& c, double a, double b,
                                            double rotDeg, double dev) {
  std::vector<ScVec> out;
  const double amin = std::min(a, b), amax = std::max(a, b);
  if (!(amin > 0.0) || !(amax > 0.0)) return out;
  const double rhoMin = amin * amin / amax;
  const double safeDev = dev > 0.0 ? dev : 0.01;
  const double arg = std::clamp(1.0 - safeDev / rhoMin, -1.0, 1.0);
  const double dTheta = std::max(2.0 * std::acos(arg), 1e-4);
  const int seg = std::clamp(static_cast<int>(std::ceil(2.0 * kPi / dTheta)), 8, 4096);
  out.reserve(static_cast<size_t>(seg) + 1);
  const double rot = rotDeg * kPi / 180.0;
  const double cr = std::cos(rot), sr = std::sin(rot);
  for (int i = 0; i <= seg; ++i) {
    const double t = 2.0 * kPi * i / seg;
    const double x = a * std::cos(t), y = b * std::sin(t);
    out.emplace_back(c.x + x * cr - y * sr, c.y + x * sr + y * cr, 0.0);
  }
  return out;
}

ScVec catmullRomAt(const std::vector<ScVec>& p, double t) {
  const int n = static_cast<int>(p.size());
  if (n < 2) return ScVec();
  int i = static_cast<int>(std::floor(t));
  i = std::clamp(i, 0, n - 2);
  const double u = std::clamp(t - i, 0.0, 1.0);
  auto at = [&](int k) -> const ScVec& { return p[static_cast<size_t>(std::clamp(k, 0, n - 1))]; };
  const ScVec& q0 = at(i - 1);
  const ScVec& q1 = at(i);
  const ScVec& q2 = at(i + 1);
  const ScVec& q3 = at(i + 2);
  auto comp = [&](double v0, double v1, double v2, double v3) {
    return 0.5 * ((2.0 * v1) + (-v0 + v2) * u +
                  (2.0 * v0 - 5.0 * v1 + 4.0 * v2 - v3) * u * u +
                  (-v0 + 3.0 * v1 - 3.0 * v2 + v3) * u * u * u);
  };
  return ScVec(comp(q0.x, q1.x, q2.x, q3.x), comp(q0.y, q1.y, q2.y, q3.y), 0.0);
}

double chordDeviation(const ScVec& p, const ScVec& a, const ScVec& b) {
  const double dx = b.x - a.x, dy = b.y - a.y;
  const double len = std::sqrt(dx * dx + dy * dy);
  if (len < 1e-12)
    return std::sqrt((p.x - a.x) * (p.x - a.x) + (p.y - a.y) * (p.y - a.y));
  return std::fabs((p.x - a.x) * dy - (p.y - a.y) * dx) / len;
}

void refineSplineSegment(const std::vector<ScVec>& ctrl, double t0, double t1, double dev,
                         std::vector<ScVec>& out, int depth) {
  const ScVec a = catmullRomAt(ctrl, t0), b = catmullRomAt(ctrl, t1);
  double worst = 0.0;
  constexpr std::array<double, 3> probes{{0.25, 0.5, 0.75}};
  for (double f : probes)
    worst = std::max(worst, chordDeviation(catmullRomAt(ctrl, t0 + (t1 - t0) * f), a, b));
  if (worst <= dev || depth >= 20) {
    if (out.empty() || std::abs(out.back().x - a.x) > 1e-12 ||
        std::abs(out.back().y - a.y) > 1e-12) out.push_back(a);
    out.push_back(b);
    return;
  }
  const double tm = 0.5 * (t0 + t1);
  refineSplineSegment(ctrl, t0, tm, dev, out, depth + 1);
  refineSplineSegment(ctrl, tm, t1, dev, out, depth + 1);
}

std::vector<ScVec> sampleSplineByDeviation(const std::vector<ScVec>& ctrl, double dev) {
  std::vector<ScVec> out;
  if (ctrl.size() < 2) return out;
  refineSplineSegment(ctrl, 0.0, static_cast<double>(ctrl.size() - 1), dev, out, 0);
  return out;
}

QJsonObject refJson(const ScElement& e, const QString& policy, double dev) {
  QJsonObject r;
  r.insert(QStringLiteral("sourceElementId"), e.id);
  r.insert(QStringLiteral("sourceElementType"), scTypeName(e.type));
  r.insert(QStringLiteral("policy"), policy);
  r.insert(QStringLiteral("maxDeviationMm"), dev);
  return r;
}

QJsonObject geometryJson(const QString& kind, const QJsonObject& params,
                         const ScElement& e, const QString& policy, double dev) {
  QJsonObject g = params;
  g.insert(QStringLiteral("kind"), kind);
  QJsonArray refs;
  refs << refJson(e, policy, dev);
  g.insert(QStringLiteral("refs"), refs);
  return g;
}

QString vecJsonString(const QJsonValue& v) {
  const QJsonArray a = v.toArray();
  return QStringLiteral("[%1,%2,%3]")
      .arg(a.size() > 0 ? a.at(0).toDouble() : 0.0, 0, 'g', 12)
      .arg(a.size() > 1 ? a.at(1).toDouble() : 0.0, 0, 'g', 12)
      .arg(a.size() > 2 ? a.at(2).toDouble() : 0.0, 0, 'g', 12);
}

} // namespace

extern "C" {

int32_t ombd_sketchcad_abi_version(void) {
    return OMBD_SKETCHCAD_HOSTING_ABI_VERSION;
}

int32_t ombd_sketchcad_qt_version(void) {
    return QT_VERSION;   // 宿主用自己的 QT_VERSION 比对，拒绝混用不同 Qt 小版本
}

const char* ombd_sketchcad_last_error(void) {
    if (t_lastError.isEmpty()) return "";
    t_lastError.append('\0');   // 保证以 NUL 结尾；constData 始终带终止符
    return t_lastError.constData();
}

void* ombd_sketchcad_create(const OmbdSkHostEnv* env) {
    clearError();
    try {
        if (!env || env->structSize < sizeof(OmbdSkHostEnv)) {
            setError(QStringLiteral("宿主环境结构体尺寸不匹配"));
            return nullptr;
        }
        if (!QCoreApplication::instance()) {
            setError(QStringLiteral("宿主必须先创建 QCoreApplication；插件禁止构造第二个应用实例"));
            return nullptr;
        }

        // 路径解析全部来自宿主注入：插件不依赖 CWD，也不猜测宿主布局。
        if (env->resourceDir && *env->resourceDir) {
            QDir::addSearchPath(QStringLiteral("sketchcad"),
                                QString::fromUtf8(env->resourceDir));
        }
        if (env->toolsDir && *env->toolsDir) {
            // DwgBridge 定位顺序第 1 位即该环境变量；显式注入可覆盖 exe 相对布局。
            qputenv("SKETCHCAD_DWGTOOLS", QByteArray(env->toolsDir));
        }

        auto* s = new Session();
        s->model = new ::SketchCAD::SketchModel();
        s->interp = new ::SketchCAD::CommandInterpreter(s->model);
        s->autoSave = new ::SketchCAD::AutoSave();
        s->autoSave->setProvider([s]() -> QByteArray {
            return s->model ? s->model->toDrawingBytes() : QByteArray();
        });
        s->autoSave->setIntervalMinutes(5);
        s->autoSave->setCurrentDrawing(QString());   // 嵌入模式不写外部图纸路径
        s->autoSave->start();

        // 载入宿主 JSON 会重建模型内容；解释器需重绑到同一对象，历史留在会话内。
        // 命令执行走同一解释器，与 sketchCAD 原生命令行/菜单同源。

        QObject::connect(s->model, &::SketchCAD::SketchModel::modelChanged,
                         s->model, [s]() {
            if (s->suppressWriteback || !s->writeback || !s->model) return;
            const QByteArray bytes = s->model->toDrawingBytes();
            s->writeback(reinterpret_cast<const uint8_t*>(bytes.constData()),
                         static_cast<int32_t>(bytes.size()), s->userData);
        });

        logLine(env, 1, QStringLiteral("SketchCAD 宿主环境就绪：abi=%1 qt=%2 tools=%3")
                            .arg(ombd_sketchcad_abi_version())
                            .arg(QT_VERSION)
                            .arg(env->toolsDir ? QString::fromUtf8(env->toolsDir)
                                               : QStringLiteral("<未注入>")));
        return static_cast<void*>(s);
    } catch (const std::exception& e) {
        setError(QString::fromUtf8(e.what()));
        return nullptr;
    } catch (...) {
        setError(QStringLiteral("create 未预期异常"));
        return nullptr;
    }
}

void ombd_sketchcad_destroy(void* handle) {
    clearError();
    auto* s = static_cast<Session*>(handle);
    if (!s) return;
    // 生命周期铁律：停自动保存 → 清 provider → 析构 AutoSave → 最后析构 Model。
    // 编辑器必须已由 destroy_editor 释放；此处做兜底，避免调用方漏调导致悬空连接。
    if (s->editor) ombd_sketchcad_destroy_editor(handle, s->editor);
    try {
        s->autoSave->stop();
        s->autoSave->setProvider(nullptr);
        delete s->autoSave;
        s->autoSave = nullptr;
        delete s->interp;
        s->interp = nullptr;
        delete s->model;
        s->model = nullptr;
        delete s;
    } catch (...) {
        setError(QStringLiteral("destroy 未预期异常"));
    }
}

void* ombd_sketchcad_create_editor(void* handle, void* parentWidget) {
    clearError();
    auto* s = static_cast<Session*>(handle);
    if (!s || !s->model) {
        setError(QStringLiteral("会话未初始化，无法创建编辑器"));
        return nullptr;
    }
    try {
        auto* view = new ::SketchCAD::GraphicsView(
            s->model, static_cast<QWidget*>(parentWidget));
        s->editor = view;
        return static_cast<void*>(view);
    } catch (const std::exception& e) {
        setError(QString::fromUtf8(e.what()));
        return nullptr;
    } catch (...) {
        setError(QStringLiteral("create_editor 未预期异常"));
        return nullptr;
    }
}

void ombd_sketchcad_destroy_editor(void* handle, void* editorWidget) {
    clearError();
    auto* s = static_cast<Session*>(handle);
    if (!s || !editorWidget || editorWidget != s->editor) return;
    try {
        auto* view = static_cast<QWidget*>(s->editor);
        if (auto* gv = dynamic_cast<::SketchCAD::GraphicsView*>(view))
            gv->cancelDrawing();   // 取消橡皮筋/待定点，杜绝析构时活动工具
        s->editor = nullptr;
        view->setParent(nullptr);  // 脱离宿主 QDockWidget 所有权
        delete view;
    } catch (...) {
        setError(QStringLiteral("destroy_editor 未预期异常"));
    }
}

int32_t ombd_sketchcad_load_json(void* handle, const uint8_t* bytes, int32_t len) {
    clearError();
    auto* s = static_cast<Session*>(handle);
    if (!s || !s->model || !bytes || len <= 0)
        return OMBD_SK_ERR_INVALID_ARG;
    try {
        // 宿主→编辑器载入必须抑制回写，否则会与宿主的文档刷新形成回环。
        s->suppressWriteback = true;
        const QByteArray payload(reinterpret_cast<const char*>(bytes), len);
        const bool ok = s->model->applyDrawingJson(payload, nullptr);
        s->suppressWriteback = false;
        if (!ok) {
            setError(QStringLiteral("图纸 JSON 导入失败（版本或结构不受支持）"));
            return OMBD_SK_ERR_INTERNAL;
        }
        return OMBD_SK_OK;
    } catch (...) {
        s->suppressWriteback = false;
        setError(QStringLiteral("load_json 未预期异常"));
        return OMBD_SK_ERR_INTERNAL;
    }
}

int32_t ombd_sketchcad_export_json(void* handle, uint8_t** outBytes, int32_t* outLen) {
    clearError();
    auto* s = static_cast<Session*>(handle);
    if (!s || !s->model || !outBytes || !outLen)
        return OMBD_SK_ERR_INVALID_ARG;
    *outBytes = nullptr;
    *outLen = 0;
    try {
        const QByteArray bytes = s->model->toDrawingBytes();
        // malloc 分配，确保由本 DLL 的 free_bytes 释放（同一 CRT 堆）。
        auto* buf = static_cast<uint8_t*>(std::malloc(
            bytes.isEmpty() ? 1 : static_cast<size_t>(bytes.size())));
        if (!buf) {
            setError(QStringLiteral("导出缓冲分配失败"));
            return OMBD_SK_ERR_INTERNAL;
        }
        if (!bytes.isEmpty())
            std::memcpy(buf, bytes.constData(), static_cast<size_t>(bytes.size()));
        *outBytes = buf;
        *outLen = static_cast<int32_t>(bytes.size());
        return OMBD_SK_OK;
    } catch (...) {
        setError(QStringLiteral("export_json 未预期异常"));
        return OMBD_SK_ERR_INTERNAL;
    }
}

void ombd_sketchcad_free_bytes(uint8_t* bytes) {
    std::free(bytes);
}

int32_t ombd_sketchcad_set_writeback(void* handle,
                                     OmbdSkWritebackCallback cb, void* userData) {
    clearError();
    auto* s = static_cast<Session*>(handle);
    if (!s) return OMBD_SK_ERR_INVALID_ARG;
    s->writeback = cb;
    s->userData = userData;
    return OMBD_SK_OK;
}

int32_t ombd_sketchcad_notify_local_change(void* handle) {
    clearError();
    auto* s = static_cast<Session*>(handle);
    if (!s || !s->model) return OMBD_SK_ERR_INVALID_ARG;
    try {
        s->model->emitModelChanged();
        return OMBD_SK_OK;
    } catch (...) {
        setError(QStringLiteral("notify_local_change 未预期异常"));
        return OMBD_SK_ERR_INTERNAL;
    }
}

int32_t ombd_sketchcad_refresh_editor(void* handle) {
    clearError();
    auto* s = static_cast<Session*>(handle);
    if (!s || !s->model) return OMBD_SK_ERR_INVALID_ARG;
    if (!s->editor) return OMBD_SK_OK;   // 无编辑器时静默成功：载入模型仍有效
    try {
        auto* view = dynamic_cast<::SketchCAD::GraphicsView*>(s->editor);
        if (view) {
            view->onModelChanged();
            view->fitView();
        }
        return OMBD_SK_OK;
    } catch (...) {
        setError(QStringLiteral("refresh_editor 未预期异常"));
        return OMBD_SK_ERR_INTERNAL;
    }
}

int32_t ombd_sketchcad_exec_command(void* handle, const uint8_t* bytes, int32_t len) {
    clearError();
    auto* s = static_cast<Session*>(handle);
    if (!s || !s->model || !s->interp || !bytes || len <= 0)
        return OMBD_SK_ERR_INVALID_ARG;
    try {
        const QString line = QString::fromUtf8(
            reinterpret_cast<const char*>(bytes), static_cast<int>(len)).trimmed();
        if (line.isEmpty()) return OMBD_SK_OK;   // 空行等价于原生命令行的空回车（无操作）
        s->interp->addHistory(line);
        if (!s->interp->execute(line)) {
            setError(QStringLiteral("命令执行失败：%1").arg(line));
            return OMBD_SK_ERR_INTERNAL;
        }
        if (s->editor) {
            auto* view = dynamic_cast<::SketchCAD::GraphicsView*>(s->editor);
            if (view) view->onModelChanged();
        }
        return OMBD_SK_OK;
    } catch (...) {
        setError(QStringLiteral("exec_command 未预期异常"));
        return OMBD_SK_ERR_INTERNAL;
    }
}

int32_t ombd_sketchcad_set_tool(void* handle, const uint8_t* bytes, int32_t len) {
    clearError();
    auto* s = static_cast<Session*>(handle);
    if (!s || !s->model || !bytes || len <= 0) return OMBD_SK_ERR_INVALID_ARG;
    try {
        const QString id = QString::fromUtf8(
            reinterpret_cast<const char*>(bytes), static_cast<int>(len)).trimmed();
        if (id.isEmpty()) return OMBD_SK_ERR_INVALID_ARG;
        s->model->setActiveTool(id);
        if (s->editor) {
            auto* view = dynamic_cast<::SketchCAD::GraphicsView*>(s->editor);
            if (view) view->update();
        }
        return OMBD_SK_OK;
    } catch (...) {
        setError(QStringLiteral("set_tool 未预期异常"));
        return OMBD_SK_ERR_INTERNAL;
    }
}

int32_t ombd_sketchcad_active_tool(void* handle, uint8_t** outBytes, int32_t* outLen) {
    clearError();
    auto* s = static_cast<Session*>(handle);
    if (!s || !s->model || !outBytes || !outLen) return OMBD_SK_ERR_INVALID_ARG;
    try {
        return writeBuffer(s->model->activeTool().toUtf8(), outBytes, outLen);
    } catch (...) {
        setError(QStringLiteral("active_tool 未预期异常"));
        return OMBD_SK_ERR_INTERNAL;
    }
}

int32_t ombd_sketchcad_command_names(uint8_t** outBytes, int32_t* outLen) {
    clearError();
    if (!outBytes || !outLen) return OMBD_SK_ERR_INVALID_ARG;
    try {
        const QString names = ::SketchCAD::CommandInterpreter::commandNames().join(QLatin1Char('\n'));
        return writeBuffer(names.toUtf8(), outBytes, outLen);
    } catch (...) {
        setError(QStringLiteral("command_names 未预期异常"));
        return OMBD_SK_ERR_INTERNAL;
    }
}

int32_t ombd_sketchcad_self_check(char* out, int32_t cap) {
  clearError();
  if (!out || cap <= 0) return OMBD_SK_ERR_INVALID_ARG;
  const QString dir = ::SketchCAD::dwgbridge::resolvedToolsDir();
  QString report;
  QTextStream(&report) << QStringLiteral("abi=%1\nqt=%2\ndwg=%3\ndwgDir=%4\n")
                              .arg(ombd_sketchcad_abi_version())
                              .arg(QT_VERSION)
                              .arg(::SketchCAD::dwgbridge::available()
                                       ? QStringLiteral("available")
                                       : QStringLiteral("unavailable"))
                              .arg(dir);
  const QByteArray bytes = report.toUtf8();
  const int n = qMin<int>(static_cast<int>(bytes.size()), cap - 1);
  std::memcpy(out, bytes.constData(), static_cast<size_t>(n));
  out[n] = '\0';
  return OMBD_SK_OK;
}

// ---- v2：无状态交换 ABI ----------------------------------------------------
int32_t ombd_sketchcad_exchange_build_three_views(
    const uint8_t* neutralJson, int32_t len, uint8_t** outBytes, int32_t* outLen) {
  clearError();
  if (outBytes) *outBytes = nullptr;
  if (outLen) *outLen = 0;
  QString err;
  const QJsonObject root = objectFromBytes(neutralJson, len, &err);
  if (root.isEmpty()) {
    setError(err);
    return OMBD_SK_ERR_INVALID_ARG;
  }

  try {
    ::SketchCAD::SketchModel model;
    std::vector<ScElement> parts;
    QJsonObject bindings;
    const QJsonArray views = root.value(QStringLiteral("views")).toArray();
    const bool withHidden = root.value(QStringLiteral("withHidden")).toBool(true);
    const bool withDims = root.value(QStringLiteral("pmiDimensions")).toBool(true);

    for (int vi = 0; vi < views.size(); ++vi) {
      const QJsonObject v = views.at(vi).toObject();
      QJsonObject binding;
      binding.insert(QStringLiteral("name"), v.value(QStringLiteral("name")).toString());
      binding.insert(QStringLiteral("type"), v.value(QStringLiteral("type")).toString());
      binding.insert(QStringLiteral("direction"),
                     vecJsonString(v.value(QStringLiteral("direction"))));
      binding.insert(QStringLiteral("up"), vecJsonString(v.value(QStringLiteral("up"))));
      binding.insert(QStringLiteral("originX"), v.value(QStringLiteral("originX")).toDouble());
      binding.insert(QStringLiteral("originY"), v.value(QStringLiteral("originY")).toDouble());
      binding.insert(QStringLiteral("degraded"), v.value(QStringLiteral("degraded")).toBool());
      QJsonObject viewCounts;
      const int visibleFirst = static_cast<int>(parts.size());
      auto appendPolys = [&](const QJsonValue& arr, const QString& fallback) {
        for (const QJsonValue& pv : arr.toArray()) {
          const QJsonObject p = pv.toObject();
          if (pointsFromJson(p.value(QStringLiteral("pts"))).size() < 2) continue;
          parts.push_back(polyToElement(p, fallback));
        }
      };
      appendPolys(v.value(QStringLiteral("polylines")), QString());
      const int visibleCount = static_cast<int>(parts.size()) - visibleFirst;
      const int hiddenFirst = static_cast<int>(parts.size());
      if (withHidden) appendPolys(v.value(QStringLiteral("hidden")), QStringLiteral("hidden"));
      const int hiddenCount = static_cast<int>(parts.size()) - hiddenFirst;
      const int dimFirst = static_cast<int>(parts.size());

      QJsonArray srcIds;
      if (withDims) {
        for (const QJsonValue& dv : v.value(QStringLiteral("dimensions")).toArray()) {
          const QJsonObject d = dv.toObject();
          if (d.value(QStringLiteral("sourceId")).toString().isEmpty()) continue;
          parts.push_back(dimToElement(d));
          srcIds << d.value(QStringLiteral("sourceId")).toString();
        }
      }
      viewCounts.insert(QStringLiteral("visibleFirst"), visibleFirst);
      viewCounts.insert(QStringLiteral("visibleCount"), visibleCount);
      viewCounts.insert(QStringLiteral("hiddenFirst"), hiddenFirst);
      viewCounts.insert(QStringLiteral("hiddenCount"), hiddenCount);
      viewCounts.insert(QStringLiteral("dimFirst"), dimFirst);
      binding.insert(QStringLiteral("_counts"), viewCounts);
      binding.insert(QStringLiteral("dimSourceIds"), srcIds);
      bindings.insert(QString::number(vi), binding);
    }

    if (parts.empty()) {
      setError(QStringLiteral("三视图中性数据没有可用几何"));
      return OMBD_SK_ERR_INVALID_ARG;
    }

    model.beginEdit(QStringLiteral("ombd:three-views"));
    const std::vector<int> ids = model.addElementsQuiet(parts);

    QJsonObject finalBindings;
    for (int vi = 0; vi < views.size(); ++vi) {
      const QString key = QString::number(vi);
      if (!bindings.contains(key)) continue;
      QJsonObject b = bindings.value(key).toObject();
      const QJsonObject counts = b.value(QStringLiteral("_counts")).toObject();
      b.remove(QStringLiteral("_counts"));
      QJsonArray allIds;
      auto appendIds = [&](int first, int count) {
        for (int i = 0; i < count && first + i < static_cast<int>(ids.size()); ++i)
          allIds << ids[static_cast<size_t>(first + i)];
      };
      appendIds(counts.value(QStringLiteral("visibleFirst")).toInt(),
                counts.value(QStringLiteral("visibleCount")).toInt());
      appendIds(counts.value(QStringLiteral("hiddenFirst")).toInt(),
                withHidden ? counts.value(QStringLiteral("hiddenCount")).toInt() : 0);
      appendIds(counts.value(QStringLiteral("dimFirst")).toInt(),
                b.value(QStringLiteral("dimSourceIds")).toArray().size());
      b.insert(QStringLiteral("elementIds"), allIds);
      finalBindings.insert(key, b);
    }

    QJsonObject meta;
    meta.insert(QStringLiteral("schema"), 1);
    meta.insert(QStringLiteral("frontDir"), vecJsonString(root.value(QStringLiteral("frontDir"))));
    meta.insert(QStringLiteral("frontUp"), vecJsonString(root.value(QStringLiteral("frontUp"))));
    meta.insert(QStringLiteral("scale"), root.value(QStringLiteral("scale")).toDouble(1.0));
    meta.insert(QStringLiteral("projection"), root.value(QStringLiteral("projection")).toString());
    meta.insert(QStringLiteral("stateHash"), root.value(QStringLiteral("stateHash")).toString());
    meta.insert(QStringLiteral("viewBindings"), finalBindings);

    ::SketchCAD::SheetSpec sheet = ::SketchCAD::SheetSpec::make(
        ::SketchCAD::SheetFormat::A3, true);
    sheet.projection = root.value(QStringLiteral("projection")).toString() == QStringLiteral("Third")
        ? ::SketchCAD::ProjectionMethod::ThirdAngle
        : ::SketchCAD::ProjectionMethod::FirstAngle;
    model.setSheetSpec(sheet);
    model.setDrawingProp(QString::fromLatin1(kBridgePropsKey),
                         QString::fromUtf8(QJsonDocument(meta).toJson(QJsonDocument::Compact)));
    return writeBuffer(model.toDrawingBytes(), outBytes, outLen);
  } catch (const std::exception& e) {
    setError(QString::fromUtf8(e.what()));
    return OMBD_SK_ERR_INTERNAL;
  } catch (...) {
    setError(QStringLiteral("build_three_views 未预期异常"));
    return OMBD_SK_ERR_INTERNAL;
  }
}

int32_t ombd_sketchcad_exchange_drawing_plan(
    const uint8_t* nativeJson, int32_t len, const char* sourceDrawingId,
    double maxDeviationMm, uint8_t** outBytes, int32_t* outLen) {
  clearError();
  if (outBytes) *outBytes = nullptr;
  if (outLen) *outLen = 0;
  QString err;
  const QJsonObject root = objectFromBytes(nativeJson, len, &err);
  if (root.isEmpty()) {
    setError(err);
    return OMBD_SK_ERR_INVALID_ARG;
  }

  try {
    const QByteArray payload(reinterpret_cast<const char*>(nativeJson), len);
    ::SketchCAD::SketchModel model;
    QString modelErr;
    if (!model.applyDrawingJson(payload, &modelErr)) {
      setError(QStringLiteral("图纸 JSON 导入失败：%1").arg(modelErr));
      return OMBD_SK_ERR_INTERNAL;
    }

    const double dev = maxDeviationMm > 0.0 ? maxDeviationMm : 0.01;
    std::vector<ScElement> loaded;
    const QJsonArray elemsJson = root.value(QStringLiteral("elements")).toArray();
    loaded.reserve(static_cast<size_t>(elemsJson.size()));
    for (const QJsonValue& v : elemsJson)
      loaded.push_back(::SketchCAD::elementFromJson(v.toObject()));
    int skipped = 0, fixed = 0;
    QString guardReport;
    const std::vector<ScElement> source = ::SketchCAD::filterElements(
        loaded, &skipped, &fixed, &guardReport);

    QJsonArray geometry;
    QJsonArray metadata;
    for (const ScElement& e : source) {
      using T = ::SketchCAD::ElementType;
      auto addMeta = [&]() {
        QJsonObject m;
        m.insert(QStringLiteral("sourceElementId"), e.id);
        m.insert(QStringLiteral("sourceElementType"), scTypeName(e.type));
        m.insert(QStringLiteral("name"), e.name);
        m.insert(QStringLiteral("data"), elementMetaJson(e));
        metadata << m;
      };
      switch (e.type) {
        case T::Line:
        case T::Xline: {
          if (e.pts.size() < 2) { addMeta(); break; }
          QJsonObject p;
          p.insert(QStringLiteral("x1"), e.pts[0].x);
          p.insert(QStringLiteral("y1"), e.pts[0].y);
          p.insert(QStringLiteral("x2"), e.pts[1].x);
          p.insert(QStringLiteral("y2"), e.pts[1].y);
          geometry << geometryJson(QStringLiteral("line"), p, e, QStringLiteral("direct"), 0.0);
          break;
        }
        case T::Circle: {
          if (e.pts.empty() || !(e.radius > 0.0)) { addMeta(); break; }
          QJsonObject p;
          p.insert(QStringLiteral("cx"), e.pts[0].x);
          p.insert(QStringLiteral("cy"), e.pts[0].y);
          p.insert(QStringLiteral("r"), e.radius);
          geometry << geometryJson(QStringLiteral("circle"), p, e, QStringLiteral("direct"), 0.0);
          break;
        }
        case T::Rectangle: {
          if (e.pts.size() < 2) { addMeta(); break; }
          QJsonObject p;
          p.insert(QStringLiteral("x1"), e.pts[0].x);
          p.insert(QStringLiteral("y1"), e.pts[0].y);
          p.insert(QStringLiteral("x2"), e.pts[1].x);
          p.insert(QStringLiteral("y2"), e.pts[1].y);
          geometry << geometryJson(QStringLiteral("rectangle"), p, e, QStringLiteral("direct"), 0.0);
          break;
        }
        case T::Polyline:
        case T::Polygon:
        case T::Boundary: {
          QJsonObject p;
          p.insert(QStringLiteral("pts"), pointsToJson(e.pts));
          p.insert(QStringLiteral("closed"), e.closed);
          geometry << geometryJson(QStringLiteral("polyline"), p, e, QStringLiteral("direct"), 0.0);
          break;
        }
        case T::Arc: {
          if (e.pts.empty() || !(e.radius > 0.0)) { addMeta(); break; }
          QJsonObject p;
          p.insert(QStringLiteral("cx"), e.pts[0].x);
          p.insert(QStringLiteral("cy"), e.pts[0].y);
          p.insert(QStringLiteral("r"), e.radius);
          p.insert(QStringLiteral("startAngleRad"), e.startAngle * kPi / 180.0);
          p.insert(QStringLiteral("endAngleRad"), e.endAngle * kPi / 180.0);
          geometry << geometryJson(QStringLiteral("arc"), p, e, QStringLiteral("native-arc"), 0.0);
          break;
        }
        case T::Ellipse: {
          if (e.pts.empty() || !(e.radius > 0.0)) { addMeta(); break; }
          const auto pts = sampleEllipseByDeviation(e.pts[0], e.radius,
                                                    e.radiusY > 0.0 ? e.radiusY : e.radius,
                                                    e.rotation, dev);
          QJsonObject p;
          p.insert(QStringLiteral("pts"), pointsToJson(pts));
          p.insert(QStringLiteral("closed"), true);
          geometry << geometryJson(QStringLiteral("polyline"), p, e,
                                   QStringLiteral("polyline-approx"), dev);
          break;
        }
        case T::Spline: {
          if (e.pts.size() < 2) { addMeta(); break; }
          const auto pts = sampleSplineByDeviation(e.pts, dev);
          QJsonObject p;
          p.insert(QStringLiteral("pts"), pointsToJson(pts));
          p.insert(QStringLiteral("closed"), false);
          geometry << geometryJson(QStringLiteral("polyline"), p, e,
                                   QStringLiteral("polyline-approx"), dev);
          break;
        }
        default:
          addMeta();
          break;
      }
    }

    // 即使没有几何也返回计划：元数据仍须交给宿主写入结果，
    // 由宿主明确失败（“没有可转换为 3D 草图的几何图元”），不静默丢弃非几何图元。
    QJsonObject plan;
    plan.insert(QStringLiteral("schema"), 1);
    plan.insert(QStringLiteral("sourceDrawingId"),
                QString::fromUtf8(sourceDrawingId ? sourceDrawingId : ""));
    plan.insert(QStringLiteral("maxDeviationMm"), dev);
    plan.insert(QStringLiteral("geometry"), geometry);
    plan.insert(QStringLiteral("metadata"), metadata);
    return writeBuffer(QJsonDocument(plan).toJson(QJsonDocument::Compact), outBytes, outLen);
  } catch (const std::exception& e) {
    setError(QString::fromUtf8(e.what()));
    return OMBD_SK_ERR_INTERNAL;
  } catch (...) {
    setError(QStringLiteral("drawing_plan 未预期异常"));
    return OMBD_SK_ERR_INTERNAL;
  }
}

int32_t ombd_sketchcad_exchange_native_from_sketch(
    const uint8_t* sketchJson, int32_t len, uint8_t** outBytes, int32_t* outLen) {
  clearError();
  if (outBytes) *outBytes = nullptr;
  if (outLen) *outLen = 0;
  QString err;
  const QJsonObject root = objectFromBytes(sketchJson, len, &err);
  if (root.isEmpty()) {
    setError(err);
    return OMBD_SK_ERR_INVALID_ARG;
  }
  try {
    std::vector<ScElement> parts;
    for (const QJsonValue& v : root.value(QStringLiteral("elements")).toArray()) {
      const QJsonObject s = v.toObject();
      const QString kind = s.value(QStringLiteral("kind")).toString();
      ScElement e;
      e.layer = QStringLiteral("ombd.geometry");
      if (kind == QStringLiteral("line")) {
        e.type = ::SketchCAD::ElementType::Line;
        e.pts = { ScVec(s.value(QStringLiteral("x1")).toDouble(),
                        s.value(QStringLiteral("y1")).toDouble(), 0.0),
                  ScVec(s.value(QStringLiteral("x2")).toDouble(),
                        s.value(QStringLiteral("y2")).toDouble(), 0.0) };
      } else if (kind == QStringLiteral("circle")) {
        e.type = ::SketchCAD::ElementType::Circle;
        e.pts = { ScVec(s.value(QStringLiteral("cx")).toDouble(),
                        s.value(QStringLiteral("cy")).toDouble(), 0.0) };
        e.radius = s.value(QStringLiteral("r")).toDouble();
      } else if (kind == QStringLiteral("rectangle")) {
        e.type = ::SketchCAD::ElementType::Rectangle;
        e.pts = { ScVec(s.value(QStringLiteral("x1")).toDouble(),
                        s.value(QStringLiteral("y1")).toDouble(), 0.0),
                  ScVec(s.value(QStringLiteral("x2")).toDouble(),
                        s.value(QStringLiteral("y2")).toDouble(), 0.0) };
      } else if (kind == QStringLiteral("polyline") || kind == QStringLiteral("polygon")) {
        e.type = ::SketchCAD::ElementType::Polyline;
        e.closed = kind == QStringLiteral("polygon") || s.value(QStringLiteral("closed")).toBool();
        e.pts = pointsFromJson(s.value(QStringLiteral("pts")));
      } else if (kind == QStringLiteral("point")) {
        e.type = ::SketchCAD::ElementType::Point;
        const std::vector<ScVec> p = pointsFromJson(s.value(QStringLiteral("pts")));
        if (p.empty()) continue;
        e.pts = { p.front() };
      } else {
        const std::vector<ScVec> p = pointsFromJson(s.value(QStringLiteral("pts")));
        if (p.size() < 2) continue;
        e.type = ::SketchCAD::ElementType::Polyline;
        e.pts = p;
      }
      parts.push_back(std::move(e));
    }
    if (parts.empty()) {
      setError(QStringLiteral("Sketch 中性数据没有可写出图元"));
      return OMBD_SK_ERR_INVALID_ARG;
    }
    ::SketchCAD::SketchModel model;
    model.beginEdit(QStringLiteral("ombd:from-sketch"));
    model.addElementsQuiet(parts);
    model.setSheetSpec(::SketchCAD::SheetSpec::make(::SketchCAD::SheetFormat::A3, true));
    return writeBuffer(model.toDrawingBytes(), outBytes, outLen);
  } catch (const std::exception& e) {
    setError(QString::fromUtf8(e.what()));
    return OMBD_SK_ERR_INTERNAL;
  } catch (...) {
    setError(QStringLiteral("native_from_sketch 未预期异常"));
    return OMBD_SK_ERR_INTERNAL;
  }
}

int32_t ombd_sketchcad_exchange_to_dxf(
    const uint8_t* nativeJson, int32_t len, uint8_t** outBytes, int32_t* outLen) {
  clearError();
  if (outBytes) *outBytes = nullptr;
  if (outLen) *outLen = 0;
  QString err;
  const QJsonObject root = objectFromBytes(nativeJson, len, &err);
  if (root.isEmpty()) {
    setError(err);
    return OMBD_SK_ERR_INVALID_ARG;
  }
  try {
    const QByteArray payload(reinterpret_cast<const char*>(nativeJson), len);
    ::SketchCAD::SketchModel model;
    QString modelErr;
    if (!model.applyDrawingJson(payload, &modelErr)) {
      setError(QStringLiteral("DXF 导出失败：%1").arg(modelErr));
      return OMBD_SK_ERR_INTERNAL;
    }
    QString dxfErr;
    const QByteArray dxf = ::SketchCAD::cadfmt::toDxf(model.elements(), &dxfErr);
    if (dxf.isEmpty()) {
      setError(QStringLiteral("DXF 导出失败：%1")
                   .arg(dxfErr.isEmpty() ? QStringLiteral("未知错误") : dxfErr));
      return OMBD_SK_ERR_INTERNAL;
    }
    return writeBuffer(dxf, outBytes, outLen);
  } catch (...) {
    setError(QStringLiteral("to_dxf 未预期异常"));
    return OMBD_SK_ERR_INTERNAL;
  }
}

int32_t ombd_sketchcad_exchange_from_dxf(
    const uint8_t* dxf, int32_t len, uint8_t** outBytes, int32_t* outLen) {
  clearError();
  if (outBytes) *outBytes = nullptr;
  if (outLen) *outLen = 0;
  if (!dxf || len <= 0) {
    setError(QStringLiteral("DXF 输入为空"));
    return OMBD_SK_ERR_INVALID_ARG;
  }
  try {
    std::vector<ScElement> elems;
    QString dxfErr;
    const QByteArray payload(reinterpret_cast<const char*>(dxf), len);
    if (!::SketchCAD::cadfmt::fromDxf(payload, elems, &dxfErr) || elems.empty()) {
      setError(QStringLiteral("DXF 回读失败：%1")
                   .arg(dxfErr.isEmpty() ? QStringLiteral("未得到图元") : dxfErr));
      return OMBD_SK_ERR_INTERNAL;
    }
    ::SketchCAD::SketchModel model;
    model.beginEdit(QStringLiteral("ombd:from-dxf"));
    model.addElementsQuiet(elems);
    model.setSheetSpec(::SketchCAD::SheetSpec::make(::SketchCAD::SheetFormat::A3, true));
    return writeBuffer(model.toDrawingBytes(), outBytes, outLen);
  } catch (...) {
    setError(QStringLiteral("from_dxf 未预期异常"));
    return OMBD_SK_ERR_INTERNAL;
  }
}

} // extern "C"
