# 验证记录

验证日期：2026-10-20（Asia/Shanghai）。以下是当前源码和透明动画版本的验证结果，不代表实体设备认证。

## 环境

| 项目 | 已验证版本 |
| --- | --- |
| 宿主 | Linux x86_64 |
| Codex CLI | 0.163.0 |
| ARCS Mini SDK | `23fca91e13dbd0f2c81eb2f69a6b337953021637` |
| 编译器 | Nuclei GCC 14.2.1 20240816 |
| Lisem | 0.8.1，Arcs-Mini / LS2684 |
| QEMU SHA-256 | `1103d6da478dbb8420144056fe350bcecbb75e2ef23535d3003d6112dd00d5ca` |

## 单元和素材检查

运行 `./scripts/test.sh`，8 项 Node、5 项 Python、4 项 C 测试全部通过：

- Codex 记录规范化、敏感正文不进入 SSE、鉴权、历史补发、重启 reset、增量日志读取。
- 彩蛋目录、点播、节日优先、北京时间边界、闰月排除、大小月除夕。
- 2,191 天离线历表全部命中缓存，范围外回退；2026 年两家天文台资料逐日校验，365 天无差异。
- 统一动画布局、原始白色像素和半透明像素保留。
- SSE 分片、边界和过长输入拒绝；全部核心和彩蛋帧解码、透明合成及损坏数据拒绝。
- 按键消抖、320 ms 双击边界、三连击、长按不重复和 tick 溢出。

## 固件构建

使用 `scripts/build-firmware.sh` 构建通过。合并镜像包含 Boot 和应用，写入 Flash 偏移 0。

- 镜像：14,824,868 字节，距 Wi-Fi NVS 边界尚余 1,903,196 字节。
- 显示：240×240 RGB565；角色 208×196；目标 12 FPS。
- PSRAM 堆：3 MiB；单个下载素材包上限 1,750,000 字节。
- 透明度：16 级，RGB 与 alpha 独立 PackBits 编码，背景按帧合成。

## Lisem 端到端验证

`python3 scripts/verify-lisem.py` 使用合成的 Codex JSONL 和 hook 输入，运行真实 RISC-V 固件，以下检查通过：

- 虚拟 Wi-Fi DHCP、HTTP SSE 连接和七种状态动画。
- 截图非空、连续帧变化，海底背景与角色合成。
- 圣诞树动画下载、播放和 Codex 状态打断；五子棋动画完整播放。
- bridge 重启后的断线重连、history reset 与新事件接收。

`python3 scripts/verify-button.py` 在关闭宿主网络的独立实例中验证：

- 连续五次单击覆盖五种回应，不重复。
- 双击蓝鲸打断单击、连续帧变化、播放完成及待机缓冲。
- 长按松手只触发一次，点击资源无需网络。

演示资产：[动画 GIF](animation-preview.gif)、[海底背景实际截图](undersea-preview.png)。目视检查了角色白色服饰、透明背景和文字可读性，未逐帧目视检查全部 88 个动画。

## 真实会话与验证边界

本地 Codex Desktop 会话曾实际驱动固件接收 `custom_tool_call`、`custom_tool_call_output`、`reasoning`、`message` 状态。网络仅传输白名单元数据，不含消息正文。真实会话日志、线程 ID、本机实例身份、截图中间文件和编译网络配置保留在被忽略的 `artifacts/`、`build/`，不随公共仓库发布。

尚未验证：ListenM 实物烧录、WPA2 接入、长期运行、多个会话的优先级仲裁。当前功能针对本地开发网络；固件不支持 TLS、DNS 或任意第三方 SSE 服务。
