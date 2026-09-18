# 自有服务 WebSocket 通道

QmClient 3.6 系列源码使用专用 WS 通道同步自有服务，后台同步不再发 HTTP 请求，也不在断线时回退 HTTP。

## 入口与范围

| 通道 | 用途 |
| --- | --- |
| `wss://qmclient.icu/ws` | 在线识别、人数分布、语音在线状态、开发者标记、称号、游玩时长、服务器时间、广播 |
| `wss://qmclient.icu/ws/voice` | 语音音频独立二进制连接（源码已迁移，待配套部署） |
| `wss://qmclient.icu/ws/editor` | 编辑器协作房间操作和地图快照推送；进入协作时建立独立连接 |
| `ws://127.0.0.1:9987/qm/realtime` | 中心进程与语音进程之间的内部连接，仅允许回环访问 |

语音音频源码已迁移到独立 WS/WSS，客户端与服务端均不再使用 UDP；本次未部署，线上状态不因此改变。切换要求见 [语音音频迁移](voice_websocket_migration.md)。DDNet、GitHub 等第三方查询及下载，以及用户主动进行的赞助码兑换、称号资料修改、广播发布，继续使用原传输方式。旧版客户端的 HTTP 接口保留兼容。

## 客户端传输

`src/engine/shared/websocket_client_rust.cpp` 在工作线程收发，通过队列交给主线程。`qm_websocket.rs` 使用 tungstenite 和 rustls 校验 TLS 证书链、有效期及域名，信任根来自 Mozilla。附带的 libwebsockets 没有 TLS，因此不再用于客户端 WSS；服务端 `WEBSOCKETS` 构建选项不影响客户端通道。

- 自有服务 WebSocket 始终启用，已删除“实时通道”开关及 `qm_websocket` 配置项；旧配置中的 `qm_websocket 0` 不再控制连接。空的 `qm_websocket_url` 使用官方地址。
- 子协议为 `qmclient-json`。开发者和称号凭据只发送到精确匹配的官方主通道。
- 断线指数退避重连，重新发送 hello 和当前玩家状态。旧连接队列清空，换服消息同时校验服务器地址。
- 主通道消息上限 8 MiB；编辑器地图消息上限 32 MiB。队列同时限制条数和字节，满队列通过重连获取新快照。
- 握手后使用非阻塞收发；退出时先发送已入队消息，再在有界时间内关闭连接。
- 保留旧的 `qm_websocket_allow_insecure_tls` 配置识别，但 WSS 连接拒绝绕过证书验证。心跳设为 0 时使用默认间隔。

## 主通道协议 v2

首次发送 `hello`，包含 `v: 2`、设备 `machine_hash`、时长 `client_id`、`player_name`、`server_address`、`session_id` 和最多两个 `players`。可信入口可附带 `title_token`、`developer_token`。有崩溃恢复标记时附带 `recovery_stop_at`，服务器沿用既有时长结算逻辑，然后确认新的 start。

客户端身份固定在首次 hello，后续 `presence` 仅更新所在服务器、玩家、配置和凭据。状态变化在下一次检查时发送，最长每 5 秒通过 WS 续租。退出发送 `stop`，主动刷新可发送 `news` 或 `subscribe_titles`。

下行消息采用 `{ "type": "...", "v": 2, "data": { ... } }`：

| 事件 | 内容 |
| --- | --- |
| `users` | 识别列表及服务器范围，不含 IP；客户端标记租约为 20 秒 |
| `developers` / `titles` | 当前服务器名单、签发及过期时间，复用既有校验和租约规则 |
| `title_profile` / `title_status` | 本人资料或认证错误，包括原有 IP 数量限制 |
| `playtime` / `time` | 已累计时长、会话时间和服务器时间 |
| `broadcast` | 版本号和 Markdown 内容，更新本地缓存 |
| `error` / `ping` / `pong` | 协议错误及心跳 |

首次握手发送完整快照；业务变化主动推送，服务器也定期下发租约和时间。语音进程通过内部 WS 接收识别上报、广播识别状态；此过程没有服务器内部 HTTP 轮询。

## 编辑器协作

上行格式为 `{ "type": "collab", "request_id": 1, "action": "join", "data": { ... } }`。动作包括 `create`、`join`、`push`、`leave`，业务字段与原房间 API 相同。

回复保留原业务字段，增加 `type: "collab"`、`request_id` 和 `status`。主动快照的 `request_id` 为 0。服务器共用原房间操作，保留四人上限、成员/房间过期规则、整图快照和每次上传递增 revision 的行为。

客户端按帧顺序处理上传确认与快照，不再执行定时 pull。重连后重新加入原房间取得最新快照，未确认的上传不跨连接盲目重放。关闭编辑器停止连接续租。

## 部署与检查

中心服务运行 `npm ci` 安装锁定依赖，Nginx 引入 `qmclient_scripts/qmclient_center_server/deploy/qmclient-realtime.conf`。中心服务使用原 `TITLE_DATA_DIR`，旧称号进程必须先停用，避免同一数据目录出现两个写入者；原认证文件、广播目录和时长数据库继续使用。

部署检查脚本：`node qmclient_scripts/qmclient_center_server/deploy/probe_realtime.js wss://qmclient.icu/ws`。脚本使用临时身份和房间，检查六类初始消息、时长停止确认、协作加入/上传/推送/退出，并验证接收方没有发出 pull。

本次部署结果、源码门禁和未执行项见 [迁移记录](websocket_migration.md)。
