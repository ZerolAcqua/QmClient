# 旁观 HUD 好友置顶与分组

## 已确认行为

- 位置：`+spectate` 旁观选择 HUD 的玩家列表，即 `src/game/client/components/spectator.cpp` 的 `CSpectator::OnRender()`。该列表只在旁观模式出现，仓库既有文档也把这一处记作「旁观 HUD」（见 `docs/qmclient/spectator_cp_search.md`）。
- 好友集中在列表最前面：显示顺序由纯函数 `qm_spectator_friends::BuildFriendFirstOrder()` 给出（`src/game/client/components/qmclient/spectator_friend_priority.h`）。它是稳定分区：好友保持原有相对顺序排在前面，其余玩家保持原有相对顺序紧随其后，不做二次排序。
- 好友单独成组：好友组首行之前画分组标题「好友」，其余玩家首行之前画分组标题「其他」。两条文案都复用仓库既有 key：`Localize("Friends")`、`Localize("Others")`，未新增源串。
- 好友判定沿用快照缓存的 `CGameClient::CClientData::m_Friend`（由 `src/game/client/gameclient.cpp` 的 `OnNewSnapshot()` 按 `Friends()->IsFriend(名字, 战队, true)` 刷新），与本列表里的好友名颜色、爱心图标同源。好友判定规则、`cl_friends_ignore_clan` 语义、`m_Foe` 等均未改动。
- 名单里没有好友时行为与改动前完全一致：不重排、不画任何分组标题。
- 全员都是好友时只画「好友」标题，不画「其他」标题（没有可分隔的另一组）。
- 分组标题只让出竖直高度（12–15 像素），不占用网格槽位：列数、每列玩家数、鼠标命中范围与改动前一致，不会因为标题多出一列把玩家挤出面板或点不到。
- 好友分区只改变绘制顺序：同一个 DDRace 队伍的底色框按显示序列逐段闭合，队伍被好友分区切开时画成闭合的两段，不会跨分区粘连。行高、Tee 图标、名字、旗帜、多视角圆点、点击选中都跟随显示序列，因此点击目标与看到的行一一对应。

## 位置判定（只读调查）

- `src/game/client/components/hud.cpp` 里没有旁观玩家列表：`RenderSpectatorHud()`（~6818 行）只画右下角「Follow/Free-View/AUTO」状态条，`CHud::OnRender()` 的旁观分支只画被观战者的血量/状态/移动信息，`RenderScoreHud()` 只画前两名，`RenderSpectatorCount()` 只画观战人数。
- `src/game/client/components/tclient/` 没有 HUD 玩家列表（`player_indicator` 是世界内指示圈，`statusbar` 是本地状态条，`menus_tclient` 是设置页）。
- `src/game/client/components/scoreboard.cpp` 是按住计分键显示的计分板：它按名次/队伍排序，是所有模式共用的计分板，不是旁观模式专属列表；改它的排序会破坏名次语义。
- 结论：旁观模式专属的玩家列表只有 `CSpectator::OnRender()`（`+spectate` 选择面板），本功能的改动落在这一处。

## 手动验收

下列步骤为待执行清单，**不代表已通过**；本次按约定未编译、未运行测试。

1. 在线进入有好友的服务器并旁观，按住 `+spectate`：好友（含战队匹配的好友）全部排在列表最前面，好友块上方显示「好友」标题，其余玩家排在「其他」标题之后。
2. 让一名好友与若干普通玩家处在同一 DDRace 队伍：检查队伍底色框在好友块与其余玩家块里各自闭合，行高、Tee 图标、名字纵向位置与改动前一致。
3. 名单里没有好友时打开面板：顺序与标题数量与改动前一致（不出现任何分组标题）。
4. 全员都是好友时打开面板：只出现「好友」标题。
5. 在 8 / 16 / 17 / 32 / 33 / 64 人（含满员）的服务器分别打开面板：不新增列、玩家不被挤出面板背景、末行不与底部「查找 CP」输入栏重叠、鼠标选中与多视角圆点位置正确。
6. Demo 旁观（含 0.7 Demo）重复步骤 1 与 5：列表可用、点击切换目标正常。
7. 切换 `cl_message_friend`、增删好友名单后重开面板：分组与标题跟随好友名单变化，爱心图标与标题同时出现。
8. 开启动画（`qm_extra_animations`）后开关面板：标题与玩家行一起淡入淡出，无残留。

## 实现记录

### 改动文件

- `src/game/client/components/qmclient/spectator_friend_priority.h`（新增）：好友优先显示顺序的纯函数，两次线性扫描、只写下标，无分配、无加锁。
- `src/game/client/components/spectator.cpp`：`CSpectator::OnRender()` 先收集非观战玩家到栈上显示序列并算出好友数量，绘制循环改按显示序列遍历；新增 `DrawGroupTitle()` 局部 lambda 画「好友」「其他」两条标题与分隔线；DDTeam 底色框的前后队伍扫描改按显示序列计算；`aNextDDTeam` 只对显示序列下标写入与读取。
- `src/test/qm_new_ui_menu_branch_test.cpp`：新增 `QmSpectatorFriendPriority` 测试组（既有旁观 HUD 用例所在文件）。
- `docs/qmclient/spectator_friend_priority.md`：本文件。

