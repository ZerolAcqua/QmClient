# HTTP 结果读取断言排查

2026-09-17 排查 2026-09-16 客户端断言日志，并复审同批 HTTP 调用链。

## 根因

断言来自旧提交 `8f2f4b188` 的称号上报链路：请求超时后代码只判断 `Done()` 就继续调用 `StatusCode()`。`Done()` 在传输失败和取消时同样为真，但并不存在已完成的 HTTP 结果，读取结果因此触发 `Request not done` 断言。

当前工作区的称号上报已随实时通道迁移改走 WebSocket，该段崩溃代码不再存在；本条按「已定位、已随迁移消除」记录，不做回补。

## 只读审查 findings

- **P0（已消除）**：`Done()` 不等于 `EHttpState::DONE`，旧代码在超时分支读取 `StatusCode()`／`ResultJson()` 触发断言。当前 `UpdateTitleAuthentication()` 在读取结果前先判断 `State() != EHttpState::DONE`，进入失败分支、复位请求并返回。
- **P2（已修复）**：广播发布把 401／403／413 当成传输失败，`PUBLISH_DENIED` 与 `PUBLISH_TOO_LARGE` 两个分支永远不会命中。`QmNewsPublishDraft()` 与赞助名单发布现在都在派发前调用 `FailOnErrorStatus(false)`，让 4xx 响应原样返回后再按状态码分类；超时和取消仍归入普通失败。
- **P2（已修复）**：称号回归测试仍在检查已删除的 HTTP 名单代码，迁移后失效。已改为检查当前实时链路中的服务器地址匹配与无效名单保护。
- **迁移遗漏（已修复）**：实时 `titles` 消息缺少 `presences` 数组或 `server_time` 无效时，原先会被当成合法空名单，从而清掉尚未过期的称号。现在 `qm_realtime.cpp` 在解析消息时就判定 `m_HasTitles`：只有 `presences` 是数组且 `server_time > 0` 才算有效快照；无效消息直接丢弃，合法空数组仍作为权威结果清除已下线者。两种既有负载格式（`data` 内嵌与顶层）都保留。

除上述四条外，称号兑换、资源工坊、翻译、Spotify 与积分查询均在读取结果前判断传输状态，未发现同类漏判。

## 回归测试代码

- `src/test/qmclient_monitoring_test.cpp`
  - `QmHttpResults.NewsPublishPreservesErrorResponsesBeforeDispatch`：约束 `FailOnErrorStatus(false)` 出现在 `Http()->Run()` 之前。
  - `QmHttpResults.TitleFailureReturnsBeforeReadingResponse`：约束失败分支内出现 `reset()` 与 `return`、且不含 `StatusCode()`／`ResultJson()`，并约束读取结果的位置在失败分支之后。
- `src/test/qm_realtime_test.cpp`：覆盖有效快照、无效快照不置 `m_HasTitles`、合法空名单。
- `src/test/qm_title_style_test.cpp`：覆盖无效称号快照、合法空名单、正常称号与租约，以及对当前实时链路入口的源码约束。

## 验证证据

| 检查 | 结果 |
| --- | --- |
| `python qmclient_scripts/gate/check_gate.py --mode quick` | PASS，11 项通过、0 警告、0 失败 |

## 未执行项

- 未执行游戏编译、`testrunner`／`run_cxx_tests` 或游戏内复现；新增回归测试仅补全代码。
- 断言日志对应的旧构建未做实际复现，结论基于源码路径核对。
