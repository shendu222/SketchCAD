// ============================================================================
// COM 自动化服务器 —— 实现（MinGW 环境：无 MIDL/typelib，手写 IDispatch 静态分派）
// ----------------------------------------------------------------------------
// 架构分层：
//   本文件只做「COM 语义 ↔ 引擎调用」的桥接：参数解包(VARIANT→QString)、
//   返回打包(QString→BSTR)、错误码映射、事件桥接(IDispatch sink)。
//   全部业务能力在 BatchPlotEngine（COM 无关），自测引擎层不经本文件。
//
// 后期绑定调用面（VBScript / PowerShell / Python win32com 均可用）：
//   GetIDsOfNames  ：静态名字表（见 kDispatchTable），O(1) 查找
//   Invoke         ：switch(dispid)，位置参数自右向左压栈（COM 约定：
//                    rgvarg[cArgs-1] 为第 1 个参数）
// ============================================================================
#include "SketchCADComServer.h"
#include "ComError.h"
#include "BatchPlotEngine.h"
#include "core/SketchModel.h"

#include <objbase.h>
#include <olectl.h>
#include <oleauto.h>

#include <QString>
#include <QStringList>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QFileInfo>

#include <atomic>
#include <memory>
#include <mutex>
#include <cwchar>

namespace SketchCAD {
namespace comauto {

// ---------------------------------------------------------------------------
// 进程级计数与模块句柄
// ---------------------------------------------------------------------------
static std::atomic<LONG> g_objCount{0};    // 存活的自动化对象数
static std::atomic<LONG> g_lockCount{0};   // LockServer 锁计数

static HINSTANCE g_hInst = nullptr;

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH)
        g_hInst = inst;
    return TRUE;
}

// ---------------------------------------------------------------------------
// BSTR / QString 互转
// ---------------------------------------------------------------------------
static BSTR qstringToBstr(const QString& s) {
    const std::wstring w = s.toStdWString();
    return SysAllocStringLen(w.c_str(), static_cast<UINT>(w.size()));
}

static QString bstrToQstring(BSTR b) {
    return b ? QString::fromWCharArray(b) : QString();
}

// ---------------------------------------------------------------------------
// 分派表：方法名 → dispid（GetIDsOfNames 查此表；Invoke switch 此枚举）
// ---------------------------------------------------------------------------
enum DispId {
    dispStart = 1, dispOpenDrawing, dispPlotToPdf, dispExportAs,
    dispBatchPlot, dispGetTaskStatus, dispRegisterCallback, dispQuit,
    dispVersion, dispLastError
};

struct DispatchEntry { const wchar_t* name; DispId id; };
static const DispatchEntry kDispatchTable[] = {
    { L"Start",           dispStart           },
    { L"OpenDrawing",     dispOpenDrawing     },
    { L"PlotToPdf",       dispPlotToPdf       },
    { L"ExportAs",        dispExportAs        },
    { L"BatchPlot",       dispBatchPlot       },
    { L"GetTaskStatus",   dispGetTaskStatus   },
    { L"RegisterCallback",dispRegisterCallback},
    { L"Quit",            dispQuit            },
    { L"Version",         dispVersion         },
    { L"LastError",       dispLastError       },
};

// ---------------------------------------------------------------------------
// 自动化对象：IDispatch 手写实现
// ---------------------------------------------------------------------------
class SketchCadAuto : public IDispatch {
public:
    SketchCadAuto() { ++g_objCount; }
    // 显式虚析构（MinGW 头文件的 IUnknown 无虚析构，不能写 override；
    // 虚析构只追加在本类 vtable 末尾，COM 客户端可见的 IUnknown/IDispatch 槽位不受影响）
    virtual ~SketchCadAuto() { --g_objCount; }

