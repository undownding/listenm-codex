# 验证记录

## 2026-10-24：资源缺失提示图

新增内置应急图片（约 13 KiB RGB565 RLE），不依赖 TF 卡背景或字体。
30 项单元测试、SDK 构建通过；Lisem 验证正常显示与点击动画，以及无卡、
截断背景、缺背景、缺核心包、缺点击包、缺字体六种失败场景，均显示完整中文提示图。
故障截图与日志：`artifacts/overlay-e2e-58qfyvzr/`（不提交）。
缺失资源的行为由下方历史记录的停止渲染，更新为停止播放并显示应急图片。

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

尚未通过验证：ListenM 实物烧录与启动、实机 WPA2 接入、长期运行、多个会话的优先级仲裁。仿真 WPA2 接入已在下面的复查中验证。该次验证时功能针对本地开发网络，固件尚不支持 TLS、DNS 或任意第三方 SSE 服务；后续 DNS/mDNS 支持见下文。

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

## Mini3 语音与 ADB 适配（2026-10-25）

Mini3 / LS2663 已使用配套 Boot、SDK Mini3 AP 和本项目 CP 完成仿真验收。
Boot 位于 `0x0`，AP 位于 `0x40000`，CP 位于 `0x600000`；AP/CP 分别使用
3 MiB / 5 MiB PSRAM。CP 兼容头声明该内存布局，并启用与 AP 一致的 IPC 区，
共享锁地址为 `0x20040e00`。省略 IPC 区会使 Wi-Fi 在关联后死锁。

验收命令：`PET_BOARD=arcs_mini3 python scripts/verify-lisem.py`（Python 环境需有
Pillow、pyfatfs）。测试使用单独的虚拟 TF 和合成 Codex 记录，未修改实体 TF。
本次结果在 `artifacts/lisem-e2e-76xly2qr/result.json`，包含：

- TF 模型加载、语音唤醒引擎初始化、Wi-Fi DHCP 与 SSE 连接。
- 七种状态的非空画面与连续动画帧。
- 节日彩蛋播放、Codex 事件打断、游戏动画完整结束。
- bridge 重启后的历史重置、自动重连和后续事件。

Mini3 的彩蛋改为按帧读取 TF，避免把最高约 1.7 MB 的动画整包放入内存。
主机测试覆盖同帧解码一致、非法文件名、截断文件、索引越界和关闭后的读取失败。
`bash scripts/test.sh` 全部通过；Mini 和 Mini3 均已重新构建。

USB 验收使用真实固件的 USB 协议，通过 Lisem 的 TCP USB 桥接运行 ADB。
已验证无 TF 时应用 ADB 在线、`adb reboot recovery`、AP/CP 写入、全部内容读回
逐字节相同，以及 Boot 重启后应用重新连接。另验证过 AP 头无效时自动进入恢复模式。
最终更新脚本为 `scripts/flash-mini3-adb.py`，要求明确 serial、Mini3 设备标识与
16 MiB Flash，并校验 CP 内存兼容头。只更新 AP/CP；Boot 和其他区域保持原样。
日志在 `artifacts/mini3-flash/adb-updater-test.log`。

这些结果不代表实体设备已刷入，也不包含真人唤醒词准确率或实体麦克风/扬声器验证。
首次安装配套 Boot 仍需实体设备处于可用下载模式；后续应用升级可使用 ADB。

### 本次实体写入

随后在 `/dev/ttyUSB0` 以 230400 baud 写入同一组已验收镜像，工具退出码为 0。
三个分区的设备端 MD5 均与本地一致：

| 分区 | 地址 | 字节数 | MD5 |
| --- | --- | --- | --- |
| AP | `0x40000` | 427868 | `b292ac77f1629b41e0733822a3d738b1` |
| CP | `0x600000` | 1277556 | `9f53529c7254c7407ee8d411079e7f32` |
| Recovery Boot | `0x0` | 133560 | `dc35990801e5528c6b43e9ddfd4e5109` |

日志：`artifacts/mini3-flash/physical-flash.log`；SHA-256 清单：
`artifacts/mini3-flash/validated-manifest.json`。未整片擦除，也未修改实体 TF。
写入完成时电脑尚未枚举出板子 ADB；实体正常启动与 USB 连接验证另行记录。

