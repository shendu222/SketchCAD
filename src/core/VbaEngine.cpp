// 仅在开启 VBA 开关且为 Windows 平台时编译（默认 OFF，不进入 CORE_SOURCES）。
#if defined(SCRIPTHOST_ENABLE_VBA) && defined(Q_OS_WIN)

#include "VbaEngine.h"
#include "ScriptApi.h"

#include <Windows.h>
#include <activscp.h>
#include <string>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

namespace SketchCAD {

// ---------------- VARIANT 辅助转换 ----------------
static bool vtDouble(const VARIANT& v, double& out) {
    if (v.vt == VT_R8) { out = v.dblVal; return true; }
    VARIANT t; VariantInit(&t);
    if (SUCCEEDED(VariantChangeType(&t, const_cast<VARIANT*>(&v), 0, VT_R8))) {
        out = t.dblVal; return true;
    }
    return false;
}
static bool vtInt(const VARIANT& v, int& out) {
    if (v.vt == VT_I4) { out = v.lVal; return true; }
    VARIANT t; VariantInit(&t);
    if (SUCCEEDED(VariantChangeType(&t, const_cast<VARIANT*>(&v), 0, VT_I4))) {
        out = t.lVal; return true;
    }
    return false;
}
static bool vtBool(const VARIANT& v, bool& out) {
    if (v.vt == VT_BOOL) { out = (v.boolVal != VARIANT_FALSE); return true; }
    VARIANT t; VariantInit(&t);
    if (SUCCEEDED(VariantChangeType(&t, const_cast<VARIANT*>(&v), 0, VT_BOOL))) {
        out = (t.boolVal != VARIANT_FALSE); return true;
    }
    return false;
}
static QString vtStr(const VARIANT& v) {
    if (v.vt == VT_BSTR && v.bstrVal) return QString::fromWCharArray(v.bstrVal);
    VARIANT t; VariantInit(&t);
    if (SUCCEEDED(VariantChangeType(&t, const_cast<VARIANT*>(&v), 0, VT_BSTR))) {
        QString s = t.bstrVal ? QString::fromWCharArray(t.bstrVal) : QString();
        VariantClear(&t);
        return s;
    }
    return QString();
}
static BSTR toBstr(const QString& s) { return SysAllocString((const OLECHAR*)s.utf16()); }

// ---------------- ScriptApi 的 IDispatch 包装 ----------------
// 把 ScriptApi 的 Q_INVOKABLE 方法映射为 COM 方法，供 VBScript 以 App.XXX(...) 调用。
// 方法分派基于 dispid（1..12），参数按 IDispatch 约定逆序存放（rgvarg 右端为第一个实参）。
class VbaAppDispatch : public IDispatch {
public:
    explicit VbaAppDispatch(ScriptApi* api) : api_(api), ref_(1) {}

    // IUnknown
    ULONG STDMETHODCALLTYPE AddRef() override { return ++ref_; }
    ULONG STDMETHODCALLTYPE Release() override {
        if (--ref_ == 0) { delete this; return 0; }
        return ref_;
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** pp) override {
        if (riid == IID_IUnknown || riid == IID_IDispatch) {
            *pp = static_cast<IDispatch*>(this); AddRef(); return S_OK;
        }
        *pp = nullptr; return E_NOINTERFACE;
    }

    // IDispatch
    HRESULT STDMETHODCALLTYPE GetTypeInfoCount(UINT* p) override { *p = 0; return S_OK; }
    HRESULT STDMETHODCALLTYPE GetTypeInfo(UINT, LCID, ITypeInfo**) override { return E_NOTIMPL; }

    HRESULT STDMETHODCALLTYPE GetIDsOfNames(REFIID, LPOLESTR* names, UINT n, LCID,
                                            DISPID* ids) override {
        // 方法名 → dispid（大小写不敏感）
        static const struct { const wchar_t* name; DISPID id; } kMap[] = {
            {L"addLine", 1}, {L"addCircle", 2}, {L"addArc", 3}, {L"addRect", 4},
            {L"addPoint", 5}, {L"addText", 6}, {L"elementCount", 7}, {L"elementInfo", 8},
            {L"removeElement", 9}, {L"run", 10}, {L"log", 11}, {L"version", 12},
        };
        bool allKnown = true;
        for (UINT i = 0; i < n; ++i) {
            DISPID found = DISPID_UNKNOWN;
            for (auto& m : kMap) {
                if (_wcsicmp(names[i], m.name) == 0) { found = m.id; break; }
            }
            ids[i] = found;
            if (found == DISPID_UNKNOWN) allKnown = false;
        }
        return allKnown ? S_OK : DISP_E_UNKNOWNNAME;
    }