    // ---- IUnknown ----
    HRESULT __stdcall QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_IDispatch) {
            *ppv = static_cast<IDispatch*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    ULONG __stdcall AddRef() override { return ++m_ref; }
    ULONG __stdcall Release() override {
        const ULONG r = --m_ref;
        if (r == 0) delete this;
        return r;
    }

    // ---- IDispatch：无 typelib（GetTypeInfo 返回 E_NOTIMPL，后期绑定不受影响）----
    HRESULT __stdcall GetTypeInfoCount(UINT* pctinfo) override {
        if (pctinfo) *pctinfo = 0;
        return S_OK;
    }
    HRESULT __stdcall GetTypeInfo(UINT, LCID, ITypeInfo** ppInfo) override {
        if (ppInfo) *ppInfo = nullptr;
        return E_NOTIMPL;
    }
    HRESULT __stdcall GetIDsOfNames(REFIID, LPOLESTR* rgszNames, UINT cNames,
                                    LCID, DISPID* rgDispId) override {
        if (!rgszNames || !rgDispId || cNames == 0) return E_POINTER;
        HRESULT hr = S_OK;
        for (UINT i = 0; i < cNames; ++i) {
            const QString name = QString::fromWCharArray(rgszNames[i]);
            bool found = false;
            for (const auto& e : kDispatchTable) {
                if (name.compare(QString::fromWCharArray(e.name), Qt::CaseInsensitive) == 0) {
                    rgDispId[i] = e.id;
                    found = true;
                    break;
                }
            }
            if (!found) {
                rgDispId[i] = DISPID_UNKNOWN;
                hr = DISP_E_UNKNOWNNAME;
            }
        }
        return hr;
    }
    HRESULT __stdcall Invoke(DISPID dispIdMember, REFIID, LCID, WORD wFlags,
                             DISPPARAMS* pDispParams, VARIANT* pVarResult,
                             EXCEPINFO*, UINT* puArgErr) override {
        Q_UNUSED(wFlags)
        Q_UNUSED(puArgErr)
        // 递归互斥：批量执行期间引擎同步回调 fireSink（同线程），需可重入
        std::lock_guard<std::recursive_mutex> lk(m_mtx);
        HRESULT hr = SCAD_S_OK;
        QString retStr;      // 字符串返回值（方法返回值 / 属性 get）
        switch (dispIdMember) {
        case dispStart:           hr = onStart(retStr); break;
        case dispOpenDrawing:     hr = onOpenDrawing(pDispParams); break;
        case dispPlotToPdf:       hr = onExport(pDispParams, /*allowLayout=*/true); break;
        case dispExportAs:        hr = onExport(pDispParams, /*allowLayout=*/false); break;
        case dispBatchPlot:       hr = onBatchPlot(pDispParams, retStr); break;
        case dispGetTaskStatus:   hr = onGetTaskStatus(retStr); break;
        case dispRegisterCallback:hr = onRegisterCallback(pDispParams); break;
        case dispQuit:            hr = onQuit(); break;
        case dispVersion:         retStr = kVersionText; break;
        case dispLastError:       retStr = m_lastError; break;
        default:                  return DISP_E_MEMBERNOTFOUND;
        }
        if (pVarResult && SUCCEEDED(hr)) {
            VariantInit(pVarResult);
            pVarResult->vt = VT_BSTR;
            pVarResult->bstrVal = qstringToBstr(retStr);
        }
        return hr;
    }

private:
    static constexpr const char* kVersionText = "SketchCAD 1.0 (Automation 1.0)";

    // 递归互斥保护（STA 宿主下单线程，但批量执行中引擎同步回调 fireSink
    // 属同线程重入；多宿主线程并发调用也需要兜底串行化）
    std::recursive_mutex m_mtx;
    std::atomic<ULONG>   m_ref{1};
    bool                 m_started = false;
    QString              m_lastError;      // LastError 属性
    QString              m_lastBatch;      // 最近一次批量结果 JSON
    IDispatch*           m_sink = nullptr; // 事件宿主（调用方所有，此处 AddRef）
    std::unique_ptr<SketchModel> m_model;

    void setErr(HRESULT hr, const QString& detail = QString()) {
        m_lastError = comErrorText(hr);
        if (!detail.isEmpty()) m_lastError += QStringLiteral("：") + detail;
    }

    // 前置检查：必须先 Start()
    HRESULT requireStarted() {
        if (!m_started || !m_model) {
            setErr(SCAD_E_NOT_INITIALIZED);
            return SCAD_E_NOT_INITIALIZED;
        }
        return SCAD_S_OK;
    }

    // 取第 idx 个（1 起）位置参数的字符串；缺失返回空串并置 isAbsent
    QString strArg(DISPPARAMS* p, int idxFrom1, bool* isAbsent = nullptr) {
        if (isAbsent) *isAbsent = false;
        if (!p || (int)p->cArgs < idxFrom1) {
            if (isAbsent) *isAbsent = true;
            return QString();
        }
        const VARIANT& v = p->rgvarg[p->cArgs - idxFrom1];   // COM 约定：自右向左
        if (v.vt == VT_BSTR) return bstrToQstring(v.bstrVal);
        if (v.vt == (VT_VARIANT | VT_BYREF) && v.pvarVal)
            return v.pvarVal->vt == VT_BSTR ? bstrToQstring(v.pvarVal->bstrVal) : QString();
        return QString();
    }

    // ---- Start：初始化引擎（宿主无 Qt 时补建 QApplication）----
    HRESULT onStart(QString& versionOut) {
        std::lock_guard<std::recursive_mutex> lk(m_mtx);
        BatchPlotEngine::ensureQtApplication();
        if (!m_model)
            m_model = std::make_unique<SketchModel>();
        m_started = true;
        m_lastError.clear();
        versionOut = QString::fromLatin1(kVersionText);
        return SCAD_S_OK;
    }

    // ---- OpenDrawing(path) ----
    HRESULT onOpenDrawing(DISPPARAMS* p) {
        std::lock_guard<std::recursive_mutex> lk(m_mtx);
        if (HRESULT pre = requireStarted(); FAILED(pre)) return pre;
        const QString path = strArg(p, 1);
        if (path.isEmpty()) {
            setErr(SCAD_E_BAD_ARG, QStringLiteral("缺少 path 参数"));
            return SCAD_E_BAD_ARG;
        }
        if (!QFileInfo::exists(path)) {
            setErr(SCAD_E_FILE_NOT_FOUND, path);
            return SCAD_E_FILE_NOT_FOUND;
        }
        QString e;
        if (!BatchPlotEngine::loadDrawing(*m_model, path, &e)) {
            m_lastError = e;   // 引擎已带错误码前缀
            return SCAD_E_PARSE_FAILED;
        }
        m_lastError.clear();
        return SCAD_S_OK;
    }

    // ---- PlotToPdf(path[, layout]) / ExportAs(path) ----
    HRESULT onExport(DISPPARAMS* p, bool allowLayout) {
        std::lock_guard<std::recursive_mutex> lk(m_mtx);
        if (HRESULT pre = requireStarted(); FAILED(pre)) return pre;
        const QString path = strArg(p, 1);
        if (path.isEmpty()) {
            setErr(SCAD_E_BAD_ARG, QStringLiteral("缺少 path 参数"));
            return SCAD_E_BAD_ARG;
        }
        QJsonObject settings;
        if (allowLayout) {
            bool absent = false;
            const QString layoutJson = strArg(p, 2, &absent);
            if (!absent && !layoutJson.isEmpty()) {
                const QJsonDocument d = QJsonDocument::fromJson(layoutJson.toUtf8());
                if (d.isObject()) settings = d.object();
            }
        }
        QString e;
        if (!BatchPlotEngine::exportDrawing(*m_model, path, settings, &e)) {
            m_lastError = e;
            // 按输出类型映射错误码：pdf → 出图失败；其余 → 写盘失败
            return path.endsWith(QLatin1String(".pdf"), Qt::CaseInsensitive)
                ? SCAD_E_PLOT_FAILED : SCAD_E_WRITE_FAILED;
        }
        m_lastError.clear();
        return SCAD_S_OK;
    }

    // ---- BatchPlot(tasksJson[, logPath])：返回结果 JSON；部分失败返回 SCAD_S_PARTIAL ----
    HRESULT onBatchPlot(DISPPARAMS* p, QString& resultOut) {
        std::lock_guard<std::recursive_mutex> lk(m_mtx);
        if (HRESULT pre = requireStarted(); FAILED(pre)) return pre;
        const QString tasksJson = strArg(p, 1);
        const QString logPath   = strArg(p, 2);
        if (tasksJson.isEmpty()) {
            setErr(SCAD_E_BAD_TASK, QStringLiteral("缺少 tasks 参数"));
            return SCAD_E_BAD_TASK;
        }
        const BatchResult r = BatchPlotEngine::runBatch(tasksJson, logPath);
        m_lastBatch = QString::fromUtf8(
            QJsonDocument(r.toJson()).toJson(QJsonDocument::Compact));
        resultOut = m_lastBatch;
        if (r.allOk) {
            m_lastError.clear();
            return SCAD_S_OK;
        }
        m_lastError = comErrorText(SCAD_S_PARTIAL);
        return SCAD_S_PARTIAL;
    }

    // ---- GetTaskStatus：返回最近一次批量结果 JSON ----
    HRESULT onGetTaskStatus(QString& out) {
        std::lock_guard<std::recursive_mutex> lk(m_mtx);
        out = m_lastBatch;
        return SCAD_S_OK;
    }

    // ---- RegisterCallback(sink)：注册 IDispatch 事件宿主 ----
    HRESULT onRegisterCallback(DISPPARAMS* p) {
        std::lock_guard<std::recursive_mutex> lk(m_mtx);
        if (!p || (int)p->cArgs < 1 || p->rgvarg[p->cArgs - 1].vt != VT_DISPATCH
            || !p->rgvarg[p->cArgs - 1].pdispVal) {
            setErr(SCAD_E_BAD_ARG, QStringLiteral("sink 必须是 IDispatch 对象"));
            return SCAD_E_BAD_ARG;
        }
        if (m_sink) { m_sink->Release(); m_sink = nullptr; }
        m_sink = p->rgvarg[p->cArgs - 1].pdispVal;
        m_sink->AddRef();
        // 桥接引擎事件 → sink（捕获 this 安全性由 Quit/析构注销事件槽保证）
        SketchCadAuto* self = this;
        BatchPlotEngine::setEventSink(
            [self](const QString& event, const QJsonObject& payload) {
                self->fireSink(event, payload);
            });
        m_lastError.clear();
        return SCAD_S_OK;
    }

    // 事件桥接：按名解析宿主方法（OnTaskStart/OnTaskDone/OnBatchFinished），
    // 携带单个 JSON 字符串参数；宿主未实现该方法则静默跳过。
    void fireSink(const QString& event, const QJsonObject& payload) {
        IDispatch* sink = nullptr;
        {   std::lock_guard<std::recursive_mutex> lk(m_mtx);
            sink = m_sink;                       // 不 AddRef：fireSink 只在对象存活期内被调
        }
        if (!sink) return;
        try {
            wchar_t nameBuf[64] = {};
            const std::wstring wname = event.toStdWString();
            wcsncpy(nameBuf, wname.c_str(), 63);
            LPOLESTR names[1] = { nameBuf };
            DISPID id = DISPID_UNKNOWN;
            if (sink->GetIDsOfNames(IID_NULL, names, 1,
                                    LOCALE_USER_DEFAULT, &id) != S_OK) return;
            const QString json = QString::fromUtf8(
                QJsonDocument(payload).toJson(QJsonDocument::Compact));
            VARIANTARG arg;
            VariantInit(&arg);
            arg.vt = VT_BSTR;
            arg.bstrVal = qstringToBstr(json);
            DISPPARAMS dp = { &arg, nullptr, 1, 0 };
            VARIANT ret;
            VariantInit(&ret);
            EXCEPINFO ex;
            ZeroMemory(&ex, sizeof(ex));
            UINT err = 0;
            sink->Invoke(id, IID_NULL, LOCALE_USER_DEFAULT,
                         DISPATCH_METHOD, &dp, &ret, &ex, &err);
            VariantClear(&ret);
            VariantClear(&arg);
        } catch (...) {
            // 事件桥接绝不允许把异常抛回引擎执行流
        }
    }

    // ---- Quit：释放模型与事件宿主 ----
    HRESULT onQuit() {
        std::lock_guard<std::recursive_mutex> lk(m_mtx);
        BatchPlotEngine::setEventSink({});   // 先注销事件槽，杜绝悬空回调
        if (m_sink) { m_sink->Release(); m_sink = nullptr; }
        m_model.reset();
        m_started = false;
        m_lastBatch.clear();
        m_lastError.clear();
        return SCAD_S_OK;
    }
};

