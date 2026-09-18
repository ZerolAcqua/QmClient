# 服务部署与 IP 收口运维说明（2026-09-18）

## 背景与范围

起因：2026-09-18 下午有玩家被他人追踪，对方称「使用 Qm 会暴露您的 IP」。排查确认语音/身份服务的 `GET /qm/users.json` 无鉴权且返回 `last_ip`，记录按「游戏服务器地址 + 玩家名」索引，任何能访问 9987/tcp 的人都能把玩家名映射成 IP；实测 `http://42.194.185.210:9987/healthz` 从公网可达，说明该路径此前确实可被外部利用。

本次改动（两个服务仓库，均已提交在 `fix/drop-legacy-http-recognition` 分支）：

- **语音/身份服务**（`wxj881027/qmclient-voicesrv` 仓库，本地 `qmclient_scripts/qmclient_voice_server/`）：删除 `/qm/token`、`/qm/report`、`/qm/users.json` 三个 HTTP 识别接口与全部 IP 相关字段；识别数据只经回环 WebSocket `/qm/realtime` 提供给中心服，对外只剩 `/healthz`；`--identity-secret` 未配置时拒绝启动。
- **中心服**（`wxj881027/qmclient-center-server` 仓库，本地 `qmclient_scripts/qmclient_center_server/`）：删除 `/token`、`/report`、`/users.json` 及其 token 签发/校验、`AUTH_SECRET`、上报常量与失效辅助函数；识别上报与在线名单只走 `/ws`。

客户端不需要改动：新客户端本来就只用 `wss://qmclient.icu/ws`，从不调用这些 HTTP 接口。

## 一、部署前必须先配置 identity secret

语音服务新版本在 `--identity-secret` 使用默认占位值 `change-me-in-production` 或短于 16 位时**拒绝启动**。systemd 单元已经通过 `EnvironmentFile` 注入：

```sh
sudo install -d -m 0755 /etc/qmclient
sudo install -m 0600 -o root -g root \
  /path/to/qmclient-voicesrv/deploy/voicesrv.env.example /etc/qmclient/voicesrv.env
# 生成随机密钥并写入 IDENTITY_SECRET
sudo sh -c 'umask 077; printf "IDENTITY_SECRET=%s\n" "$(openssl rand -hex 32)" >> /etc/qmclient/voicesrv.env'
sudo sed -i 's/^IDENTITY_SECRET=REPLACE_WITH_RANDOM_SECRET$//' /etc/qmclient/voicesrv.env
```

注意：**轮换密钥会让所有 QID 变化**，客户端需要等中心服重新上报后拿到新 QID（在线状态会短暂重建）。因此密钥只在这类安全收口时更换一次，之后长期固定。

## 二、部署语音/身份服务

```sh
cd /path/to/qmclient-voicesrv
cargo build --release
sudo systemctl stop qmclient-voicesrv
sudo install -m 0755 target/release/voicesrv /opt/qmclient-voicesrv/voicesrv
sudo systemctl start qmclient-voicesrv
systemctl status qmclient-voicesrv --no-pager
```

（路径按实际部署调整；单元文件里的 `WorkingDirectory` 为 `/opt/qmclient-voicesrv`。）

## 三、部署中心服

```sh
cd /path/to/qmclient-center-server
sudo systemctl stop qmclient-center-server
# 同步 server.js / realtime.js / voice_realtime.js 等文件后：
npm ci --omit=dev
sudo systemctl start qmclient-center-server
```

`AUTH_SECRET` 已从代码与环境文件示例中删除；如果生产环境文件里仍有该变量，可以直接删掉。

## 四、主机侧收口 9987/tcp

新客户端语音走 `wss://qmclient.icu/ws/voice`（nginx → `127.0.0.1:9987`），识别只走回环 WebSocket。因此**公网不需要直连 9987/tcp**：

```sh
# 云安全组：删除 9987/tcp 的入站放行（保留 443 与 22）
# 主机防火墙兜底（二选一，按发行版）
sudo ufw deny 9987/tcp
# 或
sudo iptables -A INPUT -p tcp --dport 9987 ! -s 127.0.0.1 -j DROP
```

服务本身仍按 `--bind 0.0.0.0:9987` 监听：`sudo ss -lntp | grep 9987` 显示 `0.0.0.0:9987` 属正常，安全边界在防火墙/安全组，而不是 bind 地址。

## 五、部署后验证

```sh
# 1. 语音服务还活着
curl -s http://127.0.0.1:9987/healthz
#    期望 {"status":"ok","recognition_users":<数字>,"voice_peers":<数字>}

# 2. 旧 HTTP 识别契约已消失（本机）
for p in /qm/token /qm/report /qm/users.json; do
  printf '%s -> ' "$p"
  curl -s -o /dev/null -w '%{http_code}\n' "http://127.0.0.1:9987$p"
done
#    期望全部 404

# 3. 外网不再能连到 9987/tcp
curl -s -m 5 -o /dev/null -w '%{http_code}\n' http://42.194.185.210:9987/qm/users.json
#    期望连接失败/超时（不再是 200）

# 4. 中心服健康与旧路由
curl -s http://127.0.0.1:8080/healthz
curl -s -o /dev/null -w '%{http_code}\n' http://127.0.0.1:8080/users.json   # 期望 404

# 5. 实时通道端到端（部署自检脚本，用临时身份，不触碰真实凭据）
node deploy/probe_realtime.js wss://qmclient.icu/ws
```

还要观察一条业务指标：`/healthz` 的 `recognition_users` 应随玩家上下线继续变化；如果它长期为 0 或停止变化，说明客户端 → 中心 `/ws` → 语音服务这条链断了。

## 六、影响面与残留风险

- **仍走旧 HTTP 的第三方客户端会失效**。仓库里存在 `client_type: "arg"`（Arghena 分支）这类来源；如果某个分支仍用 `POST /report` + `GET /users.json` 上报/查询，需要在那个客户端里改用 `/ws`。当前 QmClient 客户端不依赖这些接口。
- **本次未动的旧 HTTP 入口**：中心服的 `/playtime/start|stop|query` 与 `/editor/collab/*`（新客户端分别走 `/ws` 的 `playtime` 消息与 `/ws/editor`）。它们同样属于「留给老客户端」的兼容面，确认无外部消费方后可以按同样方式删除。
- **未部署前一切照旧**：线上仍是旧二进制，`/qm/users.json` 仍会返回 IP。必须先完成第二、三节的部署。
- 中心服的 `/client/version` 保留（客户端当前不调用，可能被网站/启动器使用）。

## 证据

- 两个仓库的提交：语音服务 `8a0649b`、中心服 `3afc9d2`（分支 `fix/drop-legacy-http-recognition`）。
- 回到父仓库的补丁：`tmp/ip-fix/voice_drop_legacy_http.patch`、`tmp/ip-fix/center_drop_legacy_http.patch`（另有只含 IP 字段移除的 `*_ip_removal.patch`）。
- 测试：语音服务 `cargo test` 11 项通过、无编译警告；中心服 `node --test test/*.test.js` 36 项通过。
- 公网可达性实测：`http://42.194.185.210:9987/healthz` 在修复前返回 200（`recognition_users: 273`）。
