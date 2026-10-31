# DXF R2004–R2018 现代写出设计分析

日期：2026-10-27　|　自测：3102/0 EXIT=0　|　双目标 -Wall -Wextra 零警告

## 1. 背景与目标

客户强制交付 R2013/R2018 可打开、块/标注/填充语义保留——对外交付硬阻塞。

**硬约束**：
- 主程序零链接 GPL 源码（LibreDWG 仅进程外桥）；
- 不虚标版本号；
- R12 自研路径与既有黄金样本回归不得破坏；
- `ElementType`/存档字段只增不改；
- 不引入 ODA Drawings SDK（商业授权）。

## 2. 关键事实与决策

| 事实 | 结论 |
|---|---|
| LibreDWG `dwgwrite` 写能力**封顶 R2000(AC1015)** | 真 R2018 `.dwg` 二进制在无 ODA 前提下不可得 |
| DXF 是纯文本、版本由 `$ACADVER` 声明、实体集自控 | **DXF 写出扩展到 R2004–R2018 完全可行且诚实** |
| AutoCAD 打开 DXF 即按 `$ACADVER` 识别版本，可另存为 DWG | R2018 语义保留验收由 DXF R2018 路径承担 |

**交付路线（唯一自洽解）**：
1. **DWG 写出仍封顶 R2000**，UI 文件过滤器明确标注「写出最高 R2000（原生 R12 / 桥接 R2000）；R2004+ 请选 DXF」；
2. **DXF 写出扩展到 R2004–R2018**，AutoCAD 打开即识别为该版本、块/标注/填充语义保留、可直接另存为 DWG。

> 说明：验收原文「R2018 DWG 在正版 AutoCAD 可打开」在「不虚标版本号 + 无 ODA + LibreDWG 封顶 R2000」三约束下，唯一技术自洽解即上表路线 2。若必须产出 R2018 `.dwg` 二进制，唯一途径是 ODA Drawings SDK 商业授权（已按范围排除）。

## 3. 版本映射

| 枚举 | $ACADVER | 实体集 |
|---|---|---|
| R12 | AC1009 | POLYLINE/VERTEX/SEQEND + ACI 62（逐字节兼容旧档） |
| R2000 | AC1015 | 同上 + AcDb 标记/句柄/OBJECTS 段（行为不变） |
| R2004 | AC1018 | **modern**：LWPOLYLINE + 420 + 440 + 370 |
| R2007 | AC1021 | 同 R2004 |
| R2010 | AC1024 | 同 R2004 |
| R2013 | AC1027 | 同 R2004 |
| R2018 | AC1032 | 同 R2004（交付目标版本） |

## 4. modern 实体集（reqV ≥ R2004）

- **LWPOLYLINE**：`90` 顶点数 + `70` 闭合标志 + 逐顶点 `10/20` + `42` bulge（弧段语义与 R12 POLYLINE 完全一致，仅载体换新）。Rectangle/Polygon/Ellipse/Spline/折线离散全部经由 `polylineEx` 统一切换。
- **真彩 420**：`0x02RRGGBB`，恒写（62 ACI 保留作回退）；精确保色，避免降级成最近 ACI 的色偏。
- **透明度 440**：`(alpha & 0xFF) << 24`，仅当 `color.alpha() < 255` 写出（QColor alpha 是天然数据源，不透明图元零噪声）。
- **线宽 370**：`qRound(lineWidth × 13)`（1/100mm），仅 `!lineWidthAuto` 写出（随层图元不强制覆盖）。
- **MULTILEADER**：`MLeader` 图元（此前 DXF 导出静默丢弃）在 ≥R2000 以最小合法结构导出：`90`(覆盖标志)/`340`(样式指针)/`170`(引线数)/`171`(点数)/`10-20-30` 点列/`304`(文字)/`42`(字高)。

组码注入顺序由 `transformDxfTextToModern`（原 `transformDxfTextToR2000` 更名，R2000+ 通用）保证：`0 → 5 句柄 → 330 owner → 100 AcDbEntity → 公共组(8/62/6/420/440/370) → 100 <子类> → 几何数据`，与 AutoCAD 期望的子类标记顺序一致。

## 5. 版本协商同步放开

`isSupportedDxfVersion` / `detectDxfVersion` 扩到 AC1021–AC1032：
- **仅约束 DXF 读取**；`fromDwg` 的 DWG 桥路由（AC1009 自研 / AC1012–AC1032 经桥）不受影响；
- 原「AC1021+ 拒绝、请另存为 2000」策略废止——本程序现已能原生读写这些版本的 LWPOLYLINE/420/440/370，继续拒绝会与自身写出能力自相矛盾；
- 真正的「不支持」仅限未知/未来版本串（如 AC9999），仍明确报错。

## 6. 测试与防回归

- `genFormatsGoldens` 新增 `golden_r2004.dxf` / `golden_r2018.dxf`（各 5249B，内容仅版本串不同）。
- `t_formatsGolden` +26 断言：R2018 结构（AC1032 / LWPOLYLINE / 无 POLYLINE / 420 / OBJECTS / AcDb 标记）→ R2018 再导入 → R2018 重导出 → 再导入全链路（实体/块/层计数一致 + 标注/填充/块引用语义保留）；R2004 下界 7 断言。
- `t_formatsVersions`：⑥ 协商改写（AC9999 拒绝 / AC1021+ 现支持 / detectDxfVersion(AC1032)=R2018）；⑪ 新增 440/370/MULTILEADER 写入断言 + **R12 反向保证**（不写 420/440/370/LWPOLYLINE——现代组码不回灌旧档）。
- R12/R2000 黄金样本逐字节不变（modern=false 分支零影响），5 条既有 golden 往返全绿。

**⚠️ 工程陷阱**：`t_formatsGolden` 从 `build/golden` 读样本（CMake `file(COPY)` 仅在**配置期**拷贝），而生成器写 `tests/golden/` → 新黄金样本必须手动 `cp tests/golden/golden_r20*.dxf build/golden/`，否则 26 条断言级联失败（"已生成"→文件不存在→全挂）。

## 7. 遗留与后续

- **需正版 AutoCAD 实机验证**（本机无）：R2018 DXF 打开、MULTILEADER 最小结构容忍度。若 MULTILEADER 触发「图形需要修复」，退化方案为 LWPOLYLINE+MTEXT 兜底（一行分支）。
- MLINE（平行多线，AutoCAD 墙体实体）在数据模型中**无对应图元类型**（ElementType 至 Fcf=19），无法凭空产生；本次以 MULTILEADER（模型确有 MLeader 图元、此前导出丢失）补齐最接近的真实能力。若需要真 MLINE，须按「只增不改」追加 ElementType + UI/渲染/序列化全链，属独立特性。
- R2007(AC1021) 官方建议 UTF-8 编码；本实现暂沿用 GBK/ANSI_936 声明（AutoCAD 兼容读取），如需严格 R2007 规范可后续切换 `decodeDxfBytes` 同款内容判定。
