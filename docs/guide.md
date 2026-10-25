# 完整使用指南

将本机 Codex 活动转换成 SSE，让 ListenM / Arcs-Mini 固件接收。
屏幕固定位置播放鲸鱼动画，随 Codex 状态实时切换；UART 同时保留事件日志。
动画参考 [dsh-whale-pet](https://github.com/luweiyabo/dsh-whale-pet)，跳过拖拽、
屏幕移动、漫游和抛掷交互。

```text
Codex Desktop / CLI
  → 本地 sessions/*.jsonl 增量记录
  → Node.js bridge：白名单状态映射
  → HTTP SSE /events
  → Lisem 虚拟 Wi-Fi → 固件 lwIP TCP → SSE 解析 → 状态调度 → ST7789 动画显示 + UART
```

已在 Linux x86_64、Lisem 0.8.1、Arcs-Mini（LS2684）上完成真实 Codex
事件接收和 bridge 重启恢复验证。固件是运行于仿真器 RISC-V CPU 的 SDK
应用；`build/host/listenm-receiver` 仅用于宿主调试，不替代仿真器验收。

以下命令均从仓库根目录执行。

## 快速运行

需要 Node.js 20+、Python 3、Git、curl、CMake 和 C 编译器。
截图验收需要 Pillow（`python3 -m pip install Pillow`）；动画包已随项目提供，
日常构建不需要 FFmpeg。
SDK 下载较大，首次构建会通过 SDK 的 `env.sh` 安装工具链到 `~/.listenai`。

```bash
./scripts/setup.sh

# 使用仿真器可达的宿主 IPv4。Linux Docker 网桥地址也是可用选项。
export PET_BRIDGE_IP=172.17.0.1
export BRIDGE_TOKEN="$(python3 -c 'import secrets; print(secrets.token_hex(24))')"
./scripts/build-firmware.sh -C
python3 scripts/prepare-tf.py /你的/TF卡挂载目录
# 编辑 config.ini，使 bridge_ip、bridge_token 与上述环境变量一致。
```

`PET_BRIDGE_IP` 必须换成本机实际存在的地址（可用 `hostname -I` 查看）。
Lisem 正式 CLI 禁止宿主回环访问，不能填 `127.0.0.1` 或以网关
`192.0.2.1` 绕到回环服务。token 在 bridge 和固件之间必须一致。当前是明文 HTTP，适用于可信开发网络。

Arcs-Mini 构建产物：`build/voice/listenm-voice-full.bin`，包含 SDK AP（含启动程序）和 CP 应用，写入
Flash 偏移 **0**。它是本项目的独立固件，不是原语音助手固件的增量更新，
不要写到原项目的 `0x600000` 应用分区。

上述偏移用于仿真器加载。实机首次写入后出现黑屏，尚未验证成功；
Arcs-Mini 整包会替换原厂 boot，且不包含 USB Recovery。再次更新可能需要 UART
ROM 下载方式。不要把 ADB 的传输进度完成提示当作 Flash 写入成功，必须检查
最终退出状态并读回校验。详见[排查记录](validation.md#实机首次烧录与仿真复查2026-10-22)。

Mini3 使用 `PET_BOARD=arcs_mini3 bash scripts/build-firmware.sh`，输出
`build/voice-mini3/listenm-voice-full.bin`。它包含配套 USB Recovery Boot，
与上述 Mini 的分区布局不同：Boot `0x0`、AP `0x40000`、CP `0x600000`。
整包仅用于仿真器；实机应按分区写入，避免覆盖原有配置区域。
首次安装配套 Boot 后，运行 `python3 scripts/flash-mini3-adb.py --serial SERIAL`
即可更新 AP/CP，脚本在完整读回一致后才重启。已经进入配套 Recovery 时加
`--recovery`。没有 TF 卡时也会启动 ADB；配置与动画仍需之后在 TF 上准备。

终端一启动 bridge，默认观察已有 Codex Desktop / CLI 会话的新事件：

```bash
BRIDGE_HOST="$PET_BRIDGE_IP" BRIDGE_TOKEN="$BRIDGE_TOKEN" \
  node bridge/src/main.js
```

终端二创建独立仿真器设备（将创建结果中的 ID 填入后续命令）：

```bash
.deps/Lisem/lisem --data-dir "$PWD/artifacts/manual" create \
  --board arcs-mini --name listenm-codex
.deps/Lisem/lisem --data-dir "$PWD/artifacts/manual" write DEVICE_ID \
  build/voice/listenm-voice-full.bin
.deps/Lisem/lisem --data-dir "$PWD/artifacts/manual" start DEVICE_ID \
  --network --seconds 300 --timeout 600
.deps/Lisem/lisem --data-dir "$PWD/artifacts/manual" logs DEVICE_ID 0 --limit 16384
```

默认连接开放热点 `Lisem`，固件自动获取 IP 并连接 bridge。在 Codex 发起
一轮对话，UART 0 会出现：

```text
[pet] DHCP 192.0.2.2
[pet] SSE connected
[pet] event=codex id=<epoch>:<sequence> data={"version":1,"source":"codex",...}
```

持续取日志可使用 `lisem --json logs ... --cursor N` 返回的游标，或
`lisem uart-receive`。运行有时间上限；结束使用 `lisem shutdown DEVICE_ID`，
bridge 使用 Ctrl+C 停止。

## 自动验收

```bash
./scripts/test.sh
# 先停止同地址/端口的 bridge；此脚本自行启动并清理独立实例和 bridge。
python3 scripts/verify-lisem.py
```

自动验收使用**合成的 Codex JSONL 记录**，验证真实仿真固件的 DHCP、SSE、
七种状态动画、非空白屏幕、连续帧变化和 bridge 重启后的 reset/重连。结果、
屏幕截图、`preview.gif` 与 UART
保存在 `artifacts/lisem-e2e-*/`；任何阶段未成功会非零退出。
实际 Codex 事件验收记录另见 [验证记录](validation.md)。

## 配置

| 变量 | 默认值 / 用途 |
| --- | --- |
| `BRIDGE_HOST` | `127.0.0.1`；仿真器联调改为宿主网卡地址 |
| `BRIDGE_PORT` | `8787`；设备配置的 bridge_port 必须一致 |
| `BRIDGE_TOKEN` | Bearer token；监听非回环地址时必填 |
| `CODEX_HOME` | `~/.codex` |
| `CODEX_SESSIONS_DIR` | `$CODEX_HOME/sessions`；可用于远程挂载目录或测试 |
| `BRIDGE_WATCH` | 设为 `0` 关闭文件监听，只接收 `/ingest` |
| `SDK_ROOT` | `.deps/arcs_mini`；完整 SDK 应用仓库路径 |
| TF `config.ini` | 网络、token、录音阈值、唤醒词与模型路径，见 `config/config.example.ini` |
| `LISEM_BIN` | 自动验收所用 Lisem 可执行文件路径 |

固件网络参数从 TF 卡 `config.ini` 读取；改变参数后重启即可。修改 Boot 配置时
使用 `-C` 清理构建，避免 SDK 子工程沿用旧配置。

## 事件与接口

- `GET /health`：bridge 进程状态、连接数、事件序号。不是 Codex 活跃保证。
- `GET /events`：SSE；`event: codex`，JSON 数据格式见下。
- `POST /ingest`：接收单个 Codex rollout、受支持的 app-server notification
  或 hook JSON 对象；最大 64 KiB。支持的记录转为状态，未知记录返回 202
  并忽略。此接口不执行命令，也不代替 Codex 审批。
- 配置 token 后，上述接口均要求 `Authorization: Bearer <token>`。

```json
{"version":1,"source":"codex","session_id":"<thread-id>","type":"function_call","state":"working","timestamp":"2026-10-20T03:25:36.440Z"}
```

`timestamp` 是 bridge 观察时间。状态为 `idle`、`thinking`、`working`、
`speaking`、`attention`、`sleeping`、`error`。发送白名单元数据，不发送
提示词、工具参数/结果、模型回答或推理正文。多会话以 `session_id` 区分。

bridge 每 500 ms 轮询，首次启动跳过旧内容；新建文件从头读取。JSONL 是
本地兼容性适配，并非 OpenAI 承诺稳定的公开订阅协议；没有日志的临时会话
无法从此来源观察。审批等待等状态需要外部 hook 向 `/ingest` 提交；本项目
不自动安装或修改 Codex hooks。也不自动附着到 Desktop 的内部 app-server。

SSE 每 15 秒发送注释心跳，保留最近 256 条事件、最多 16 个客户端。
通过 `Last-Event-ID` 补发；bridge 重启或历史过期时发送 `event: reset`，
再发送最新事件（若有），不会声称完整恢复丢失历史。队列不落盘。
慢客户端断开后重连，避免占用无界内存。

固件使用有界缓冲处理 TCP 任意分片、LF/CRLF/CR、注释、多行 data；40 秒
接收超时，断流后等待 2 秒重连。支持本 bridge 的 close-delimited HTTP
响应；不支持 HTTPS、DNS、重定向或第三方 chunked SSE 服务。收到事件后
输出完整 JSON，并通过 `pet_ui_event()` 校验 version/source/state 后更新动画。

## 屏幕动画

| Codex 状态 | 动作 | 屏幕文字 |
| --- | --- | --- |
| idle | 呼吸待机 breathing | IDLE |
| thinking | 深度思考 deep_thought | THINKING |
| working | 写代码 coding | WORKING |
| speaking | 挥手回应 cheerful_wave | SPEAKING |
| attention | 侧耳倾听 attentive_listening | WAITING |
| sleeping | 原地小憩 quick_nap | SLEEPING |
| error | 惊吓反应 jump_scare | ERROR |

所有动画共用源画布裁剪区域、缩放比例及位置基准，避免状态切换时角色忽大
忽小；同时保留气泡等较宽特效所需空间。

渲染目标 12 FPS，每段动画循环 10 秒。同状态事件保持播放进度，普通切换
至少展示 700 ms，等待/错误立即切换。断线后显示“离线” 和待机动画，重连
后跟随新事件。渲染在独立 FreeRTOS 任务中，网络回调只更新状态，不执行刷屏。
多会话共用一个桌宠，当前按最近收到的事件显示，不做多会话优先级仲裁。

屏幕为 Arcs-Mini 的 240×240 ST7789，SPI0 DMA 输出 RGB565。核心、点击动画和背景
都存储在 TF 卡，CP 应用约 1.2 MiB，AP/CP 完整镜像为 16 MiB。必须插入包含配置和资源的 FAT TF 卡。
背光使用 95% PWM（本次 Lisem/SDK 组合的 100% 静态输出路径会黑屏）。

动画资源有独立的非商业使用条款，来源、署名、格式与重建命令见
[动画包说明](../firmware/assets/README.md)，不由参考项目的 MIT 代码许可覆盖。

## 参考与版本

- [ARCS Mini SDK 应用仓库](https://cloud.listenai.com/CSKG836746/arcs-sdk/public/arcs_mini)，
  固定提交 `23fca91e13dbd0f2c81eb2f69a6b337953021637`。
- [Lisem](https://github.com/LISTENAI/Lisem)，使用 0.8.1 Linux x86_64 发布包，
  安装脚本校验 SHA256。
- [clawd-on-desk](https://github.com/rullerzhou-afk/clawd-on-desk)，参考其 Codex
  日志旁路观察思路；未复制其实现或安装其 hooks。
- [OpenAI Codex App Server](https://learn.chatgpt.com/docs/app-server)，适合未来
  由产品主动管理 Codex 会话时使用；当前默认路径观察已有聊天日志。

SDK 和 Lisem 属于外部依赖，保留各自许可证。本项目没有修改它们的源码。
ListenM 实物首次烧录后黑屏，仍未通过实机验收；当前通过范围是仿真器事件接收及屏幕动画。

### 待机彩蛋（88 个动画）

7 个 Codex 核心状态和新增 81 个动画均存放 TF 卡；彩蛋由 bridge 选择名称后在卡上读取，包括 20 个节日、9 个游戏，以及日常、饮食、音乐、魔术、季节、鲸鱼特效等。排除了原项目的拖拽、位移和转身分类。资源仍遵循原项目的素材授权，见 `licenses/whale-assets-upstream.md`。

联网且 Codex 空闲持续约 20 秒后，播放一个约 10 秒的彩蛋，再回到呼吸。工作、思考、等待授权等核心状态优先，状态切换会打断彩蛋；断网回到本地动画。每次只缓存一个动画（上限 1.75 MB），不增加 Flash 中的素材量。所有动画复用现有 208×196 容器、源图裁切、统一缩放和定位，不再按单个动画的包围盒调整人物大小；超出固定视口的边缘特效会被裁切。

普通日期随机轮播、不连续重复；按 bridge 主机日期提高当季动画、万圣节（10 月 24 日起）、圣诞节（12 月 20 日起）的出现概率。游戏动画是人物玩游戏的演出，不包含交互游戏。

农历节日按北京时间（`Asia/Shanghai`）自动匹配，每次选择下一段彩蛋时重新判断，无需重启或联网查询日历。优先级为：手动点播 → 当天农历节日素材 → 原有季节加权/随机轮播。Codex 工作状态仍优先于所有彩蛋。节日有多个素材时避免连续重复；仅有一个时允许重复，避免下一次轮播落回无关动画。跨日后下一次选择使用新日期，正在播放的动画自然结束。

| 日期 | 优先彩蛋 |
| --- | --- |
| 除夕、正月初一 | 舞狮、红包、烟花、写福字、饺子、年糕 |
| 正月十五（元宵） | 汤圆、放天灯 |
| 五月初五（端午） | 粽子 |
| 七月初七（七夕） | 七夕乞巧 |
| 七月十五（中元） | 放河灯 |
| 八月十五（中秋） | 中秋赏月 |
| 九月初九（重阳） | 赏菊、重阳糕 |
| 腊月初八（腊八） | 腊八粥 |

除夕通过“次日为正月初一”判断，兼容腊月二十九和三十；闰月不重复触发普通月份的节日。优先查阅内置的香港天文台 **2026–2031 年离线历表**（2,191 天），其中 2026 年已用紫金山天文台年度资料逐日交叉校验，365 天全部一致。启动和运行时均不拉取日历；缓存范围外才使用 Node 自带 ICU 中国农历转换（需完整 ICU 支持）。清明属于节气，此次未加入自动日期触发，青团素材仍可随机播放或点播。更新后重启 bridge 即可生效，无需重新刷固件。

查询完整目录或点播下一次待机彩蛋（沿用 bridge 鉴权）：

```bash
curl -H "Authorization: Bearer $BRIDGE_TOKEN" http://127.0.0.1:8787/pet/catalog
curl -X POST -H "Authorization: Bearer $BRIDGE_TOKEN" \
  http://127.0.0.1:8787/pet/play/playing_gomoku
# 节日示例：decorating_a_christmas_tree、moon_festival、eating_zongzi
```

点播在下一次待机加载时生效，不打断正在进行的 Codex 工作。当前面向单设备，多个设备共享 bridge 的待播选择。设备使用 `GET /pet/next-id` 获取名称后读取 TF 卡 `extras/` 中的包；`GET /pet/next` 保留给兼容客户端。额外素材源位于 `bridge/assets`（约 90 MiB），使用 `prepare-tf.py` 复制到卡上；缺失时核心动画仍正常工作。重建素材：

```bash
python3 scripts/pack-extras.py
```

需要已有 `.deps/dsh-whale-pet`、FFmpeg（libvpx-vp9）及 Pillow，与核心素材转换工具一致。修改核心素材布局后应重新生成额外素材，保持相同的缩放与位置。


历表缓存与原始资料保存在 `bridge/data/calendar/`，包含来源 URL、SHA-256 和校验结果。手动重建缓存：

```bash
# 使用仓库内原始资料重建，并强制执行逐日交叉校验
python3 scripts/cache-calendar.py
# 可选：手动重新下载同一批官方资料再重建（不是运行时任务）
python3 scripts/cache-calendar.py --refresh
```

重建需要 Python 3、`pdftotext`（Ubuntu 包名 `poppler-utils`）；重新下载另需 `curl`。Bridge 日常运行仍只需要 Node。缺失日期、日期不连续、PDF 格式变化或两份 2026 年历表不一致时，脚本会报错并保留已有 JSON 缓存。

### 功能键点击互动

Arcs-Mini 功能键（PB4，低电平按下）在松手时触发点击，20 ms 消抖；长按期间不重复触发。参考原 dsh 插件 `handleClick`：

- 单击：使用原插件完整点击回应池：开心跃动（`happy_hop`）、害羞惊讶（`shy_surprise`）、傲娇生气（`tsundere_pout`）、挠痒咯咯笑（`ticklish_giggle`）、元气挥手（`cheerful_wave`）。每轮随机打乱五个动画，播遍后再洗牌，且跨轮不连续重复；实体功能键不再区分头/身体/尾巴位置。与原插件一样，抵达目标后的 `arrival_wave` 不属于点击回应池。
- 双击：两次释放间隔小于 320 ms，立即切换 `blue_whale_appears`（蓝鲸现世），与原插件一样第一击无需等待双击窗口结束。
- 点击优先于所有 Codex 状态和待机彩蛋；播放期间继续接收 Codex 状态，播完回到 700 ms 呼吸缓冲，再恢复最新状态。后续点击可再次打断当前回应。
- 点击动画从 TF 卡读取，断网仍可响应；点击素材包约 5.27 MiB，不占固件 Flash。

在 Lisem 设备窗口按下并松开「功能键」即可验收。命令行可使用 `lisem button DEVICE_ID function press` / `release`。离线自动验收脚本为 `python3 scripts/verify-button.py`，使用独立设备，不占用 bridge 端口。

重建素材时，先运行 `scripts/pack-whale.py` 和 `scripts/pack-extras.py`，最后运行 `python3 scripts/pack-clicks.py`。后者复用已转换的同尺寸素材，并校验核心布局版本一致。


### 海底背景与透明动画

背景原图位于 `assets/backgrounds/whale-undersea.png`，设备使用 `firmware/assets/background.rgb565`。动画从透明 WebM 重新生成，采用 WHP2 格式：RGB565 调色板索引与独立透明度平面；16 级透明度保留柔和边缘和原本不透明的白色衣服。固件每帧先绘制背景，再合成角色，最后绘制状态文字。

核心、点击、联网彩蛋均使用同一透明格式。升级时必须同时更新固件、TF 卡和 bridge 目录，并重启 bridge；旧 WHP1 固件不能播放 WHP2 彩蛋。PSRAM 堆设为 5 MiB，为唤醒模型、录音、单个彩蛋及显示缓冲留出空间。背景转换工具是 `scripts/pack-background.sh`，动画转换顺序保持核心 → 额外彩蛋 → 点击包。
