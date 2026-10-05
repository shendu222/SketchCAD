#pragma once
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QVector>

// ============================================================================
// DiagBundle：诊断包（一键打包"客户说崩了"→"发一个包即可复现"）。
//
// 设计要点：
// 1. 纯 core（无 QtWidgets），可无头直测；不依赖 SketchModel —— 图纸元信息由
//    调用方（MainWindow）组装为 QVariantMap 传入，"不含几何/文本内容"的边界
//    由调用方守卫，core 只负责打包。
// 2. ZIP 写入器自研零依赖：全 Stored（method=0，不压缩）。诊断文本/转储本身
//    压缩收益低，换取实现极简、结构可静态断言（本地头 + 中央目录 + EOCD +
//    CRC32，全部小端手写，UTF-8 名字标志 bit11）。
// 3. 包内容固定为"环境 + 计数"，绝不包含图元几何/文本内容：
//    - system-info.txt   版本/Qt/OS/CPU/显卡（仅设备名字符串）
//    - drawing-meta.json 图纸元信息（文件名 + 图元/图层/块计数，调用方提供）
//    - SketchCAD.log{,.1,.2}  滚动日志（若存在）
//    - 最近崩溃转储/摘要（若存在）
// ============================================================================
class QByteArray;

namespace SketchCAD {

namespace DiagBundle {

// ---- Stored（不压缩）ZIP 写入器 ----
// 用法：逐个 addFile → finish() 拿完整 ZIP 字节流（内存组装，量级 MB 级无压力）。
class ZipBuilder {
public:
    void addFile(const QString& name, const QByteArray& bytes);
    bool isEmpty() const;
    // 组装：每条目 local file header + data，尾接 central directory + EOCD。
    QByteArray finish() const;

private:
    struct Entry {
        QString    name;      // ZIP 内路径（正斜杠）
        QByteArray bytes;
        quint32    crc = 0;
        quint32    localOffset = 0;   // finish() 时回填
    };
    QVector<Entry> entries_;
};

// CRC32（IEEE 0xEDB88320，ZIP 标准）。公开给测试复算比对。
quint32 crc32(const QByteArray& data);

// 系统信息文本：程序版本 / Qt / OS / CPU 架构 / 构建ABI / 显卡名（仅名字）。
QString systemInfoText();

// 收集参与打包的既有源文件（绝对路径）：
// 日志 SketchCAD.log{,.1,.2} + crashes/last.txt + crash-*.{dmp,txt}（按名降序取前 3 对）。
QStringList collectSourceFiles();

// 一键打包。
//   outputDir 为空 → 用户主目录（QDir::homePath()）；
//   drawingMeta    → 原样写入包内 drawing-meta.json（调用方保证不含几何/文本内容）；
//   成功返回 ZIP 绝对路径；失败返回空串并写 *err。
QString exportBundle(const QVariantMap& drawingMeta,
                     const QString& outputDir = QString(),
                     QString* err = nullptr);

} // namespace DiagBundle

} // namespace SketchCAD