    HRESULT STDMETHODCALLTYPE Invoke(DISPID id, REFIID, LCID, WORD flags,
                                     DISPPARAMS* dp, VARIANT* result,
                                     EXCEPINFO*, UINT* errArg) override {
        if (result) VariantInit(result);
        auto arg = [&](int idx, const VARIANT*& out) -> bool {
            if (!dp || idx < 0 || idx >= (int)dp->cArgs) return false;
            out = &dp->rgvarg[dp->cArgs - 1 - idx];   // 逆序
            return true;
        };
        const VARIANT* a;
        switch (id) {
        case 1: { double x1,y1,x2,y2;
            if (!arg(0,a)||!vtDouble(*a,x1)||!arg(1,a)||!vtDouble(*a,y1)||
                !arg(2,a)||!vtDouble(*a,x2)||!arg(3,a)||!vtDouble(*a,y2)) return E_INVALIDARG;
            if (result){ result->vt=VT_I4; result->lVal=api_->addLine(x1,y1,x2,y2);} return S_OK; }
        case 2: { double cx,cy,r;
            if (!arg(0,a)||!vtDouble(*a,cx)||!arg(1,a)||!vtDouble(*a,cy)||!arg(2,a)||!vtDouble(*a,r)) return E_INVALIDARG;
            if (result){ result->vt=VT_I4; result->lVal=api_->addCircle(cx,cy,r);} return S_OK; }
        case 3: { double cx,cy,r,s,e;
            if (!arg(0,a)||!vtDouble(*a,cx)||!arg(1,a)||!vtDouble(*a,cy)||!arg(2,a)||!vtDouble(*a,r)||
                !arg(3,a)||!vtDouble(*a,s)||!arg(4,a)||!vtDouble(*a,e)) return E_INVALIDARG;
            if (result){ result->vt=VT_I4; result->lVal=api_->addArc(cx,cy,r,s,e);} return S_OK; }
        case 4: { double x,y,w,h;
            if (!arg(0,a)||!vtDouble(*a,x)||!arg(1,a)||!vtDouble(*a,y)||!arg(2,a)||!vtDouble(*a,w)||!arg(3,a)||!vtDouble(*a,h)) return E_INVALIDARG;
            if (result){ result->vt=VT_I4; result->lVal=api_->addRect(x,y,w,h);} return S_OK; }
        case 5: { double x,y;
            if (!arg(0,a)||!vtDouble(*a,x)||!arg(1,a)||!vtDouble(*a,y)) return E_INVALIDARG;
            if (result){ result->vt=VT_I4; result->lVal=api_->addPoint(x,y);} return S_OK; }
        case 6: { double x,y,h; QString t;
            if (!arg(0,a)||!vtDouble(*a,x)||!arg(1,a)||!vtDouble(*a,y)||!arg(2,a)||!(t=vtStr(*a),true)||!arg(3,a)||!vtDouble(*a,h)) return E_INVALIDARG;
            if (result){ result->vt=VT_I4; result->lVal=api_->addText(x,y,t,h);} return S_OK; }
        case 7:
            if (result){ result->vt=VT_I4; result->lVal=api_->elementCount();} return S_OK;
        case 8: { int idv;
            if (!arg(0,a)||!vtInt(*a,idv)) return E_INVALIDARG;
            if (result){ result->vt=VT_BSTR; result->bstrVal=toBstr(api_->elementInfo(idv)); } return S_OK; }
        case 9: { int idv;
            if (!arg(0,a)||!vtInt(*a,idv)) return E_INVALIDARG;
            if (result){ result->vt=VT_BOOL; result->boolVal=api_->removeElement(idv)?VARIANT_TRUE:VARIANT_FALSE; } return S_OK; }
        case 10: { QString c;
            if (!arg(0,a)||!(c=vtStr(*a),true)) return E_INVALIDARG;
            if (result){ result->vt=VT_BOOL; result->boolVal=api_->run(c)?VARIANT_TRUE:VARIANT_FALSE; } return S_OK; }
        case 11: { QString m;
            if (!arg(0,a)||!(m=vtStr(*a),true)) return E_INVALIDARG;
            api_->log(m); return S_OK; }
        case 12:
            if (result){ result->vt=VT_BSTR; result->bstrVal=toBstr(api_->version()); } return S_OK;
        }
        if (errArg) *errArg = 0;
        return DISP_E_MEMBERNOTFOUND;
    }

private:
    ScriptApi* api_;
    LONG ref_;
};

// ---------------- 脚本站点（捕获错误） ----------------
class VbaScriptSite : public IActiveScriptSite {
public:
    VbaScriptSite() : ref_(1), appDisp_(nullptr) {}
    void setAppDispatch(IDispatch* d) { appDisp_ = d; }
    QString errorText() const { return error_; }

