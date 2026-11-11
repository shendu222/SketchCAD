#pragma once
// ============================================================================
//  打赏入口配置 —— 「帮助 > 关于」对话框底部「打赏」按钮
//
//  当前形态：内置微信收款码（由用户提供）。点击「打赏」直接弹出可扫码的收款码，
//  二维码编译进 exe 的 Qt 资源，不依赖外部图片文件、不需要联网、不引入任何
//  支付 SDK / 第三方支付流程。
//
//  资源源文件：resources/reward/wechat_reward_qr.png
//  资源注册：  CMakeLists.txt → qt_add_resources(SketchCAD "sketchcad_reward_qr" ...)
//  更换收款码：用新图覆盖上述源文件后重新构建即可（CMake 会重跑 rcc）。
//              建议保持白底、二维码四周留白、整图不低于 600×600 像素。
//
//  kTargetUrl / kWeChatId 是可选的【降级通道】：仅当内置收款码资源加载失败时
//  才会依次尝试（链接 → 微信号 → 明确报错）。正常发行版不会走到这两条分支，
//  默认均留空，无需补充任何微信号或链接。
//
//  ⚠️ 本文件只承担「打赏目标」这一项配置，不涉及界面、账户、收款或第三方依赖。
//     修改后需重新构建 SketchCAD（./build.sh 或 cmake --build build -j 1）。
// ============================================================================

namespace SketchCAD {
namespace RewardConfig {

// 内置微信收款码的 Qt 资源路径（唯一真相源，与 CMakeLists.txt 的 PREFIX 对应）。
inline constexpr const char* kQrResource = ":/reward/wechat_reward_qr.png";

// 收款码在对话框中的显示宽度（逻辑像素）。420px 已实测仍可正常解码扫码。
inline constexpr int kQrDisplayWidth = 420;

// 【可选降级 ①】可直接打开的链接（https:// / weixin:// 等）。
inline constexpr const char* kTargetUrl = "";

// 【可选降级 ②】微信号（纯文本），复制到剪贴板并提示在微信中搜索添加。
inline constexpr const char* kWeChatId = "";

} // namespace RewardConfig
} // namespace SketchCAD
