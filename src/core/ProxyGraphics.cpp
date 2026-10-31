// 代理图形（Proxy Graphics）提取 —— 实现。详见 ProxyGraphics.h 头注释。
#include "ProxyGraphics.h"
#include "CadFormats.h"   // cadfmt::fromDxf：复用成熟 DXF 实体分发（架构一致）
#include <QString>
#include <QByteArray>
#include <vector>
#include <cstdint>
#include <cstring>

namespace proxy {

// ---- 二进制 DXF 值类型（必须与 CadFormats.cpp::DxfBinType 保持逐行同步）----
// 原因：二进制 DXF 的「组码 → 字节布局」映射是 AutoCAD 规范，解析与写出两端必须一致，
// 否则长度前缀/字符串边界会错位。若 CadFormats.cpp 中 dxfBinType 调整，此处须同步。
enum class BinType { Str, I16, I32, I64, Dbl, Chunk };

static BinType binTypeOf(int code) {
    if (code >= 0   && code <= 9)    return BinType::Str;
    if (code >= 10  && code <= 59)   return BinType::Dbl;
    if (code >= 60  && code <= 79)   return BinType::I16;
    if (code >= 90  && code <= 99)   return BinType::I32;
    if (code == 100 || code == 102 || code == 105) return BinType::Str;
    if (code >= 110 && code <= 149)  return BinType::Dbl;
    if (code >= 160 && code <= 169)  return BinType::I64;
    if (code >= 170 && code <= 179)  return BinType::I16;
    if (code >= 210 && code <= 239)  return BinType::Dbl;
    if (code >= 270 && code <= 279)  return BinType::I16;
    if (code >= 280 && code <= 289)  return BinType::I16;
    if (code >= 300 && code <= 309)  return BinType::Str;
    if (code >= 310 && code <= 319)  return BinType::Chunk;
    if (code >= 320 && code <= 329)  return BinType::Str;
    if (code >= 330 && code <= 369)  return BinType::Str;
    if (code >= 370 && code <= 379)  return BinType::I16;
    if (code >= 380 && code <= 389)  return BinType::I16;
    if (code >= 390 && code <= 399)  return BinType::Str;
    if (code >= 400 && code <= 409)  return BinType::I16;
    if (code >= 410 && code <= 419)  return BinType::Str;
    if (code >= 420 && code <= 429)  return BinType::I32;
    if (code >= 430 && code <= 439)  return BinType::Str;
    if (code >= 440 && code <= 449)  return BinType::I32;
    if (code >= 450 && code <= 459)  return BinType::I32;
    if (code >= 460 && code <= 469)  return BinType::Dbl;
    if (code >= 470 && code <= 479)  return BinType::Dbl;
    if (code >= 480 && code <= 481)  return BinType::Str;
    if (code >= 999 && code <= 1009) return BinType::Str;
    if (code >= 1010 && code <= 1059) return BinType::Dbl;
    if (code == 1070)                return BinType::I16;
    if (code == 1071)                return BinType::Dbl;
    return BinType::Str;
}

struct Pair { int code; QByteArray raw; };

// 解码纯二进制 DXF 主体（无哨兵头）为 token 列表。
// 与 CadFormats.cpp::binaryDxfToAscii 同构，仅去掉哨兵头校验；返回 false 表示
// 解码越界（数据损坏/非二进制 DXF），调用方据此拒绝该候选块。
static bool decodeBinaryBody(const QByteArray& blob, std::vector<Pair>& out) {
    const int N = blob.size();
    int pos = 0;
    while (pos + 2 <= N) {
        const int code = (quint8)blob.at(pos) | ((quint8)blob.at(pos + 1) << 8);   // 组码 int16 LE
        pos += 2;
        QByteArray val;
        switch (binTypeOf(code)) {
        case BinType::Str: {
            const int start = pos;
            while (pos < N && blob.at(pos) != 0x00) ++pos;                          // 空终止字符串
            val = blob.mid(start, pos - start);
            if (pos < N) ++pos;                                                    // 跳过结尾 0x00
            break;
        }
        case BinType::Chunk: {
            if (pos >= N) return false;
            const int len = (quint8)blob.at(pos); ++pos;
            if (pos + len > N) return false;                                       // 越界 → 非法
            pos += len;                                                           // 二进制块不进 token
            continue;
        }
        case BinType::I16: {
            if (pos + 2 > N) return false;
            const qint16 v = (qint16)((quint8)blob.at(pos) | ((quint8)blob.at(pos + 1) << 8));
            pos += 2; val = QByteArray::number(v); break;
        }
        case BinType::I32: {
            if (pos + 4 > N) return false;
            qint32 v = 0; for (int b = 0; b < 4; ++b) v |= ((quint8)blob.at(pos + b)) << (8 * b);
            pos += 4; val = QByteArray::number(v); break;
        }
        case BinType::I64: {
            if (pos + 8 > N) return false;
            qint64 v = 0; for (int b = 0; b < 8; ++b) v |= (qint64)(quint8)blob.at(pos + b) << (8 * b);
            pos += 8; val = QByteArray::number(v); break;
        }
        case BinType::Dbl: {
            if (pos + 8 > N) return false;
            quint64 u = 0; for (int b = 0; b < 8; ++b) u |= (quint64)(quint8)blob.at(pos + b) << (8 * b);
            double d; std::memcpy(&d, &u, 8);
            val = QByteArray::number(d, 'f', 12);                                 // 高精度，坐标往返
            pos += 8; break;
        }
        }
        out.push_back(Pair{code, val});
    }
    return true;
}

// 已知实体类型：首 token 为 "0 <实体类型>" 即视为像样的 DXF 实体（廉价预筛）。
static bool isKnownEntityType(const QByteArray& t) {
    static const char* const types[] = {
        "LINE","POINT","CIRCLE","ARC","ELLIPSE","SPLINE","TEXT","MTEXT","SOLID",
        "3DFACE","POLYLINE","LWPOLYLINE","VERTEX","SEQEND","INSERT","ATTRIB",
        "ATTDEF","DIMENSION","HATCH","XLINE","RAY","LEADER","SHAPE","TRACE"
    };
    for (auto* s : types) if (t == s) return true;
    return false;
}

bool decodeProxyGraphicsBlob(const QByteArray& blob, std::vector<SketchCAD::Element>& out) {
    if (blob.size() < 6) return false;                 // 至少 1 组码(2B) + 最小值(≥1B) + 终止符
    std::vector<Pair> pairs;
    if (!decodeBinaryBody(blob, pairs)) return false;  // 越界 → 损坏/非二进制 DXF
    if (pairs.empty()) return false;

    // 廉价首类型预筛：整段 token 中至少要有一个 "0 <实体类型>"。
    bool hasEntity = false;
    for (const auto& p : pairs)
        if (p.code == 0 && isKnownEntityType(p.raw)) { hasEntity = true; break; }
    if (!hasEntity) return false;

    // 组装 ASCII DXF：若 body 自带 SECTION/ENTITIES 则原样回放，否则包一层。
    bool hasSection = false;
    for (const auto& p : pairs)
        if (p.code == 0 && p.raw == QByteArray("SECTION")) { hasSection = true; break; }

    QByteArray ascii;
    if (!hasSection) ascii += "0\r\nSECTION\r\n2\r\nENTITIES\r\n";
    for (const auto& p : pairs) {
        ascii += QByteArray::number(p.code); ascii += "\r\n";
        ascii += p.raw; ascii += "\r\n";
    }
    if (!hasSection) ascii += "0\r\nENDSEC\r\n0\r\nEOF\r\n";

    // 复用成熟 DXF 实体分发（版本协商 / 子类标记忽略 / 实体构造全部现成）。
    QString err;
    SketchCAD::cadfmt::DxfImportStats st;
    const bool ok = SketchCAD::cadfmt::fromDxf(ascii, out, &err, &st);
    return ok && st.entities > 0;
}

std::size_t extractProxyGraphics(const QByteArray& dwg, std::vector<SketchCAD::Element>& out) {
    const int n = dwg.size();
    if (n < 8) return 0;
    const char* d = dwg.constData();
    std::size_t total = 0;
    const int kMinPerBlob = 2;     // 单块至少 2 个图元才接纳（防单实体误报）
    int lastGoodEnd = -1;          // 已接纳块之后跳过，避免同一份数据的重叠重复提取
    for (int p = 0; p + 4 <= n; ++p) {
        if (p <= lastGoodEnd) continue;
        // 4 字节小端长度前缀（长度 = 紧随其后的二进制 DXF 字节数）。
        const quint32 L = (quint32)((quint8)d[p] | ((quint8)d[p + 1] << 8)
                                  | ((quint8)d[p + 2] << 16) | ((quint32)(quint8)d[p + 3] << 24));
        if (L < 16 || L > (quint32)(n - (p + 4))) continue;
        QByteArray blob = dwg.mid(p + 4, (int)L);
        // 廉价预筛：二进制 DXF 主体首 2 字节应为组码 0（0x00 0x00），否则必非 DXF。
        if (blob.size() < 2 || blob.at(0) != 0x00 || blob.at(1) != 0x00) continue;
        std::vector<SketchCAD::Element> cand;
        if (decodeProxyGraphicsBlob(blob, cand) && (int)cand.size() >= kMinPerBlob) {
            for (auto& e : cand) out.push_back(std::move(e));
            total += cand.size();
            lastGoodEnd = p + 4 + (int)L;             // 跳到本块之后
        }
    }
    return total;
}

} // namespace proxy
