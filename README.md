# listenm-codex 🐋

把 Codex 的工作状态变成桌宠动画，运行在 ListenM / Arcs-Mini 固件和 Lisem 仿真器上。

![Lisem 实际运行演示：Codex 状态切换与待机彩蛋](docs/animation-preview.gif)

*真实 RISC-V 固件的仿真器录屏帧，演示使用合成 Codex 事件驱动；不是网页效果图。*

## 功能

- **跟随 Codex 状态**：待机、思考、工作、回应、等待确认、小憩和错误，共 7 个核心动画。
- **无需安装 hook**：默认增量读取本地 Codex 会话 JSONL；也支持向 bridge 提交外部事件。
- **88 个动画**：联网待机时按需播放节日、游戏、饮食、音乐等彩蛋。
- **功能键互动**：单击随机播放五种回应，每轮不重复；320 ms 内双击播放“蓝鲸现世”。点击动画支持离线。
- **透明角色与海底背景**：统一缩放和位置，16 级透明度，240×240 RGB565 显示，目标 12 FPS。
- **农历节日优先**：内置香港天文台 2026–2031 年离线历表；2026 年与紫金山天文台资料逐日交叉校验。
- **断线恢复**：SSE 心跳、重连与有限历史补发。

已在 **Linux x86_64、Lisem 0.8.1、Arcs-Mini / LS2684** 上验证。**尚未在实体 ListenM 上烧录验证。** 当前固件约 14.14 MiB，面向 16 MiB Flash 板型，不能直接用于更小容量设备。

## 工作方式

```text
Codex Desktop / CLI 的本地会话日志
       ↓ 增量监听，只提取状态元数据
Node.js bridge ── HTTP SSE ──→ 固件状态调度 ──→ ST7789 屏幕
       └──── 按需下载彩蛋 ────→ 透明动画合成
                                  ↑
                             功能键单击 / 双击
```

默认日志监听不修改 Codex 配置，也不发送提示词、工具参数、回答或推理正文到设备。审批等待等状态需要外部事件补充；日志格式属于兼容性适配，并非稳定的公开订阅协议。多会话当前采用“最新事件优先”。

## 快速开始

需要 Node.js 20+、Python 3、Git、curl、CMake 和 C 编译器。SDK 工具链由其环境脚本安装，首次下载较大。转换后的素材已随仓库提供，普通固件构建不需要重新转换视频。

```bash
git clone https://github.com/undownding/listenm-codex.git
cd listenm-codex
./scripts/setup.sh

# 换成仿真器可访问的宿主 IPv4，例如实际存在的 Docker 网桥地址。
export PET_BRIDGE_IP=172.17.0.1
export BRIDGE_TOKEN="$(python3 -c 'import secrets; print(secrets.token_hex(24))')"
./scripts/build-firmware.sh -C

# 在同一终端启动 bridge，保持运行。
BRIDGE_HOST="$PET_BRIDGE_IP" node bridge/src/main.js
```

bridge 和固件使用相同 token。当前使用明文 HTTP，适合可信开发网络；Lisem 禁止访问宿主回环地址，不能把固件目标填成 `127.0.0.1`。

在另一个终端创建仿真器实例，以下 `DEVICE_ID` 替换为创建命令返回的 ID：

```bash
cd listenm-codex
.deps/Lisem/lisem --data-dir "$PWD/artifacts/manual" create \
  --board arcs-mini --name "Codex Whale"
.deps/Lisem/lisem --data-dir "$PWD/artifacts/manual" write DEVICE_ID \
  build/firmware/listenm-pet.bin

# Linux 图形桌面：打开设备窗口，再点击上电。
LISEM_DATA_DIR="$PWD/artifacts/manual" .deps/Lisem/lisem-desktop
```

镜像包含 Boot 和应用，写入 Flash 偏移 **0**。在设备窗口保持热点 `Lisem` 可用并开启互联网访问。也可无界面启动：

```bash
.deps/Lisem/lisem --data-dir "$PWD/artifacts/manual" start DEVICE_ID \
  --network --seconds 3600 --timeout 7200
.deps/Lisem/lisem --data-dir "$PWD/artifacts/manual" logs DEVICE_ID 0 --limit 16384
```

