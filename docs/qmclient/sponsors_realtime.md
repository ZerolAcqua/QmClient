# 赞助名单实时更新

## 已确认的行为

- 名单使用独立的 `sponsors_draft.md`，沿用「新功能」的开发者草稿发布、中心服实时推送和离线缓存。
- 赞助卡片保留金色姓名、逗号分隔及自动换行；二维码和赞助码兑换保持原行为。
- 每个 Markdown 列表条目对应一位赞助者，提醒人数与显示名单共用解析结果。
- 名单推送同时更新文字、换行缓存和卡片高度，不需要重新打开页面。
- 赞助频道独立于新闻频道，版本、草稿和缓存互不覆盖。

## 维护与发布

初始名单在 [sponsors_draft.md](sponsors_draft.md)，按原顺序迁移现有 73 位赞助者。

1. 将该文件复制到客户端保存目录的 `qmclient/sponsors_draft.md`。
2. 每个姓名独占一行，例如 `- 喵不一`；支持 `-`、`*`、`+` 或数字加点的列表前缀。
3. 在赞助卡片的开发者区域点击「Reload」重载草稿，再点击「Publish」。「Open folder」打开上述保存目录。
4. 服务端确认成功后，在线客户端立即收到名单；断线重连时收到最新名单。

标题、说明和空行不计入人数；姓名中的 Markdown 符号按字面保留，重复条目按独立条目计数。解析最多 400 位、64 KiB；服务端默认发布上限与新闻一致为 16 KiB。

发布权限沿用 `qmclient/developer_token.txt` 和服务端 `NEWS_PUBLISH_DEVELOPER_IDS`。普通用户只能刷新当前名单。重载草稿不会直接修改公开名单；发布进行中不能再次重载或发布。

## 数据与部署

- 客户端缓存：保存目录下的 `qmclient/sponsors_cache.json`。
- 发布接口：`POST /api/v1/sponsors/publish`，成功响应包含服务端确认的 Markdown 和版本。
- WS：独立 `sponsors` 事件；首次连接、重连和手动刷新取得当前内容，发布后广播更新。
- 服务端数据：`SPONSORS_DATA_DIR` 指定目录内的 `sponsors.json`；默认 `sponsors_data/sponsors.json`。
- 断网保留最后一次成功的名单。无缓存时等待首次同步；服务端尚未发布时显示空名单，不再编译内置姓名。
- 较旧的推送或发布响应不覆盖更新版本；没有正文或版本的无效消息不清空名单。

中心服和 nginx 需要部署新增的赞助路由，随后首次发布迁移后的 MD。仅修改仓库源码不会改变线上服务。详见 [中心服说明](../../qmclient_scripts/qmclient_center_server/README.md)。

## 验证记录

当前共享工作区版本为 `3.7.1`；保留并行任务对版本文件的更新。本次先补回归测试代码，再实现名单解析、客户端接入和中心服频道；按开发期约定不编译、不运行 C++ 或 Node 测试。

只读审查发现新增测试曾误用不存在的 `json_integer_get`，已改为仓库现有的 `json_int_get`；客户端、页面与服务端审查未发现其他确定问题。

quick 门禁首次检查发现格式问题，修正测试文件混合换行与多余空行后复查通过：11 项通过、0 警告、0 失败。日志保存在 `tmp/sponsors_gate.log`，机器报告在 `tmp/sponsors_gate_report.json`。命令：

```powershell
python qmclient_scripts/gate/check_gate.py --mode quick --base-ref HEAD --report-json-path tmp/sponsors_gate_report.json --scope-report-path tmp/sponsors_gate_scope.json
```

尚未进行实际客户端界面、断网缓存恢复或发布响应乱序的运行验证；未部署中心服及 nginx，也未发布线上名单。

## 本地迁移记录（2026-09-17）

- 已将初始草稿原样复制到当前 Windows 用户的 `%APPDATA%/DDNet/qmclient/sponsors_draft.md`，复制后字节一致；73 位姓名及顺序与原内置名单一致。
- 用户确认仅发布需要开发者凭证，普通客户端自动接收更新。目标目录已有 `developer_token.txt`，本次未读取或修改凭证内容。
- 本次仅迁移本地草稿，未发布线上名单，未修改程序代码，未运行编译或测试。
