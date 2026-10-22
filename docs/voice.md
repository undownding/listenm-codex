# 唤醒词语音输入

语音版使用 ARCS SDK 的单麦唤醒算法（示例资源唤醒词为“小聆小聆”），录制用户发言，
经电脑 bridge 转写后投递给当前项目最近更新的 Codex 主会话。不安装 Codex hook。

## 启动 bridge

需要 Node.js 20+、已登录的 Codex CLI（支持 `app-server`）、以及语音转写 API 凭据。
Codex 使用本机已有登录和权限配置；转写通过独立的 `OPENAI_API_KEY` 调用，不能用 Codex 登录替代。

```bash
# 在仓库根目录运行；先在目标项目中创建至少一条 Codex 会话。
export BRIDGE_VOICE=1
export VOICE_CWD="$PWD"                 # 目标项目的绝对路径，必须与会话 cwd 一致
export BRIDGE_HOST=172.17.0.1            # 换成设备可访问的宿主 IPv4
export BRIDGE_TOKEN='替换为你生成的随机令牌'
export OPENAI_API_KEY='填写语音转写服务的密钥'
node bridge/src/main.js
```

| 配置 | 默认值 / 说明 |
| --- | --- |
| `BRIDGE_VOICE` | `1` 启用；不设置时保留原有只读状态 bridge |
| `VOICE_CWD` | 必填绝对路径，用于限定目标项目和 App Server 工作目录 |
| `CODEX_BIN` | `codex`，可指定 CLI 可执行文件绝对路径 |
| `VOICE_ASR_BASE_URL` | `https://api.openai.com/v1`，可替换为兼容转写接口的服务 |
| `VOICE_ASR_MODEL` | `gpt-4o-mini-transcribe` |
| `OPENAI_API_KEY` | 转写服务密钥，仅放电脑端 |
| `BRIDGE_TOKEN` | 启用语音时必填，设备和 bridge 必须一致 |

音频会发送到所配置的转写服务。bridge 不把音频或转写正文写入日志/文件；
提交后的文字成为 Codex 会话历史。bridge 的 SSE 仍只包含状态元数据。

## 构建语音固件

```bash
export PET_BRIDGE_IP="$BRIDGE_HOST"
export PET_WIFI_SSID='你的 Wi-Fi'
export PET_WIFI_PASSWORD='你的 Wi-Fi 密码'
./scripts/build-voice.sh
```

输出 `build/voice/listenm-voice-full.bin` 是 16 MiB 整包，Flash 写入偏移为 **0**。
`build/voice/listenm-pet.bin` 只是 CP 应用，偏移为 **0x800000**，不能单独写到 0。
本次开发构建使用测试网络和测试 token；实际使用必须按自己的网络、token 重新构建。

语音版与普通版采用不同 Flash 布局：

| 偏移 | 内容 |
| --- | --- |
| `0x000000` | SDK 单麦示例的 AP 固件（含启动程序） |
| `0x200000` | SDK 单麦算法资源 |
| `0x4D0000` | 算法 wrap 配置 |
| `0x800000` | 本项目 CP 语音和显示应用，最大 8 MiB |

构建脚本校验每段资源不超过对应分区。AP/CP 的共享 SRAM 布局按 SDK 单麦示例匹配。
SDK 路径可用 `SDK_ROOT` 覆盖，AP 镜像来自其
`arcs-sdk/samples/algorithms/wake_up/wakeup_single/res`，没有在本仓库另行复制二进制。

为适应 CP 分区，语音版内置动画抽取为 **6 FPS**，播放时长保持不变。
普通版保持 12 FPS。若使用 TF overlay，请复制 **`build/voice/voice-assets/`** 内的三个资源文件；
普通版 `firmware/assets/` 下的完整动画包与语音版索引不同，不能混用。
联网彩蛋仍按原时长播放，语音版每次显示跳过一帧。

## 输入规则

1. 检测到主唤醒词后开始录音，并向 `/voice/wake` 请求目标会话。
2. bridge 按 `updated_at` 降序选择 `VOICE_CWD` 下未归档的主会话，排除子代理来源；
   返回一次性 capture ID。此后即使其他会话更新，目标也不改变。
3. 设备采集 16 kHz、单声道 PCM16LE；累计至少 200 ms 有声后，连续 1 秒低音量结束。
   5 秒内没有足够语音则取消，最长录制 30 秒。这里采用音量阈值端点检测，非语义 VAD。
4. bridge 转写后通过 `thread/resume`、`turn/start` 提交。
   若该会话已有本 bridge 管理的活动轮次，则用 `turn/steer` 追加输入。
5. 空音频、重复上传、过期 capture、转写错误均不会启动新会话；不自动重试投递，
   防止超时但实际已收到时执行两遍。设备日志会显示提交成功、失败或取消。

会话选择以 bridge 处理唤醒请求时为准。capture 60 秒过期；同一时间只允许一次录音/上传。
没有符合条件的会话时直接失败，不自动创建。默认音量阈值为 600（PCM 平均绝对值），
可用 `./scripts/build-voice.sh -DPET_VOICE_THRESHOLD=800` 调整。

App Server 是 bridge 启动的独立进程。恢复同一会话不保证桌面端即时刷新，也不能可靠识别
其他进程正在执行的轮次；使用时避免桌面端和 bridge 同时执行同一会话。
需要人工审批或交互的问题不会由 bridge 自动批准。CLI 异常退出后需重启 bridge。
本实现按要求未做桌面端专门联调，也尚未在实体设备上验证麦克风/唤醒效果。

## HTTP 接口

所有接口要求 `Authorization: Bearer <BRIDGE_TOKEN>`，仅在启用语音时提供。

| 请求 | 内容 / 返回 |
| --- | --- |
| `POST /voice/wake` | 空 body；返回 `{id, threadId}` |
| `POST /voice/:id/audio` | 原始 PCM16LE，最大 960000 字节；返回 `{status:"submitted", threadId, turnId}` |
| `POST /voice/:id/cancel` | 取消尚未使用的 capture，立即允许下一次唤醒 |

HTTP 200 只代表输入已提交，不代表 Codex 已执行成功。失败返回 4xx/5xx；
尤其提交超时的结果可能未知，检查目标会话后再决定是否重新发言。

## 验证

`./scripts/test.sh` 包含 bridge 的语音路由与模拟 App Server 传输测试。
`./scripts/build-voice.sh` 检查 SDK 编译、链接及整包分区界限，不调用实际转写服务、
不向真实 Codex 会话发送测试消息，也不自动烧录。

接口参考：[Codex App Server](https://learn.chatgpt.com/docs/app-server)、
[OpenAI 文件转写](https://developers.openai.com/api/docs/guides/speech-to-text)。
