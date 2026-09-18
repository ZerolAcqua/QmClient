# 识别服务不再保存与下发客户端 IP（2026-09-18）

## 起因

用户 2026-09-18 下午跑 solo 时被他人追踪（对方在聊天中称「使用 Qm 会暴露您的 IP 地址」）。只读排查后在自有服务里找到与该说法一致的唯一通道：

- 语音/身份服务 `GET /qm/users.json` 无鉴权，返回的每条在线识别记录里带 `last_ip`，而记录按「游戏服务器地址 + 玩家名」索引，因此任何能访问该端口的人都能把玩家名映射成 IP。
- 该字段只是被服务端保存下来序列化，`RecognitionRecord.last_ip` 除 `/qm/users.json` 投影外没有任何消费者；只有中心服回环 WS 快照那一份把它清空（旧 `realtime.rs` 的 `user.last_ip = None`）。
- 默认 `--bind 0.0.0.0:9987`，且 9987 是本服务的 UDP 语音端口 + TCP 识别端口（见 `docs/qmclient/server_audit_20260917.md`）。是否真的从公网可达只能由主机侧确认，仓库里无法判定。

本次按用户确认的范围「彻底删掉 IP 字段」执行：语音进程不再接收、保存或返回客户端来源地址。

## 改动

语音与身份服务（`qmclient_scripts/qmclient_voice_server/`，该目录此刻已是独立 git 仓库）：

- `src/main.rs`：删除 `RecognitionRecord.last_ip`、`RecognitionUser.last_ip`、`apply_report` 的 `remote_ip` 参数与 `handle_report` 的 `remote_addr` 参数（连带移除上报路由上的 `warp::addr::remote()`）；`/qm/token` 路由仍保留远端地址用于既有的 token ↔ IP 绑定。
- `src/realtime.rs`：内部 `recognition` 指令不再解析 `ip` 字段；快照不再需要逐条清空 `last_ip`。
- 新增两个回归用例：`users_response_never_exposes_source_ip`（`/qm/users.json` 命中记录时响应体不含 `last_ip`）、`snapshot_never_carries_source_ip`（回环 WS 快照不含 `last_ip`，也不含上报里出现过的地址字面量）；`internal_report_reuses_identity_and_announces_changes` 保留「指令里带 `ip` 也被忽略」的输入。
- `README.md`：字段表删除 `last_ip` 行，中心服→服务端的推送示例去掉 `"ip"`，测试清单更新为 12 个用例。

中心服（`qmclient_scripts/qmclient_center_server/`，同为独立仓库）：

- `voice_realtime.js`：`Events.Report(Body)` 不再向语音进程发送 `ip`。
- `realtime.js`：两处 `Recognition.Report(...)` 不再传 `Session.Ip`；`Session.Ip` 保留，因为中心服自己仍用它做 token 绑定与头衔并发 IP 上限。
- `test/realtime.test.js`：新增「识别上报不再向语音进程转发来源地址」，断言上报调用没有第二个参数。

## 保留项与边界

- 客户端一行未改：客户端从不发送 IP 字段，也不解析 `last_ip`，所以本次改动对客户端与协议兼容性为零影响（旧客户端继续工作）。
- 中心服侧 IP 仍是内存内瞬时数据：限流、`/token` 绑定（TTL 300 秒）、头衔 IP 上限；未落库、未写日志。
- 游戏服务器本身知道你连接的 IP，这是 DDNet/Teeworlds 协议固有性质，与本次改动无关。

## 验证

- `cargo test`（`qmclient_scripts/qmclient_voice_server`）：12 passed / 0 failed，包含两个新增用例。
- `node --test test/*.test.js`（`qmclient_scripts/qmclient_center_server`）：36 passed / 0 failed，包含新增的上报不带来源地址用例。
- `node --check realtime.js|voice_realtime.js|test/realtime.test.js`：通过。
- 语义复查：`/qm/users.json` 现在只返回 `server_address`、`player_name`、`player_id`、`dummy`、`client_type`、`type`、`qid`、`client_id`、粒子/语音开关与 `last_seen`；Rust 源码里除两个断言字符串外不再出现 `last_ip`。

## 提交

- 语音与身份服务：`9375922 fix(voicesrv): 在线识别不再接收、保存与下发客户端 IP`（该目录已是独立仓库）。
- 中心服：`65b52da fix(center-server): 识别上报不再向语音进程转发客户端 IP`。
- 两个仓库提交后工作区干净；补丁留档于 `tmp/ip-fix/voice_server_ip_removal.patch`、`tmp/ip-fix/center_server_ip_removal.patch`。

## 未完成项与风险

- **未部署**：线上语音服务仍是旧二进制，修复要重新构建并重启 `voicesrv` 后才生效；在此之前该接口仍会返回 IP。
- 未能证明对方确实使用了这条通道（也可能是服主/rcon 或其它手段），只能确认该通道此前可用且现已移除。
- 服务端代码在本任务进行期间被并行任务拆分为多个独立仓库（各目录含自己的 `.git`，父仓库里对应路径处于暂存删除状态）。20:46:21 该并行进程用 git 还原过这两个仓库的工作区，第一次手工应用的改动被整体丢弃；本记录对应的改动是重新应用后**立即提交**的版本，因此不再依赖未提交的工作区状态。父仓库的子模块接线仍由那次拆分决定，本次未参与。
- 本次改动全部落在父仓库跟踪范围之外的两个子仓库，故未跑父仓库的 `check_gate.py`；验证以两个服务的原生测试套件（`cargo test`、`node --test`）为准。

## 后续（2026-09-18 当晚）

本页记录的提交后来被并行进程的 `git reset --hard origin/main` 丢弃（这两个目录是 GitHub 克隆）。相关改动已在分支 `fix/drop-legacy-http-recognition` 上重新落地并扩大范围：`/qm/users.json`、`/qm/token`、`/qm/report` 与中心服的 `/token`、`/report`、`/users.json` **整体删除**，识别数据只经回环 WebSocket，IP 不再被接收、保存或下发。提交为语音服务 `8a0649b`、中心服 `3afc9d2`；部署与主机侧收口步骤见 [service_deploy_and_ip_hardening.md](service_deploy_and_ip_hardening.md)。
