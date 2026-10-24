# 授权范围与第三方来源

本项目为混合授权仓库。根目录 `LICENSE` 的 MIT 授权适用于本项目原创代码，不改变第三方素材、历表或外部依赖的权利。

## 鲸鱼动画及衍生画面

- 来源：[`luweiyabo/dsh-whale-pet`](https://github.com/luweiyabo/dsh-whale-pet)，固定提交 `4c9ad7ff8c8ad640a827525b3512ca544572e9ca`。
- 上游媒体署名：**PC2005-cloud/dsh-pet contributors**。
- 范围：`firmware/assets/whale.bin`、`firmware/assets/click.bin`、`bridge/assets/*.bin`，以及包含角色素材的演示 GIF、截图和固件镜像。
- 这些动画素材**不按 MIT 重新授权**。保留的上游资源说明指出：允许开源项目使用，禁止商业使用。商业使用需另获权利人授权。
- 保留的原文：[第三方资源说明](licenses/whale-assets-upstream.md)；参考项目代码许可证：[MIT](licenses/dsh-whale-pet-MIT.txt)。资源说明中“本项目”指原始 dsh-whale-pet 项目。

## 背景

`assets/backgrounds/whale-undersea.png` 为本项目通过 OpenAI 图像生成工具生成的背景，设备转换资源和来源记录见该目录 README 与 manifest。它不含上游鲸鱼角色；与角色合成的演示仍包含受上述条款约束的动画素材。

## 历表

`bridge/data/calendar/sources` 保留香港天文台 2026–2031 年日期对照文本、紫金山天文台 2026 年日历 PDF。原始资料的权利归各发布机构；本项目转换其中客观日期信息用于离线节日调度，不将原始出版物重新授权为 MIT。来源 URL、SHA-256 和校验范围见 `bridge/data/calendar/README.md` 及 `calendar.json`。

## 外部依赖

ARCS Mini SDK、Lisem 和原始 WebM 仓库下载至被忽略的 `.deps/`，不纳入本仓库源码提交，使用时遵循各自许可。本项目未修改 SDK 或 Lisem 源码。

## Chinese status font

`firmware/assets/status-font.bin` and the `resource-missing.png` /
`resource_missing.h` emergency screen contain rasterized labels from Noto Sans CJK SC
Medium, by the Noto/Source Han contributors, licensed under SIL OFL 1.1.
Source: https://github.com/notofonts/noto-cjk/tree/main/Sans
License: [noto-sans-cjk-OFL.txt](licenses/noto-sans-cjk-OFL.txt).
Rebuild with `scripts/pack-font.py`; labels remain separate TF resources.