// ---------------------------------------------------------------------------
// 类厂
// ---------------------------------------------------------------------------
class SketchCadFactory : public IClassFactory {
public:
    HRESULT __stdcall QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_IClassFactory) {
            *ppv = static_cast<IClassFactory*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    ULONG __stdcall AddRef() override  { ++g_lockCount; return 2; }   // 类厂常驻，计锁
    ULONG __stdcall Release() override { --g_lockCount; return 1; }

    HRESULT __stdcall CreateInstance(IUnknown* punkOuter, REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (punkOuter) return CLASS_E_NOAGGREGATION;
        SketchCadAuto* obj = new (std::nothrow) SketchCadAuto();
        if (!obj) return E_OUTOFMEMORY;
        const HRESULT hr = obj->QueryInterface(riid, ppv);
        obj->Release();   // QI 成功时已替新持有者 AddRef
        return hr;
    }
    HRESULT __stdcall LockServer(BOOL fLock) override {
        fLock ? ++g_lockCount : --g_lockCount;
        return S_OK;
    }
};

// ---------------------------------------------------------------------------
// 注册表辅助
// ---------------------------------------------------------------------------
static QString clsidString() {
    OLECHAR buf[64] = {};
    StringFromGUID2(kCLSID_SketchCadAutomation, buf, 64);
    return QString::fromWCharArray(buf);
}

