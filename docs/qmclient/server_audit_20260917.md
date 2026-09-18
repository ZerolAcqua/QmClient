# 2026-09-17 服务器检查与清理记录

## 范围与授权

- 主机：`42.194.185.210`，SSH 使用 `ubuntu` 和端口 22；9987 为语音 UDP / 识别 TCP 服务端口。
- 检查范围：最近 30 天部署变化、运行稳定性、清理候选及本地客户端代码对应关系。
- 用户随后取消 GitHub PR 查询，本记录不包含 PR 判断。
- 用户确认删除下列两处暂存目录、两个安装包，以及超过 30 天的 journald 归档。保留业务数据、当前程序、回滚备份和最近 30 天日志。

## Findings

1. 旧 IP 站点仍将 `/api/` 转发至未监听的 19090，将 `/api/v1/auth/register` 转发至未监听的 19091。通过 Nginx 本机入口携带 `Host:42.194.185.210`，只读 GET 注册路径实测返回 502。近 30 天现存 Nginx error.log 中，上游 19090 连接失败 1226 条，最新为 9 月 17 日 17:04:05；19091 为 6 条，最新为 9 月 14 日 18:11:47。计数取自最终探测前，不等同完整 502 请求统计。此次未改动这些路由。
2. 中心服务近 30 天有 8 条 `BadRequestError: request aborted`，为请求中断，未发现因此崩溃或自动重启。Nginx 到中心 8080 的连接失败日志最新为 9 月 14 日 22:57:26，与当前中心进程启动时刻一致；现存日志中此后未见同类失败。
3. 客户端源码支持已部署功能，但不能据此确认用户正在运行的客户端二进制也已更新。迁移记录注明 9 月 14 日部署时未生成或安装新客户端二进制。

## 近 30 天主要变化

- 9 月 9 日：部署头衔服务。9 月 14 日头衔逻辑接入中心进程，独立 `qmclient-titles.service` 当前为 inactive / disabled；中心仍使用 `/home/ubuntu/qmclient-titles/data`。
- 9 月 11 日：增加 Markdown 新功能公告接口。当前公告为版本 1，更新时间 9 月 14 日 23:42:33（UTC+8），正文 7259 字节，包含 Demo 剪辑、旁观查找 CP、本地双人存档、地图进度、拖尾与描边、实时同步与协作等栏目。
- 9 月 14 日：中心新增 `/ws`，统一推送识别、开发者状态、头衔、时长、时间和公告；新增 `/ws/editor` 承载编辑器房间同步。
- 语音进程新增仅允许回环访问的 `/qm/realtime`，中心通过内部 WebSocket 共享识别状态；语音音频仍走 UDP。
- 以上日期依据现存部署文档、文件时间、备份差异及服务启动记录；服务器源码存在直接部署的未提交改动，不能仅靠服务器 Git 日志还原全部部署。

## 健康检查证据

- `qmclient-center-server`、`voicesrv`、`nginx`、`mysql`、`waline` 均为 active / running；当前运行周期 `NRestarts=0`，`systemctl --failed` 为空。
- 中心从 9 月 14 日 22:57 运行，语音从同日 22:49 运行。近 30 天现存相关服务和内核日志未见服务崩溃、OOM 或自动重启证据。
- 服务器 uptime 约 214 天；检查时内存可用约 1.9 GiB / 3.6 GiB。现存 9 月 9–17 日 sysstat 日均 CPU idle 为 95.80%–97.03%，1 分钟 load 日均 0.06–0.09，平均换入换出为 0。
- GET 中心 `/healthz`、官网、公告和头衔查询返回 200；主通道、编辑器通道和语音内部通道 WebSocket 握手通过。内部通道收到 `users` 快照，检查时有 247 条记录。
- 探测未发送业务 `hello`、创建协作房间、发布公告或修改头衔；未进行音频通话及多人协作端到端测试。
- 当前域名证书有效至 2026 年 11 月 10 日。健康结论为检查时状态及现存日志表现，不代表完整 30 天可用率。

## 已完成清理

删除前检查服务、定时任务、Nginx 配置引用及进程 exe / cwd / fd；两处暂存目录和旧 ZIP 均无进程引用。正在运行的 `target/release/voicesrv` 已识别并保留。

| 已删除路径 | 清理前约占用 |
| --- | ---: |
| `/home/ubuntu/QmClient_Service_codex_stage_20260808` | 858 MiB |
| `/home/ubuntu/qmclient-ws-stage-20260914` | 326 MiB |
| `/home/ubuntu/QmClient_Service/voicesrv.zip` | 269 MiB |
| `/home/ubuntu/qmclient-ws-deploy-20260914.tar.gz` | 56 KiB |

- 执行 `sudo -n journalctl --vacuum-time=30d`，工具报告释放 3.5 GiB 归档日志；journal 总占用降至 393.8 MiB。
- 根分区已用空间：24,187,813,888 → 18,824,945,664 字节；净释放 5,362,868,224 字节，约 5.0 GiB。磁盘使用率 40% → 32%。
- 清理后五项服务 PID 均未变化，仍 active / running、`NRestarts=0`；中心健康接口返回 `ok:true`。未重启服务。
- 保留全部业务数据、当前构建目录、9 月 14 日回滚备份及其他历史备份。

## 客户端对应关系

| 服务端能力 | 本地客户端实现位置 |
| --- | --- |
| 主实时通道 | `src/game/client/components/qmclient/qm_realtime.cpp` 默认 `wss://qmclient.icu/ws`；`qmclient.cpp` 的 `UpdateQmRealtime` 实际收发 |
| 玩家识别、语音状态、开发者与头衔 | `qmclient.cpp` 的 `hello` / `presence` 上报及 `users` / `developers` / `titles` / `title_profile` / `title_status` 处理 |
| 游玩时长、服务端时间 | `qmclient.cpp` 的 `playtime` / `time` / `stop` 处理 |
| 公告及发布界面 | `qmclient.cpp` 接收 `broadcast`；`menus_qmclient.cpp` 解析显示 Markdown，发布仍使用 HTTP |
| 头衔兑换和资料修改 | `qmclient.cpp` 的 `/api/v1/titles/redeem`、`/api/v1/titles/profile` 请求及对应菜单 |
| 编辑器协作 | `src/game/editor/editor.cpp` 使用 `wss://qmclient.icu/ws/editor`，实现 create / join / leave / push 和主动快照 |
| 语音音频 | `src/engine/shared/config_variables_qmclient.h` 默认 `42.194.185.210:9987`；音频继续 UDP |

上述实时功能已在本地 HEAD `3413f1376`（2026-09-15，3.6.1）中。当前工作区版本为 3.6.3，并有未提交的始终启用实时通道、头衔快照校验等调整，不能混作已发布状态。

本次未修改客户端代码，未编译或运行代码测试；新增此记录为纯文档，按仓库约定人工核对，不运行代码 gate。
