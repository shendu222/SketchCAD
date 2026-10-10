#pragma once
#include <QString>
#include <QByteArray>

// =============================================================================
// DwgBridge —— DWG R2000+ 外部转换器隔离层（进程外桥接，纯 core，无 UI 依赖）
// =============================================================================
// 背景（D1 差距项）：SketchCAD 自研 DWG 写读仅覆盖 R12 (AC1009)；R13+ 的 DWG 是
// ODA 私有二进制、无公开规范。本层把 R2000+ 的读写委托给**独立的 LibreDWG
// 转换器 exe**（dwg2dxf / dwgwrite，GPLv3），经 QProcess 进程外调用。
//
// 合规要点（GPLv3 边界）：
//   * 转换器是**独立进程、独立 exe**，与本程序之间只通过"临时文件字节流 +
//     命令行参数"交换数据——不静态/动态链接、不共享地址空间、不 include 其
//     任何头文件。GPL 只覆盖转换器本体，SketchCAD 主程序零 GPL 代码。
//   * 转换器随安装包分发时须附 GPLv3 文本与源码获取途径（见第三方声明）。
//
// 鲁棒性要点（与项目崩溃不丢稿文化一致）：
//   * 转换器崩溃（坏文件/内存越界）只杀死转换器进程本身，主程序毫发无损，
//     由 QProcess::CrashExit 归一为明确错误报文（含版本串原文）。
//   * 所有调用带超时上限（默认 30s），超时 kill 并报错，杜绝主线程假死。
//   * 中转格式为 DXF：读 = dwg2dxf → 复用自有 cadfmt::fromDxf（R12/R2000 已
//     支持）；写 = cadfmt::toDxf(R2000) → dwgwrite --as r2000。零新增解析代码。
//
// LibreDWG 能力边界（如实声明，不虚标）：
//   * 读：R13(R1013) ~ R2018(R1032) 全系（本层路由 AC1012 及以后版本串）。
//   * 写：**仅到 R2000 (AC1015)**。R2004/R2007 写请求按"回落 R2000 并如实
//     报文说明"处理（详见 CadFormats::toDwg 注释与 docs/ 深思文档）。
//
// 转换器定位顺序（首个命中生效）：
//   1) 环境变量 SKETCHCAD_DWGTOOLS 指向的目录（开发/自测注入）；
//   2) 应用程序 exe 同目录的 tools/dwg/ 子目录（安装包布局）。
// 找不到 → available()==false，调用方报"未安装 DWG 转换组件"。
// =============================================================================
namespace SketchCAD {
namespace dwgbridge {

// 转换器是否可用（找到 dwg2dxf + dwgwrite 两个 exe 且可执行）。
// 纯探测，无副作用（不启动进程）。
bool available();

// 探测 DWG 字节流头部 6 字节版本串（"AC1009"/"AC1015"/"AC1021"…）。
// 文件过短或无版本串 → 空串。这是路由与错误报文共用的唯一探测入口。
QString versionFromBytes(const QByteArray& data);

// 版本串 → 人类可读名（复用 cadfmt::cadVerToHuman，保持报文一致）。
QString versionHuman(const QString& acadVer);

// 该版本串是否可经本桥读取（AC1012~AC1032；AC1009 不经过本桥——走自研 R12）。
bool canReadVersion(const QString& acadVer);

// ---- 读桥：任意支持版本 DWG → DXF 字节（交 cadfmt::fromDxf 回读）----
// 成功返回 true 且 dxfOut 填充；失败返回 false 并把原因写入 *err
//（错误报文一律含探测到的版本串原文，杜绝"不支持的版本"这类无诊断价值的提示）。
bool dwgToDxf(const QByteArray& dwgBytes, QByteArray& dxfOut, QString* err,
              int timeoutMs = 30000);

// ---- 写桥：DXF(R2000) 字节 → DWG R2000 字节 ----
// 输入应为 cadfmt::toDxf(..., DxfVersion::R2000) 的产物。
// 成功返回 true 且 dwgOut 填充（可被 AutoCAD 2000+ 直接打开）。
bool dxfToDwgR2000(const QByteArray& dxfBytes, QByteArray& dwgOut, QString* err,
                   int timeoutMs = 30000);

// 自测注入：覆写转换器目录（优先于环境变量/默认布局；空串=恢复默认查找）。
// 仅 SketchSelfTest 调用；进程级单例，无锁（自测单线程）。
void setToolsDirForTest(const QString& dir);

} // namespace dwgbridge
} // namespace SketchCAD