static bool writeRegValue(HKEY root, const QString& subKey,
                          const wchar_t* valueName, const QString& data) {
    HKEY k = nullptr;
    if (RegCreateKeyExW(root, subKey.toStdWString().c_str(), 0, nullptr,
                        REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr,
                        &k, nullptr) != ERROR_SUCCESS)
        return false;
    const std::wstring w = data.toStdWString();
    const BOOL ok = RegSetValueExW(k, valueName, 0, REG_SZ,
                                   reinterpret_cast<const BYTE*>(w.c_str()),
                                   static_cast<DWORD>((w.size() + 1) * sizeof(wchar_t)))
                    == ERROR_SUCCESS;
    RegCloseKey(k);
    return ok;
}

static void deleteRegTree(HKEY root, const QString& subKey) {
    RegDeleteTreeW(root, subKey.toStdWString().c_str());
}

} // namespace comauto
} // namespace SketchCAD

// ---------------------------------------------------------------------------
// DLL 标准导出
// ---------------------------------------------------------------------------
using namespace SketchCAD::comauto;

extern "C" {

HRESULT __stdcall DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    if (rclsid != kCLSID_SketchCadAutomation)
        return CLASS_E_CLASSNOTAVAILABLE;
    static SketchCadFactory factory;   // 类厂单例（进程级）
    return factory.QueryInterface(riid, ppv);
}

