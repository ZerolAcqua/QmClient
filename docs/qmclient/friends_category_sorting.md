# 好友分组拖动排序

好友侧栏中的好友、战队成员、自定义分组和离线分组均可调整顺序。

- 左键按住分组标题并移动至少 5 个 UI 单位后开始拖动，无需 Ctrl。
- 普通点击仍展开或收起分组；齿轮和右键仍打开分组管理。
- 拖动时临时隐藏分组内容，显示拖动轮廓和插入位置；靠近列表边缘可滚动。
- 在列表内松开按插入位置排序；在列表外松开取消。两者均恢复原来的展开状态。
- 正常保存客户端配置后，重启保留全部分组的顺序及对应展开状态。

顺序沿用好友配置的命令保存机制：先创建所有自定义分组，再输出 `qm_friend_category_move "分组名" 位置`。位置从 0 开始，包含内置分组，分组名沿用配置字符串转义。此命令不改变好友所属分组。

回归测试覆盖点击与拖动阈值、离开标题后继续拖动、释放时不误触展开、内置分组排序，以及顺序保存和恢复的代码契约。按本次要求只补充测试代码，不编译或运行测试。

## 拖动好友更改分组

- 按住普通好友条目并移动至少 5 个 UI 单位，即可拖到“好友”或自定义分组标题。
- 拖动期间临时隐藏好友行，保留原展开状态；目标标题绿色表示可移入，红色表示不可移入。
- 松开后沿用 `SetFriendCategory` 更改持久好友记录并刷新列表。在列表外、空白处、原分组或无效目标松开取消。
- “战队成员”和“离线”仍按匹配规则、在线状态自动归类，不作为拖入目标；战队匹配条目不能按普通好友拖动。
- 离线好友可更改归属，但仍显示在“离线”中，上线后进入所选分组。好友姓名、备注及所属战队不变。
- 保留单击选服、双击加入、右键菜单及复制、跟随、删除按钮。分组标题的拖动排序继续保留。

新增 5 个拖动状态测试，覆盖点击阈值、身份复制、来源限制、自动分组/原分组目标拒绝及离线来源移动。

## 验证记录（2026-09-17）

只读核心审查 findings：无。已检查鼠标占用与释放、点击和拖动区分、展开状态随分组移动、列表外取消、插入索引、边缘滚动及配置恢复。

- `python qmclient_scripts/gate/check_gate.py --mode quick --report-json-path tmp/friends_category_sorting_gate.json`：9 项通过、2 项失败。失败包含本次测试末尾多余空行及其他并行改动中的 `friend_enter_tracker.h`、`qm_realtime_test.cpp` 格式问题，另有 `menus_qmclient.cpp` 的设置页字体检查问题。本次多余空行已修正，未修改其他任务代码；完整日志为 `tmp/friends_category_sorting_gate.log`。
- `python qmclient_scripts/fix_style.py -n src/engine/client/friends.cpp src/engine/client/friends.h src/game/client/components/menus_browser.cpp src/game/client/components/qmclient/friends_category_drag.h src/test/qm_new_ui_menu_branch_test.cpp src/test/qmclient_monitoring_test.cpp src/game/version.h`：修正后通过。
- 本次涉及文件的 `git diff --check`：通过。
- `extract_strings.py` 与 `generate_all.py`：完成；本次两个提示 key 均已生成到 12 种运行时语言文件。
- `validate.py`：全仓校验未通过，报告表情和地图上传等其他改动的翻译缺失，以及 `menus_qmclient.cpp` 的 3 条中文 source key；本次两个 key 不在失败清单。日志为 `tmp/friends_category_sorting_i18n_validate.log`。
- `review_duplicate_entries.py --show-groups 0 --show-unused 0`：完成，重复 key 和空译文均为 0。

尚未进行编译、测试执行和客户端交互验证。

## 好友移动验证（2026-09-17）

只读核心审查 findings：无。另已复核头像提示区的鼠标占用，以及复制、跟随、删除按钮不启动拖动的边界。

- `python qmclient_scripts/gate/check_gate.py --mode quick --report-json-path tmp/friend_drag_move_gate_final.json`：最终 11 项通过，0 项失败。日志为 `tmp/friend_drag_move_gate_final.log`。
- `python qmclient_scripts/fix_style.py -n src/game/client/components/menus_browser.cpp src/game/client/components/qmclient/friends_category_drag.h src/test/qm_new_ui_menu_branch_test.cpp src/game/version.h`：通过。
- 本次涉及文件的 `git diff --check`：通过。
- `extract_strings.py`、`generate_all.py`：完成；新增拖动提示已生成到 12 种语言文件。
- `validate.py`：未通过，全仓仍有表情功能缺失翻译及 `menus_qmclient.cpp` 中 3 条中文 source key；本次新增 key 不在失败清单。日志为 `tmp/friend_drag_move_i18n_validate.log`。
- `review_duplicate_entries.py --show-groups 0 --show-unused 0`：完成，无重复 key、空译文和未使用候选条目。

本轮新增 5 个状态测试；遵照本次约定，未编译、未执行测试、未启动客户端做交互验证。