用户随后反馈实体测试成功。该结论为用户实测反馈；本次未另外采集实机启动或 ADB 日志。

## 主机名与 mDNS 回退（2026-10-25）

`bridge_ip` 保持兼容原有配置键，现在支持 IPv4、短主机名及完整域名。
短主机名先查询当前网络 DNS，失败或仅返回回环等不可用远端地址时查询
`NAME.local`；带点的完整域名不再追加后缀。SSE、语音上传、彩蛋请求使用同一解析入口。

主机测试覆盖域名格式、大小写/末尾点规范化、DNS 优先、失败回退、回环结果回退、
完整域名失败及结果释放。`scripts/test.sh` 全部通过（9 项 C 测试）；Mini/Mini3 均构建成功。

真实 RISC-V 仿真通过系统现有 mDNS 服务解析 `ubuntu-12700h.local`，获得
`192.168.31.108`，完成原有全部显示/语音初始化/联网验收。短名测试中，宿主 DNS
返回 `127.0.1.1`，固件排除它后通过 mDNS 找到 LAN 地址，完成 SSE、七种状态动画、
彩蛋打断、游戏结束和 bridge 重连。记录分别位于：

- `artifacts/lisem-e2e-09pbs6lg/result.json`：显式 `.local`。
- `artifacts/lisem-e2e-1kv9_0_f/result.json`：短主机名回退。

可用 `PET_BOARD=arcs_mini3 PET_TEST_HOST=192.168.31.108 PET_TEST_BRIDGE_NAME=ubuntu-12700h python scripts/verify-lisem.py`
复测；宿主需实际发布对应 mDNS 名称，`PET_TEST_PORT` 可隔离 bridge 测试端口。

随后通过 ADB 更新实机 `6d7b101a3b2bcba5`，AP/CP 全量读回一致，重启后应用 ADB
重新连接。CP SHA-256 为 `428c76885c6508ec78821ea20f41db2aec60b11a7fef887295d67058ea4b05db`；
日志为 `artifacts/mini3-flash/physical-mdns-adb.log`。Boot 未改动，未使用 TTL。

本机 bridge 同时保留 LAN 地址 `192.168.31.108:8787` 并启动 Tailscale 地址
`100.107.87.106:8787` 的监听，沿用同一 token，两个地址的鉴权健康检查通过。
Tailscale 路径仍要求设备网络提供相应 DNS 与路由；未把局域网 mDNS 测试当作 tailnet 端到端验收。

### 实机待机无彩蛋排查（2026-10-25）

局域网 bridge 原先仅记录启动，新增 `BRIDGE_LOG=1` 后记录状态、SSE 连接和已知
HTTP 路由的响应状态，不记录 token、请求头或正文。临时关闭会话监听、发布 idle 并
点播五子棋，持续观察 60 秒后恢复监听。北京时间 12:47:49，实机
`192.168.31.181` 请求 `/pet/next-id`，bridge 返回 200；用户仍观察到待机。
这确认了触发请求及服务端响应，不能证明实机完整接收、解析或读取 TF 成功。

Mini3 原固件及新增诊断日志的固件均通过独立 Lisem 验收，包括圣诞树彩蛋、
五子棋完整播放、工作事件打断和 bridge 重连。诊断版还使用与实机相同的
`ubuntu-12700h` 短名，通过 `.local` 回退完成全部验收：

- `artifacts/lisem-e2e-jb6thaes/result.json`：修改前固件，IPv4。
- `artifacts/lisem-e2e-1jdlg8xa/result.json`：诊断固件，IPv4。
- `artifacts/lisem-e2e-7a4hfj66/result.json`：诊断固件，实机短主机名。
- `artifacts/device-bridge.log`：实机 bridge 请求记录。
- `artifacts/extras-debug-tests.log`：16 项 Node、7 项 Python、9 项 C 测试通过。

固件新增待机计时、响应接收阶段、选择被状态变化丢弃、TF 打开/头部/索引失败日志。
仿真未复现实机故障，不能归因于 TF 缺失；诊断固件仅完成构建和仿真，按用户要求
尚未更新实机。随后用户确认实机已恢复正常。根因尚未确定，本次提交保留诊断能力，
不将恢复归因于尚未刷入实机的固件改动。