### 测试

- `QmSpectatorFriendPriority.StablePartitionPutsFriendsFirst`：好友与其余玩家各自保持原有相对顺序（原下标 1 在 3 之前、0/2/4 依次在后）。
- `QmSpectatorFriendPriority.HandlesAllFriendsNoFriendsAndEmptyList`：全员好友、没有好友、空列表与负数量四种边界，空列表不写下标。
- `QmSpectatorFriendPriority.FullServerOrderIsAPermutationWithSingleBoundary`：64 人满员时输出是原下标的排列（每个下标恰好一次），且只有一个分区边界（边界前全是好友、边界后全不是好友）。
- `QmSpectatorFriendPriority.SpectatorHudRendersFriendsFirstWithOwnGroupTitles`：结构断言——先算顺序再按 `apDisplayPlayers[aDisplayOrder[` 绘制、两条标题与 `i == FriendCount` 边界存在、换行判定只保留玩家行那一处（标题不占槽位）、好友判定取自 `m_Friend` 缓存且渲染函数内不出现 `Friends()->IsFriend(` / `Foes()->IsFriend(`。

### 性能

- 每帧只做一次 `MAX_CLIENTS`（64）规模的收集加两次计数（纯函数），全部写在函数栈数组上：不分配、不加锁、不查好友表、不做字符串处理。
- 好友判定读 `m_aClients[ClientId].m_Friend`（快照刷新时写好的 bool），与同一循环里原本就读取的字段同一份缓存。
- 没有新增排序调用：稳定分区是两次 O(N) 扫描，随面板绘制一起进行；面板关闭时不执行（`OnRender()` 提前返回）。
- 缓存失效条件：不需要额外缓存——`m_Friend` 随 `OnNewSnapshot()` 更新，显示序列每帧按当前快照重算。

### 只读审查

- Findings：
  - 原循环里 10 处 `m_apInfoByDDTeamName[i]->m_ClientId` 引用（9 行）必须一并改成显示序列的 `pInfo`，否则会按旧下标画出错误玩家；已全部替换。
  - `aNextDDTeam` 逆序扫描与 `OldDDTeam` 回扫原本按原始下标，好友分区会切开同队玩家；已改为按显示序列计算，队伍底色框在分区处闭合。
  - 标题若占用网格槽位，会在 8/16/32/64 人等整列边界把最后 1–2 名玩家挤到新列（超出面板背景与鼠标范围）；改为只让出竖直高度，不占槽位。
  - 标题高度上限取 15 像素：小布局（≤16 人、行高 60）最坏情况下末行下沿停在 CP 查找栏上沿，不再压上去。
  - 爱心图标绘制块（好友爱心，`FONT_ICON_HEART` + `m_ClMessageFriendHeartColor` + `IconX += IconSize - 2.0f`）按要求原样保留，未做任何改动或删除；好友分组标题用的是好友名颜色 `m_ClMessageFriendColor`，与爱心颜色配置相互独立。分组标题与爱心图标并列存在，不冲突（标题表达分组，爱心表达单个玩家）。
- 结论：本次范围内未发现阻断问题；渲染路径新增开销为栈上线性扫描。运行时观感与鼠标命中尚未经编译或实机验证。

### quick 门禁

- `python qmclient_scripts/gate/check_gate.py --mode quick`：退出码 1，10 项通过、0 警告、1 失败、0 跳过、0 不适用，有效结果 FAIL（改动收尾后复跑两次，两次汇总数字相同）。
- 唯一失败项是格式干跑检查里的 `src/game/client/components/menus_settings.cpp`（`code should be clang-formatted`，两次分别报在 3638–3640 与 3639 行），属于本次范围外、且由并行任务改动的文件。
- 格式阶段按批次执行并在首个失败批次返回，因此 `src/test/qm_new_ui_menu_branch_test.cpp` 本次未被格式阶段覆盖；`src/game/client/components/spectator.cpp` 与新增的 `src/game/client/components/qmclient/spectator_friend_priority.h` 属于已执行批次且没有格式报错。
- 头文件 guard 检查、未使用头文件检查、配置变量检查、设置页统一 UI 迁移合同、ruff、shellcheck 均通过。原始输出见 `tmp/b2_spectator_friends_gate.log`。

### 未执行项

- 未编译 `game-client`，未运行 `testrunner` / `run_cxx_tests` / `run_rust_tests`。
- 未运行 i18n 脚本（`extract_strings` / `generate_all` / `validate` / `review_duplicate_entries`）；本功能复用既有 key，没有新增源串。
- 未修改 `src/game/version.h`，版本号未按 MMP 变动（任务要求不改该文件，交给统一收口）。
- 未做游戏内手动验收（本文「手动验收」为待执行清单）。
