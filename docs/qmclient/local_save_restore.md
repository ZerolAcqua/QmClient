# 本地存档恢复

## 已确认行为

- 同地图提示 `ddnet-saves.txt` 中包含完整双人名字的存档，允许跨服务器提示。
- `/qm Yes` 恢复最新一条；`/qm Yes 序号` 选择其他记录，序号按提示排序。
- 本体和分身名字能对应时保留对应关系，否则按存档顺序分配并主动改名。
- 确认后连接分身，等待改名生效，复用仅有本体和分身的队伍或加入空队，再读取存档。
- `/qm No` 取消本次恢复；忽略提示不会触发任何操作。同一次进图仅提示一次。
- 手动 `/load` 与自动恢复均只在确认成功后删除同地图、同存档码的本地记录，失败保留。
- 不完整的旧记录仍显示普通存档提示；名字无法确定的记录不进入自动恢复列表。

## 恢复流程与边界

- `/qm` 只由本地处理，支持大小写混用，不发送到服务器。只有确认后才修改名字、连接分身或组队。
- 存档列表按保存时间从新到旧排序，相同地图和存档码的重复行只显示一个候选项；删除时一起清理这些重复行。
- 组队前等待服务器回传的名字和角色状态；主号进队后邀请分身，再等待两人进入同一队。命令按步骤间隔发送。
- 仅使用空队或恰好包含本体和分身的现有队伍；队伍被其他玩家占用时停止恢复。
- 准备阶段最多等待 60 秒，读档结果最多等待 30 秒。断线、换图、拒绝或超时会停止自动操作；已发送的 `/load` 仍以服务器结果为准。已修改的名字和队伍不会自动还原。
- 0.7 连接沿用现有客户端能力：若需要在线改名则提示用户先用存档名字重连。分身尚未连接时可以先设置名字再连接。
- `Player` 表头的旧文件只记录保存者，因此保守地只显示普通提示；`Players` 记录须能明确解析出两个不同名字。包含分隔符歧义、过长名字或缺失队员的记录不自动恢复。

## 读档成功判定

兼容服务器的 `Loading successfully done` / `存档载入成功` 消息，只接受服务器消息，并匹配当前地图、队伍和待处理读档请求。

新版 DDNet 的成功分支没有发送成功聊天消息（见 [DDNet teams.cpp](https://github.com/ddnet/ddnet/blob/master/src/game/server/teams.cpp)），因此也读取服务器快照中的恢复计时：请求前必须未开始计时，恢复后的起跑时间必须比发出请求早至少两秒。普通重新开跑不满足此条件；已有计时、旁观其他队伍、请求重叠或没有可用计时证据时保留记录。

服务器关闭计时、读档后立即死亡或结果超时等无法确认成功的情况会保留记录。不会用“没有收到错误”作为删除依据。

删除保持原 CSV 表头、其他记录和换行不变，不改变存档文件格式。先写入临时文件，再替换原列表；替换失败时保留恢复数据并报告清理失败。

## 实现与验证记录

- 实现入口：`src/game/client/components/qmclient/local_saves.h`，由 `CTClient` 接入提示和自动恢复，聊天发送路径记录读档请求，客户端收包路径处理本体和分身的服务器结果。
- `src/test/qm_chat_interactions_test.cpp` 新增 12 项行为测试，覆盖确认命令、CSV 保留、地图隔离、排序去重、名字对应、转义、成功判定、过期与重叠请求、恢复顺序、队伍占用和比赛边界。先补测试代码，再实现；按用户要求未编译、未执行这些测试。
- 只读代码审查：本次范围内未发现剩余阻断项。普通玩家聊天不能确认读档成功；删除没有跨地图兜底；换图和离线会清理旧请求。

2026-09-14 验证结果：

| 检查 | 结果 |
| --- | --- |
| `python qmclient_scripts/gate/check_gate.py --mode quick --report-json-path tmp/qm_local_saves_gate.json` | FAIL：10 项通过、1 项失败；格式检查报告范围外的 `perf_diagnostics.h`、`perf_logging.h`、`background_particles.h`、`statusbar.cpp` 等文件问题。没有进行构建或测试。 |
| `clang-format --dry-run --Werror src/game/client/components/qmclient/local_saves.h src/test/qm_chat_interactions_test.cpp` | PASS。 |
| 本次所改客户端文件与测试文件的 `git diff --check` | PASS。 |
| `python qmclient_scripts/languages_qmclient/extract_strings.py` | 完成，退出码 0。 |
| `python qmclient_scripts/languages_qmclient/generate_all.py` | 完成，生成 12 种语言。 |
| `python qmclient_scripts/languages_qmclient/validate.py` | FAIL：全仓提取缓存过期、实时连接功能的 11 个翻译条目缺失，以及 `qmclient.toml` 的 7 处条目间空行问题。 |
| `python qmclient_scripts/languages_qmclient/review_duplicate_entries.py --show-groups 0 --show-unused 0` | 完成，退出码 0。 |
| 本次 15 条提示的维护源、占位符与运行时语言条目核对 | 12 种语言全部通过；结果见 `tmp/qm_local_saves_i18n_scope.json`。 |

日志位于 `tmp/qm_local_saves_gate.log`、`tmp/qm_local_saves_validate.log`、`tmp/qm_local_saves_generate.log`、`tmp/qm_local_saves_duplicates.log`。全仓失败项来自当前工作区的其他改动，未扩大本次修改范围去处理。

验证缺口：尚未进行实际客户端连接、在线改名、服务器读档和文件替换的运行验证；新增 C++ 测试也尚未执行。