HRESULT __stdcall DllCanUnloadNow(void) {
    return (g_objCount.load() == 0 && g_lockCount.load() == 0) ? S_OK : S_FALSE;
}

HRESULT __stdcall DllRegisterServer(void) {
    if (!g_hInst) return SELFREG_E_CLASS;
    wchar_t dllPath[MAX_PATH] = {};
    if (GetModuleFileNameW(g_hInst, dllPath, MAX_PATH) == 0)
        return SELFREG_E_CLASS;
    const QString path = QString::fromWCharArray(dllPath);
    const QString clsid = clsidString();
    const QString clsidKey = QStringLiteral("CLSID\\") + clsid;
    bool ok = true;
    // 版本化 ProgID
    ok &= writeRegValue(HKEY_CLASSES_ROOT, QString::fromWCharArray(kProgIDVer),
                        nullptr, QString::fromWCharArray(kDescription));
    ok &= writeRegValue(HKEY_CLASSES_ROOT, QString::fromWCharArray(kProgIDVer),
                        L"CLSID", clsid);
    // 版本无关 ProgID → CurVer
    ok &= writeRegValue(HKEY_CLASSES_ROOT, QString::fromWCharArray(kProgID),
                        nullptr, QString::fromWCharArray(kDescription));
    ok &= writeRegValue(HKEY_CLASSES_ROOT, QString::fromWCharArray(kProgID),
                        L"CurVer", QString::fromWCharArray(kProgIDVer));
    // CLSID 注册：进程内服务器 + 线程模型
    ok &= writeRegValue(HKEY_CLASSES_ROOT, clsidKey, nullptr,
                        QString::fromWCharArray(kDescription));
    ok &= writeRegValue(HKEY_CLASSES_ROOT, clsidKey + QStringLiteral("\\InprocServer32"),
                        nullptr, path);
    ok &= writeRegValue(HKEY_CLASSES_ROOT, clsidKey + QStringLiteral("\\InprocServer32"),
                        L"ThreadingModel", QStringLiteral("Apartment"));
    ok &= writeRegValue(HKEY_CLASSES_ROOT, clsidKey + QStringLiteral("\\ProgID"),
                        nullptr, QString::fromWCharArray(kProgIDVer));
    ok &= writeRegValue(HKEY_CLASSES_ROOT, clsidKey + QStringLiteral("\\VersionIndependentProgID"),
                        nullptr, QString::fromWCharArray(kProgID));
    return ok ? S_OK : SELFREG_E_CLASS;
}

HRESULT __stdcall DllUnregisterServer(void) {
    const QString clsidKey = QStringLiteral("CLSID\\") + clsidString();
    deleteRegTree(HKEY_CLASSES_ROOT, clsidKey);
    deleteRegTree(HKEY_CLASSES_ROOT, QString::fromWCharArray(kProgID));
    deleteRegTree(HKEY_CLASSES_ROOT, QString::fromWCharArray(kProgIDVer));
    return S_OK;
}

} // extern "C"
