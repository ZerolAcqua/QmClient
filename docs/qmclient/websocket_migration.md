# 自有服务改用 WebSocket

2026-09-14 用户确认实现并部署，入口为 `wss://qmclient.icu/ws`。

## 范围与行为

- 在线识别、远程粒子、语音在线状态、开发者认证、头衔名单、游玩时长和服务端时间、新功能广播统一走长连接。
- 新客户端删除这些功能的 HTTP 定时查询、定时上报和断线回退。重连后重新发送身份及当前状态，重新接收完整快照。
- 玩家身份和配置变化立即上报；短租约通过 WS 保活续期。断线后认证标记按现有租约失效。
- 语音音频保留 UDP。第三方 DDNet 查询、下载和用户主动发起的赞助码兑换、资料修改、广播发布保留原方式。
- 用户补充确认：编辑器协作同步也迁移到 WS，只改变传输，保留房间、revision 递增及地图应用规则。
- 既有 HTTP 接口继续服务已发布客户端；新客户端不会调用这些接口进行定时同步。

## 实现安排

- 中心 Node 服务提供 WS 入口，复用已有开发者、头衔和广播业务对象与存储。
- 头衔由中心进程统一承载，部署时迁移其监听入口，沿用原数据目录，避免两个进程同时写同一数据库。
- 语音服务提供仅允许回环访问的内部 WS 通道。中心服通过该长连接提交识别状态、接收识别快照；兼容 HTTP 上报产生的变化也主动推送，不在服务器内部轮询 HTTP。
- 保留后台线程与主线程派发模式，客户端传输更换为支持证书校验的 Rust WS/WSS 后端；使用同一 JSON 解析树应用快照，过滤换服前的旧消息。
- 补充协议、订阅/重连、身份隔离、过期与无 HTTP 回退测试代码。按仓库要求不执行客户端编译或测试；执行 quick 源码门禁及必要的部署检查。语音服务部署需要构建其服务端二进制。

## 状态

客户端源码迁移完成，本次功能按 MMP 推进至 3.6 系列；服务端已于 2026-09-14 部署。没有生成或安装新客户端二进制，旧客户端仍会使用兼容 HTTP 接口，需更新构建后切换。

## 只读审查

Findings（实现期间发现并已修复）：

- 附带 libwebsockets 缺少 TLS，无法承担官方 WSS；改为 tungstenite/rustls 后端，客户端不再依赖服务端的 WEBSOCKETS 构建选项。
- 原退出流程可能清空尚未发送的 stop；工作线程现先发送出站队列，再在有界时间内关闭连接。
- 编辑器 JSON writer 会附带末尾换行，直接 pop_back 会留下错误 JSON；已按对象结束位置插入业务数据。
- 主通道 upgrade 处理器原先会截断编辑器入口；已按路径分派，两个入口分别限制消息大小。
- 主通道现在承载全服在线名单，接收上限调整为 8 MiB，与内部语音通道一致，编辑器仍为 32 MiB。

结论：完成传输、状态落地、租约与协作房间逻辑的只读核对；没有剩余的已确认代码阻断项。以下未执行项仍是交付限制，不能视作验证通过。

## 部署记录

- SSH 主机：`ubuntu@42.194.185.210:22`。9987 为语音 UDP 和原识别 HTTP 监听端口。
- 语音进程：`voicesrv.service`，程序和源码位于 `/home/ubuntu/QmClient_Service`。新增仅允许回环地址访问的 `/qm/realtime`，UDP 包处理未改变。
- 中心进程：`qmclient-center-server.service`，路径 `/home/ubuntu/Q1menG_Client/tclient_scripts/qmclient_center_server`，继续监听 8080。
- 中心服务 drop-in：`/etc/systemd/system/qmclient-center-server.service.d/30-websocket.conf`，指定原称号目录、内部 WS 和回环代理信任。既有开发者认证文件、新闻目录和时长数据库环境配置保持使用。
- 原称号服务 `qmclient-titles.service` 已停止并禁用自启动。原 `/home/ubuntu/qmclient-titles/data` 由中心服务使用，数据结构没有迁移。
- Nginx 已引入 `/etc/nginx/snippets/qmclient-realtime.conf`，提供 `/ws`、`/ws/editor`；原称号 HTTPS 路由转发至 8080。
- 备份：`/home/ubuntu/qmclient-ws-backup-20260914`，包含旧程序、应用文件、systemd 配置、Nginx 配置和数据备份。
- 暂存产物：`/home/ubuntu/qmclient-ws-stage-20260914`。暂存服务已停止；生产端口继续为 UDP 9987、TCP 9987/8080/443。

回退时先停止中心和语音服务，恢复备份中的旧程序、应用文件及 Nginx 配置，移除新增中心 drop-in 和 WS snippet，执行 daemon-reload，再启动中心、语音与原称号服务并恢复称号自启动。称号数据格式未变，正常回退保留当前数据，不覆盖部署后的用户写入。

## 验证证据

| 检查 | 结果 |
| --- | --- |
| 暂存 `npm ci --ignore-scripts --no-audit --no-fund` | 安装成功，包含锁定的 ws 依赖 |
| 语音服务 `cargo build --release --manifest-path /home/ubuntu/qmclient-ws-stage-20260914/voice/Cargo.toml` | release 产物构建成功并部署；Tokio 固定 1.48.0，更新并保存 Cargo.lock |
| `node --check`（中心应用和新增 WS 模块） | 语法检查通过 |
| 暂存部署脚本 `probe_realtime.js ws://127.0.0.1:18080/ws` | 六类初始消息、时长 stop 确认、协作创建/加入/推送/退出通过 |
| 暂存识别链路检查 | 主 WS 上报经内部 WS 进入语音进程，返回 users 推送；粒子/语音字段存在且不含 IP |
| 公网 `probe_realtime.js wss://qmclient.icu/ws` | 主服务和编辑器协作检查通过；最终中心服务切换后再次通过 |
| 本机 Python SSL 连接公网两个入口 | 证书及域名验证通过，TLS 1.3、HTTP 101、Sec-WebSocket-Accept 匹配 |
| `nginx -t` 与 systemd 状态 | 配置有效；语音、中心、Nginx active；原独立称号进程 inactive |
| 语音/中心 healthz、新闻与称号兼容 HTTPS 接口 | 均返回 200 |
| `cargo metadata --format-version 1` | 依赖解析和锁文件更新成功，没有执行客户端构建 |
| 本次 C++ 文件 `clang-format --dry-run --Werror`，新增 Rust 文件 `rustfmt --check` | 通过 |
| `python qmclient_scripts/gate/check_gate.py --mode quick` | 10 项通过、1 项失败；全仓格式干跑被其他正在修改的文件阻断，完整输出位于 `tmp/ws_quick_gate.txt` |

已补充 C++ 协议解析、传输可用性及 TLS 验证约束测试，以及 Node 初始快照、换服、身份固定、协作推送和 Rust 内部识别测试。按照仓库约定，未执行游戏编译、testrunner、npm test 或 cargo test，也未做真实游戏客户端与真实凭据的联机验证。

