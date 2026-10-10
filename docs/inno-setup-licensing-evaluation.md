# Inno Setup 授权评估说明

> 文档性质：合规评估 / 人工决策依据
> 项目：SketchCAD（航空 CAD 草图绘制模块）商业安装包
> 日期：2026-10-09
> 结论适用前提：本评估基于官方公告与官方订单页（jrsoftware.org）于 2026-10-09 的公开内容，条款以官方最新发布为准。

---

## 1. 结论先行（TL;DR）

| 事项 | 结论 |
|---|---|
| 商业安装包是否**被法律强制**购买 Inno Setup 许可 | **否**。Inno Setup 开源许可本身允许任意用途（含商业）免费使用；6.5.0 起的"商业许可"是**官方请求（request）**，不是许可条款的强制变更 |
| 是否**建议**购买 | 对年营收超过 USD 5,000 的营利性组织，官方明确请求购买；从**合规审计、供应商软件台账、社区支持**角度建议纳入采购评估 |
| 立即可做的零成本缓解 | 本机实测已装 **6.7.1**（≥ 6.5.0），"锁旧版"缓解需降级至 6.4.3 才成立；否则维持现状（请求未响应，无许可违约，见路径 A） |
| 产物影响 | **无水印、无标记**：官方 Q&A 明确"installers built without a license are not explicitly marked as unlicensed"，未购许可的产物与已购许可产物在用户侧无外观差异 |
| 备选方案 | NSIS（zlib 许可，完全免费，无任何商业使用限制），但需重写打包脚本，迁移成本见 §5 |

---

## 2. 背景：本项目当前使用方式

- 打包脚本 `build.sh package` 调用 Inno Setup 6 命令行编译器 `ISCC.exe` 将 staging 目录编译为 `SketchCAD-<版本>-Setup.exe`。
- 使用形态属于官方定义的 **"running the compiler from a build script"（CI/脚本调用）**：无人工交互使用 IDE，官方口径为**至少购买 1 个单用户许可**。
- 安装器分发给公司内外用户安装使用（含企业内部部署场景）。
- **本机已安装版本：Inno Setup 6.7.1**（ISCC 控制台横幅实测：`Compiler engine version: Inno Setup 6.7.1`，其下紧跟一行 `Non-commercial use only`）。即当前安装已落入"官方请求商业许可"的版本区间。

## 3. 授权事实核查（官方来源）

### 3.1 版本时间线

| 版本 | 授权状态 |
|---|---|
| ≤ 6.4.3（2025-05-03） | **旧宽松许可**：明确授予"任何人、任何目的（包括商业应用）"免费使用权。已下载安装的版本按其自带许可执行，后续新条款**不溯及既往** |
| ≥ 6.5.0（2025-08-13） | 官方公告引入 Commercial Licenses："商业语境下使用并受益的组织，恳请购买许可"；同时声明"Inno Setup 仍对非商业使用免费" |

### 3.2 "商业用户"的官方定义（满足任一）

1. **年营收超过 USD 5,000**（或等值其他货币）的营利性组织；
2. 以营利为目的使用 Inno Setup、且年营收超过 USD 5,000 的个人/自由职业者。

补充口径：
- **仅内部使用也算商业**——官方 Q&A 对"只给内部装、不发给客户"的答复是 Yes，预期购买；
- 组织内的"user"按分配给单个员工计，单用户许可可跨多台机器（须为该用户主力使用）。

### 3.3 是否强制

两层事实需要分开：

- **法律层面**：Inno Setup 6.5.x 仍以"Inno Setup License"（宽松自由软件许可）分发，该许可原文授权"any purpose, including commercial applications"。官方 Q&A 原文承认 *"Are commercial users required to purchase a license? — It is not strictly required."*
- **请求层面**：官方在公告、发布说明、编译器输出中**反复请求**商业用户购买（"we kindly ask that you purchase a license"）。忽略该请求不构成许可违约，但属于明知且持续不接受作者诉求的商业使用，存在社区关系与商誉层面的考量。

### 3.4 定价与授权模式（官方订单页 isorder.php）

| 档位 | 覆盖 | 页面标价（样例，随地区货币浮动） |
|---|---|---|
| Single User | 1 用户 | ¥24,507 |
| Team | 2–5 用户 | ¥64,824 |
| Enterprise | 不限用户 | ¥188,939 |

要点：
- **永久授权、一次性付费**，含 2 年更新（Enterprise 可购更长更新期）；到期后已装版本永久有效，续期更新权可按当时价格 6 折（到期后两个月内续费）；
- 结算经 Paddle（Merchant of Record），支持对公转账，30 天退款保证；
- **捐赠换许可**：近两年内向项目捐赠过（金额不限）可免费换取 1 个单用户许可。

> 注：订单页价格按访问者地区以当地货币显示，上表为本次抓取样例（日元计价）；实际以结算时 Paddle 页面为准。以 USD 计，Single 档大致在 $150–170 区间量级。

### 3.5 "Non-commercial" 标记实测与产物外观

经实测与官方口径核对，"Non-commercial" 标记分两个层面，结论不同：