看到 `SSE connected` 后，在 Codex 中发起对话即可观察动画。空闲约 20 秒后播放彩蛋；点击设备「功能键」可随时触发互动。停止实例使用 `lisem ... shutdown DEVICE_ID`。

## 配置和接口

| 配置 | 用途 |
| --- | --- |
| `BRIDGE_HOST` / `BRIDGE_PORT` | 默认 `127.0.0.1:8787`；仿真器联调使用宿主网卡地址 |
| `BRIDGE_TOKEN` | bridge 鉴权与固件连接 token；非回环监听必填 |
| `PET_BRIDGE_IP` | 固件编译时的目标 IPv4 |
| `PET_WIFI_SSID` / `PET_WIFI_PASSWORD` | 默认开放热点 `Lisem` |
| `CODEX_SESSIONS_DIR` | 覆盖默认 `$CODEX_HOME/sessions` 日志目录 |
| `BRIDGE_WATCH=0` | 关闭日志监听，只接收外部输入 |

配置 token 后，所有 HTTP 接口都需 `Authorization: Bearer <token>`：

| 接口 | 用途 |
| --- | --- |
| `GET /health` | 连接数及事件序号 |
| `GET /events` | SSE 状态流 |
| `POST /ingest` | 接收一个受支持的 Codex 记录或 hook 对象，最大 64 KiB |
| `GET /pet/catalog` | 彩蛋目录 |
| `POST /pet/play/:id` | 点播下一次待机彩蛋，例如 `playing_gomoku` |
| `GET /pet/next` | 固件下载下一段彩蛋 |

完整状态映射、协议限制、农历节日表、素材重建和配置详见[使用指南](docs/guide.md)。

## 开发与验证

```bash
# Pillow 用于布局/截图检查，pdftotext 用于离线历表交叉校验。
python3 -m pip install -r scripts/requirements-media.txt
# Ubuntu / Debian：sudo apt-get install poppler-utils
./scripts/test.sh

# 已构建固件后执行；第一项需要 bridge 端口空闲。
python3 scripts/verify-lisem.py
python3 scripts/verify-button.py
```

单元测试无需 SDK 或仿真器；仿真验收使用独立实例。测试产生的日志、截图和设备身份放在被忽略的 `artifacts/`，不会提交。详情见[验证记录](docs/validation.md)。

| 目录 | 内容 |
| --- | --- |
| `bridge/src`、`bridge/test` | Node.js bridge 与协议测试，无第三方 npm 依赖 |
| `firmware/src`、`firmware/tests` | FreeRTOS/lwIP 固件、显示合成及 C 测试 |
| `firmware/assets`、`bridge/assets` | 可直接使用的核心、点击和彩蛋动画包 |
| `assets/backgrounds` | 海底背景原图、设备版和来源信息 |
| `bridge/data/calendar` | 离线历表、官方源文件及校验信息 |
| `scripts` | 依赖安装、构建、转换和验收工具 |
| `docs`、`licenses` | 使用文档、演示和第三方许可说明 |

## 许可与致谢

**原创代码采用 [MIT License](LICENSE)。第三方动画及其衍生演示不适用 MIT：上游允许用于开源项目，但禁止商业使用。** 具体范围和来源见 [THIRD_PARTY.md](THIRD_PARTY.md)。

- [dsh-whale-pet](https://github.com/luweiyabo/dsh-whale-pet)：动画目录、点击互动参考；上游素材署名为 [PC2005-cloud/dsh-pet contributors](https://github.com/PC2005-cloud/dsh-pet)。
- [ListenAI ARCS Mini SDK](https://cloud.listenai.com/CSKG836746/arcs-sdk/public/arcs_mini)：设备 SDK。
- [Lisem](https://github.com/LISTENAI/Lisem)：芯片、网络与显示仿真。
- [clawd-on-desk](https://github.com/rullerzhou-afk/clawd-on-desk)：Codex 日志旁路观察思路参考。
- 香港天文台及中国科学院紫金山天文台：离线农历数据与交叉校验资料。
