#include "PluginManager.h"
#include "SdkContext.h"
#include "SketchModel.h"
#include "CommandInterpreter.h"
#include <QLibrary>
#include <QFileInfo>
#include <QDir>

namespace SketchCAD {

namespace {

// 规范化路径（解析符号链接 / 大小写），用于去重比对。
QString normalizePath(const QString& p) {
    return QFileInfo(p).canonicalFilePath();
}

// 受支持的插件文件后缀（跨平台）。
bool isPluginFile(const QString& path) {
    const QString suf = QFileInfo(path).suffix().toLower();
#ifdef Q_OS_WIN
    return suf == QStringLiteral("dll");
#else
    return suf == QStringLiteral("so") || suf == QStringLiteral("dylib");
#endif
}

} // namespace

PluginManager::PluginManager(SketchModel* model, CommandInterpreter* interp, QObject* parent)
    : QObject(parent), model_(model), interp_(interp), app_(model, interp) {}

PluginManager::~PluginManager() {
    unloadAll();
}

bool PluginManager::loadPlugin(const QString& path) {
    const QString norm = normalizePath(path);
    if (norm.isEmpty()) { emit pluginError(path, QStringLiteral("文件不存在")); return false; }
    if (!isPluginFile(norm)) {
        emit pluginError(path, QStringLiteral("不是受支持的插件文件（需 .dll/.so/.dylib）"));
        return false;
    }
    if (loadedPaths_.contains(norm)) { emit pluginError(path, QStringLiteral("插件已加载")); return false; }

    auto* lib = new QLibrary(norm);
    if (!lib->load()) {
        const QString msg = QStringLiteral("加载失败: ") + lib->errorString();
        delete lib;
        emit pluginError(path, msg);
        return false;
    }

    // ---- 1) 解析 ABI 版本符号，运行时协商二进制兼容 ----
    // 协商区间 [SDK_ABI_MIN_SUPPORTED, SDK_ABI_VERSION]：
    //   插件 ABI ≤ 宿主 ABI → 可接受（vtable 追加式演进保证旧代际槽位仍在）；
    //   插件 ABI >  宿主   → 拒绝（宿主无法凭空提供新代际的 vtable 槽位）；
    //   插件 ABI <  区间下限 → 拒绝（宿主不再保证该代际接口仍存在）。
    using AbiFn = int(*)();
    AbiFn abiFn = reinterpret_cast<AbiFn>(lib->resolve("sketchcad_sdk_abi_version"));
    if (!abiFn) {
        lib->unload(); delete lib;
        emit pluginError(path, QStringLiteral("缺少导出符号 sketchcad_sdk_abi_version"));
        return false;
    }
    const int abi = abiFn();
    if (abi > SKETCHCAD_SDK_ABI_VERSION) {
        lib->unload(); delete lib;
        emit pluginError(path, QStringLiteral("ABI 版本过新：插件=%1，宿主=%2（请升级宿主程序）")
                              .arg(abi).arg(SKETCHCAD_SDK_ABI_VERSION));
        return false;
    }
    if (abi < SKETCHCAD_SDK_ABI_MIN_SUPPORTED) {
        lib->unload(); delete lib;
        emit pluginError(path, QStringLiteral("ABI 版本过旧或非法：插件=%1（宿主支持区间 %2..%3）")
                              .arg(abi).arg(SKETCHCAD_SDK_ABI_MIN_SUPPORTED)
                              .arg(SKETCHCAD_SDK_ABI_VERSION));
        return false;
    }

    // ---- 2) 解析工厂符号，创建实例 ----
    using CreateFn = sdk::IPlugin*(*)();
    CreateFn createFn = reinterpret_cast<CreateFn>(lib->resolve("sketchcad_create_plugin"));
    if (!createFn) {
        lib->unload(); delete lib;
        emit pluginError(path, QStringLiteral("缺少导出符号 sketchcad_create_plugin"));
        return false;
    }
    sdk::IPlugin* plugin = nullptr;
    try { plugin = createFn(); } catch (...) { plugin = nullptr; }
    if (!plugin) {
        lib->unload(); delete lib;
        emit pluginError(path, QStringLiteral("sketchcad_create_plugin 返回空"));
        return false;
    }

    // ---- 3) 初始化（try/catch 防止插件异常击穿宿主）----
    const sdk::PluginInfo* info = plugin->info();
    bool ok = false;
    try { ok = plugin->initialize(&app_); } catch (...) { ok = false; }
    if (!ok) {
        try { plugin->shutdown(); } catch (...) {}
        try { plugin->dispose(); } catch (...) {}
        lib->unload(); delete lib;
        const QString name = info ? QString::fromUtf8(info->name) : QStringLiteral("?");
        emit pluginError(path, QStringLiteral("插件 %1 初始化失败").arg(name));
        return false;
    }

    // ---- 4) 登记 ----
    LoadedPlugin lp;
    lp.path    = norm;
    lp.instance = plugin;
    lp.info    = info;
    lp.active  = true;
    lp.abi     = abi;      // 协商结果随记录保存（供宿主 UI/日志展示）
    lp.lib     = lib;
    plugins_.append(lp);
    loadedPaths_.insert(norm);
    const QString name = info ? QString::fromUtf8(info->name) : QStringLiteral("?");
    if (abi < SKETCHCAD_SDK_ABI_VERSION) {
        // 兼容模式：插件基于较旧 ABI 构建；vtable 追加式演进承诺保证其槽位全部有效
        emit pluginLoaded(QStringLiteral("%1（兼容模式 ABI %2）").arg(name).arg(abi), norm);
    } else {
        emit pluginLoaded(name, norm);
    }
    return true;
}

int PluginManager::loadDirectory(const QString& dir) {
    const QDir d(dir);
    if (!d.exists()) return 0;
    int n = 0;
    for (const QFileInfo& fi : d.entryInfoList(QDir::Files | QDir::NoDotAndDotDot)) {
        if (isPluginFile(fi.filePath()) && loadPlugin(fi.filePath())) ++n;
    }
    return n;
}

bool PluginManager::unloadPlugin(const QString& path) {
    const QString norm = normalizePath(path);
    for (int i = 0; i < plugins_.size(); ++i) {
        if (plugins_[i].path == norm) {
            LoadedPlugin& lp = plugins_[i];
            const QString name = lp.info ? QString::fromUtf8(lp.info->name) : QStringLiteral("?");
            try { lp.instance->shutdown(); } catch (...) {}
            try { lp.instance->dispose(); } catch (...) {}
            if (lp.lib) { lp.lib->unload(); delete lp.lib; lp.lib = nullptr; }
            plugins_.removeAt(i);
            loadedPaths_.remove(norm);
            emit pluginUnloaded(name, norm);
            return true;
        }
    }
    return false;
}

void PluginManager::unloadAll() {
    // 逆序卸载（后加载的先释放），避免依赖顺序问题。
    for (int i = plugins_.size() - 1; i >= 0; --i) {
        LoadedPlugin& lp = plugins_[i];
        try { lp.instance->shutdown(); } catch (...) {}
        try { lp.instance->dispose(); } catch (...) {}
        if (lp.lib) { lp.lib->unload(); delete lp.lib; lp.lib = nullptr; }
    }
    plugins_.clear();
    loadedPaths_.clear();
}

} // namespace SketchCAD
