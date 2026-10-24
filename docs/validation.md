# 验证记录

## 2026-10-24：统一语音固件、TF 配置与中文状态

- 只保留 12 FPS 语音固件，`build-firmware.sh` 构建 AP/CP 整包。CP 约 1.2 MiB，
  整包仍为 16 MiB；原算法资源 Flash 区间为空白，模型与 wrap 从 TF 卡读入 PSRAM。
- `config.ini` 运行时加载网络、token、音量阈值、模型路径和精确唤醒 keyword；
  无卡/配置错误停止启动，缺失或损坏的显示资源停止渲染，无 Flash 回退。
- 16 个 Node 测试、6 个 Python 测试、7 个 C 测试通过。新增配置边界、TF 有界读取、
  字体 atlas/裁切、Flash 打包不含资源及分区超限测试。
- SDK 编译链接和整包校验通过。Lisem 验证 TF 模型初始化、Wi-Fi/DHCP/SSE、
  七种中文状态动画、TF 节日/游戏彩蛋、工作状态抢占、bridge 重启与重新连接。
- 离线单击、五种回应不重复、双击抢占、动画完成与长按只触发一次通过。
  FAT 卡读取、动态截图、缺卡和截断资源错误路径通过。
- 字体为 Noto Sans CJK SC Medium v2.004，18 px 抗锯齿，标题和状态上下各留 8 px。
  “聆听中”使用专注动画和音量条，录音结束恢复最新状态。
- 尚未验证实机麦克风识别、实际语音提交与三代 LS2663 语音 AP/CP 布局。
  三代构建被明确拒绝；不提供无语音版本。此前三代 PSRAM/屏幕适配保留为后续基础。

本地验收产物（不提交）：`artifacts/lisem-e2e-2ovxopk5/`、
`artifacts/button-e2e-oe940wsw/`、`artifacts/overlay-e2e-dqy3ccsw/`。

## 2026-10-20 历史验证

以下记录对应当时的源码与资源布局，不代表当前固件的 Flash 资源布局或实体设备认证。

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

尚未通过验证：ListenM 实物烧录与启动、实机 WPA2 接入、长期运行、多个会话的优先级仲裁。仿真 WPA2 接入已在下面的复查中验证。当前功能针对本地开发网络；固件不支持 TLS、DNS 或任意第三方 SSE 服务。

## 实机首次烧录与仿真复查（2026-10-22）

无语音、12 FPS 镜像大小为 14,880,892 字节，SHA256 为
`8afc07f85c9e97d9b4daae6c03345e733ca55f554803203cd0396e902b7ba62c`。
该摘要仅标识此次诊断镜像；网络参数和 token 不同会改变摘要。

实机通过 Type-C 进入原厂 ADB Recovery，查询到 16 MiB Flash，并已完整备份。
向偏移 0 写入整包时，ADB 显示文件传输完成，但最终确认失败（EOF），之后
Recovery 离线。复位后黑屏且未连接 bridge。实机未能读回校验，不能认定烧录成功。

在独立 Lisem 0.8.1 / Arcs-Mini 实例中使用同一镜像复查：

- 无匹配热点时，仍显示 240×240 离线动画，UART 报告 12 FPS。
- 配置匹配的 WPA2-PSK 热点后，认证、DHCP、SSE 连接均成功，连续截图有变化。
- 使用原厂完整备份启动另一实例，保持宿主网络关闭，通过 ADB 进入 Recovery。
- 重放整包写入后，ADB 正常退出；读回的全部 14,880,892 字节与目标镜像一致。
- 复位后仍能显示离线动画，无模拟 CPU 异常。没有复现实机黑屏或写入确认失败。

发现的实机相关差异：原厂固件启动后主动驱动 PB3 电源保持为高；本项目镜像
未驱动 PB3（运行状态 GPIOB 输出掩码为 `0x200`，只包含 PB9）。SDK 的原厂
电源管理实现会初始化并拉高电源保持脚，而本项目精简 boot 和应用未包含该流程。
Lisem 的板型将此脚建模为信号探针，不会因它未拉高而模拟物理断电。
这是待修复、待实机确认的问题，尚不能认定为此次黑屏根因。

当前镜像也不提供 USB ADB / Recovery，因此重启后 USB 消失本身不能证明启动失败。
后续实机排查应先通过 UART 查看启动日志、核验 Flash 内容；再次烧录前需处理
板级电源初始化和可恢复的 boot 方案。仿真使用独立 OTP/UID 和模拟外设，不能验证
实际电源时序、Flash 写入完整性或真实 LCD 电气行为。

诊断日志、截图、实例及备份保存在本地忽略目录 `artifacts/black-screen/`、
`artifacts/device-backup/`；不发布设备身份、固件内凭据或原厂完整备份。

## TF overlay 仿真验证

2026-10-20 使用 `scripts/verify-overlay.py` 在独立 Lisem Arcs-Mini 实例中完成：

- 64 MiB FAT16 TF 卡镜像，根目录放入金黄色棋盘背景及当前 `whale.bin`、`click.bin`。
- UART 确认三个文件均从 TF 卡选用；截图四角 RGB 均为 `(242, 180, 0)`，
  与测试背景 RGB565 `0xFDE0` 经固件 95% 背光后的颜色一致。
- 连续截图确认待机动画、双击蓝鲸动画都在变化，双击播放完成并进入待机缓冲。
- 无 TF 卡、TF 卡背景文件截断两种情况下均恢复内置海底背景；两者截图顶部边缘逐像素一致。
- 未出现动画读取/解码失败或 display/asset failure；人工检查了金色背景与透明角色的合成截图。

复现（需 `mkfs.fat`，测试镜像、UART 日志和截图均保存在忽略的 `artifacts/`）：

```bash
python3 -m venv artifacts/overlay-tools
artifacts/overlay-tools/bin/pip install pillow pyfatfs 'setuptools<81'
artifacts/overlay-tools/bin/python scripts/verify-overlay.py
```

固件交叉编译及全部项目测试通过，C 测试新增 overlay 后为 5 项，覆盖 TF
优先、缺失/截断文件、动画边界检查、损坏帧不污染回退像素。未验证实体 TF 卡性能或热插拔。
