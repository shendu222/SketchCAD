#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <QSet>
#include "SketchCAD/i_plugin.h"
#include "SketchCAD/i_application.h"
#include "SdkContext.h"   // SdkApplication 以值成员持有，需完整类型（析构需要）

class QLibrary;   // 全局前向声明（Qt 的 QLibrary 在全局命名空间，勿写成 SketchCAD::QLibrary）

// ============================================================================
// 插件管理器（宿主侧，对标 ObjectARX acrxLoadModule / acedRegCmds）
// ----------------------------------------------------------------------------
// 负责插件的：
//   - 加载：经 QLibrary 动态加载 DLL/SO，解析 C 链接符号
//           sketchcad_sdk_abi_version（ABI 校验）与 sketchcad_create_plugin（建实例）
//   - 生命周期：initialize() → 运行期（命令回调）→ shutdown() → 卸载 DLL
//   - 注册表：维护已加载插件列表，去重、按路径卸载、整体卸载
//
// 设计要点：
//   - 仅依赖 SDK 公共头（纯接口 + POD），不感知任何插件内部结构，
//     因此第三方插件可完全独立于宿主二进制演进。
//   - ABI 不一致 / 符号缺失 / 初始化失败 一律拒绝并 emit pluginError，
//     不污染宿主运行。
//   - 全程 try/catch 包裹插件调用，防止插件异常击穿宿主进程。
// ============================================================================

namespace SketchCAD {

class SketchModel;
class CommandInterpreter;

class PluginManager : public QObject {
    Q_OBJECT
public:
    explicit PluginManager(SketchModel* model, CommandInterpreter* interp,
                           QObject* parent = nullptr);
    ~PluginManager() override;

    // 加载单个插件文件（.dll/.so/.dylib）。成功返回 true。
    bool loadPlugin(const QString& path);
    // 扫描目录，加载其中所有插件文件。返回成功加载的数量（失败项经 pluginError 上报）。
    int  loadDirectory(const QString& dir);
    // 卸载指定路径的插件。成功返回 true。
    bool unloadPlugin(const QString& path);
    // 卸载全部插件（析构时自动调用）。
    void unloadAll();

    // 已加载插件记录。
    struct LoadedPlugin {
        QString            path;                 // 插件文件完整路径（规范化）
        sdk::IPlugin*      instance = nullptr;   // 插件实例（宿主负责 delete）
        const sdk::PluginInfo* info = nullptr;   // 元信息（指向插件内部静态数据）
        bool               active   = false;    // initialize() 是否成功
        int                abi      = 0;        // 运行时协商确定的插件 ABI 代际
        QString            error;               // 加载失败原因（active==false 时有效）
        QLibrary*          lib = nullptr;        // 动态库句柄（内部持有，卸载时释放）
    };
    const QVector<LoadedPlugin>& loadedPlugins() const { return plugins_; }

signals:
    void pluginLoaded(const QString& name, const QString& path);
    void pluginUnloaded(const QString& name, const QString& path);
    void pluginError(const QString& path, const QString& msg);

private:
    SketchModel*      model_ = nullptr;
    CommandInterpreter* interp_ = nullptr;
    SdkApplication    app_;                  // 实现 IApplication，桥接 model_ + interp_
    QVector<LoadedPlugin> plugins_;
    QSet<QString>     loadedPaths_;          // 去重（规范化路径）
};

} // namespace SketchCAD