- **编译器输出层（有标记）**：ISCC 6.7.1 每次编译的控制台输出中，版本横幅下固定打印 `Non-commercial use only` 一行（本机 2026-10-09 打包日志实测）。这属于构建机侧的提示，随构建日志留痕。
- **安装产物层（无标记）**：官方 Q&A 原文 *"installers built without a license are not explicitly marked as unlicensed."* 编译出的 `Setup.exe` 不带任何"未授权/Non-commercial"印记，最终用户侧无从分辨。

即：本仓库此前"免费版带 Non-commercial 提示"的记录**在编译日志层面成立**，在**安装产物层面不成立**——产物本身不受影响。

## 4. 可选路径对比

### 路径 A：版本策略调整，暂不采购（零成本）

- 现状前提：本机已装 **6.7.1**（≥ 6.5.0），官方"请求"当前处于未响应状态；法律上不违约（Inno Setup License 原文仍授权商业应用），但构建日志持续留有 `Non-commercial use only` 痕迹，且审计口径弱于路径 B。
- 零成本子选项：
  - **A1 降级/固定 6.4.3**：适用旧宽松许可，商业使用免费且完全合规；代价是放弃 6.5+ 的安全修复与新特性（TLS 1.3 下载、RAR5、整体加密增强等），且属逆向操作；
  - **A2 保持 6.7.1 现状**：不采购、不降级，接受"官方请求未响应"状态；风险仅在商誉与审计叙事，无许可违约。
- 共同风险：无论 A1/A2，均错过 6.5+ 的持续维护。

### 路径 B：购买 Single User 商业许可（合规最优）

- 成本：一次性（页面样例约 ¥24,507，USD 约 150–170 量级），2 年后可选续期。
- 收益：软件台账干净、审计有据、直接支持维护者；对商业化发行的 CAD 产品，是省心且金额不大的选项。
- 覆盖面：1 个单用户许可即可覆盖"1 名工程师 + 其本机与 CI 机"的官方口径（CI 无人交互时至少 1 单用户许可）。

### 路径 C：迁移 NSIS（零许可成本，有工程成本）

- NSIS 为 zlib 许可，任何用途免费、无商业请求条款。
- 迁移成本：
  - 重写打包脚本（.iss → .nsi），包括：文件关联任务（现 `/TASKS="fileassoc"`）、静默安装参数、卸载、版本资源注入；
  - `build.sh` 的 ISCC 探测与调用段、`/DAppVersion=` 注入方式需改为 NSIS 的命令行定义（`/D` 同名机制可用，改动可控）；
  - 需回归测试：静默安装、指定目录、文件关联、升级覆盖卸载全链路。
- 判定：除非出现"Inno Setup 新版本许可条款实质收紧（真正强制）"的情况，否则迁移 NSIS 的工程成本**高于**购买一份 Single 许可的成本，当前不建议。

## 5. 决策建议

1. **立即**：确认本机/CI 的 Inno Setup 版本并记录归档（检查方法见 §6）。
2. **短期（本周）**：Authenticode 签名流水线已就绪（见 `docs/authenticode-signing-design-analysis.md`），签名事宜独立于安装器授权，先行推进。
3. **中期（随商业化发行节奏）**：若产品确定对外商用发行，按路径 B 采购 1 个 Single User 许可；采购前先核对是否满足"近两年捐赠换免费许可"的条件。
4. **持续**：不盲目升级 ISCC；若未来升级到 6.5+，同步触发本评估第 4 节路径重议。

## 6. 版本检查方法与实测记录（归档）

```bash
# Git Bash 下查看本机 ISCC 版本（编译日志首部横幅亦可见）
ISCC="$LOCALAPPDATA/Programs/Inno Setup 6/ISCC.exe"
"$ISCC" /? 2>&1 | head -3
```

**本机实测记录（2026-10-09，`build/package-nocert2.log`）：**

```
[OK] ISCC: C:\Users\shend\AppData\Local/Programs/Inno Setup 6/ISCC.exe
...
Inno Setup 6 Command-Line Compiler
Compiler engine version: Inno Setup 6.7.1
Non-commercial use only
```

判定：**本机 6.7.1 ≥ 6.5.0 → 处于"官方请求商业用户购买"区间**（产物层无标记，见 §3.5）。

## 7. 参考来源

1. Inno Setup 6.5.0 发布公告（2025-08-13，官方 Google Groups）：jrsoftware.org / groups.google.com/g/innosetup-announce/c/xFoxpW036-w
2. Inno Setup Commercial Licenses 订单页与 FAQ：https://jrsoftware.org/isorder.php
3. Inno Setup License（许可原文，含"any purpose, including commercial applications"授权条款）：项目内 license.txt / community.chocolatey.org/packages/InnoSetup
4. Inno Setup 6.4 Revision History（6.4.3 = 2025-05-03）：https://jrsoftware.org/files/is6.4-whatsnew.htm
5. Wikipedia - Inno Setup（6.5.0 商业许可"非严格强制"表述）：en.wikipedia.org/wiki/Inno_Setup