    ULONG STDMETHODCALLTYPE AddRef() override { return ++ref_; }
    ULONG STDMETHODCALLTYPE Release() override {
        if (--ref_ == 0) { delete this; return 0; } return ref_;
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** pp) override {
        if (riid == IID_IUnknown || riid == IID_IActiveScriptSite) {
            *pp = static_cast<IActiveScriptSite*>(this); AddRef(); return S_OK;
        }
        *pp = nullptr; return E_NOINTERFACE;
    }
    HRESULT STDMETHODCALLTYPE GetLCID(LCID* plcid) override { *plcid = LOCALE_USER_DEFAULT; return S_OK; }
    HRESULT STDMETHODCALLTYPE GetItemInfo(LPCOLESTR name, DWORD mask,
                                          IUnknown** ppUnk, ITypeInfo** ppType) override {
        if (ppType) *ppType = nullptr;
        if (!(mask & SCRIPTINFO_IUNKNOWN) || !ppUnk) return E_INVALIDARG;
        if (appDisp_ && wcscmp(name, L"App") == 0) {
            *ppUnk = static_cast<IDispatch*>(appDisp_);
            appDisp_->AddRef();
            return S_OK;
        }
        *ppUnk = nullptr; return TYPE_E_ELEMENTNOTFOUND;
    }
    HRESULT STDMETHODCALLTYPE GetDocVersionString(BSTR* p) override {
        if (p) *p = SysAllocString(L"1.0"); return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnScriptTerminate(const VARIANT*, const EXCEPINFO*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnStateChange(SCRIPTSTATE) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnScriptError(IActiveScriptError* err) override {
        BSTR desc = nullptr;
        if (err && SUCCEEDED(err->GetDescription(&desc)) && desc) {
            error_ = QString::fromWCharArray(desc);
            SysFreeString(desc);
        } else {
            error_ = QStringLiteral("VBA 脚本执行错误");
        }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnEnterScript() override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnLeaveScript() override { return S_OK; }

private:
    LONG ref_;
    IDispatch* appDisp_;
    QString error_;
};

bool VbaEngine::available() const {
    // 本文件仅在 Windows + 开启开关时编译；运行时需系统 VBScript 引擎可见。
    CLSID clsid;
    if (SUCCEEDED(CLSIDFromProgID(L"VBScript", &clsid))) return true;
    return false;
}

ScriptResult VbaEngine::execute(const QString& code, ScriptApi* api,
                                std::function<void(const QString&)> logSink) {
    ScriptResult res;
    res.language = language();
    if (!api) { res.error = QStringLiteral("内部交互面未初始化"); return res; }

    HRESULT hrCo = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool needUninit = SUCCEEDED(hrCo);

    // 捕获脚本输出（App.Log 经 ScriptApi::log 发射 scriptOutput）
    QMetaObject::Connection conn = QObject::connect(api, &ScriptApi::scriptOutput, api,
        [&](const QString& s) { res.output += s + "\n"; if (logSink) logSink(s); });

    CLSID clsid;
    if (FAILED(CLSIDFromProgID(L"VBScript", &clsid))) {
        res.error = QStringLiteral("VBScript 引擎不可用");
        QObject::disconnect(conn);
        if (needUninit) CoUninitialize();
        return res;
    }

    IActiveScript* pScript = nullptr;
    if (FAILED(CoCreateInstance(clsid, nullptr, CLSCTX_INPROC_SERVER,
                                IID_IActiveScript, (void**)&pScript))) {
        res.error = QStringLiteral("无法创建 VBScript 引擎实例");
        QObject::disconnect(conn);
        if (needUninit) CoUninitialize();
        return res;
    }

    VbaScriptSite* site = new VbaScriptSite();
    VbaAppDispatch* appDisp = new VbaAppDispatch(api);
    site->setAppDispatch(static_cast<IDispatch*>(appDisp));

    bool ok = true;
    if (FAILED(pScript->SetScriptSite(site))) { res.error = QStringLiteral("SetScriptSite 失败"); ok = false; }

    if (ok && FAILED(pScript->AddNamedItem(L"App",
                SCRIPTITEM_ISVISIBLE | SCRIPTITEM_GLOBALMEMBERS | SCRIPTITEM_ISPERSISTENT))) {
        res.error = QStringLiteral("AddNamedItem(App) 失败"); ok = false;
    }

    IActiveScriptParse* pParse = nullptr;
    if (ok && FAILED(pScript->QueryInterface(IID_IActiveScriptParse, (void**)&pParse))) {
        res.error = QStringLiteral("获取 IActiveScriptParse 失败"); ok = false;
    }
    if (ok && FAILED(pParse->InitNew())) {
        res.error = QStringLiteral("脚本引擎初始化失败"); ok = false;
    }

    if (ok) {
        BSTR codeB = toBstr(code);
        HRESULT hr = pParse->ParseScriptText(codeB, nullptr, nullptr, nullptr,
                                             0, 0, SCRIPTTEXT_ISVISIBLE, nullptr, nullptr);
        SysFreeString(codeB);
        if (FAILED(hr)) {
            res.error = site->errorText().isEmpty() ? QStringLiteral("脚本解析失败") : site->errorText();
            ok = false;
        } else if (FAILED(pScript->SetScriptState(SCRIPTSTATE_RUNNING))) {
            res.error = site->errorText().isEmpty() ? QStringLiteral("脚本运行失败") : site->errorText();
            ok = false;
        } else {
            res.ok = true;
            if (!site->errorText().isEmpty()) res.error = site->errorText(); // 运行期警告/错误
        }
    }

    if (pParse) pParse->Release();
    pScript->Close();
    pScript->Release();
    appDisp->Release();
    site->Release();

    QObject::disconnect(conn);
    if (needUninit) CoUninitialize();
    return res;
}

} // namespace SketchCAD

#endif // defined(SCRIPTHOST_ENABLE_VBA) && defined(Q_OS_WIN)
