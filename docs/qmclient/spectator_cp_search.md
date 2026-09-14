# 旁观 HUD 查找 CP

## 已确认的行为

- 在 `+spectate` 选择 HUD 底部加入 CP 编号输入、减号、加号、查找/下一个按钮；在线与 Demo 旁观均可使用。
- 编号范围为 1–255，增减按钮在两端循环；点击输入框选中当前编号，回车执行查找。
- 默认按住右 Shift 打开 HUD 时，主键盘和小键盘的数字按键仍输入数字，不输入 Shift 符号，也不重复录入文本事件。
- 编辑编号时不触发 Demo 的数字跳转、回车暂停等快捷键。
- 查找范围沿用编辑器的传送层“查看”：包含同编号的普通传送入口、出口、CP 和 CTO；不包含不使用编号的 CFRM。
- 同编号按地图行顺序循环，沿用编辑器 10 格距离规则跳过附近格子；改号后从头查找。
- 找到后退出多视角/玩家跟随，切到自由视角并定位到目标格中心；在线等待自由视角状态生效后再移动镜头。
- 非法编号或无匹配时在 HUD 提示，保持当前旁观目标和镜头位置。
- 查找结果在本地图的 HUD 开关之间保留；重置或切换地图时清空。

## 实现与验证记录

- 先补全编号边界、传送格类型、循环顺序、10 格距离和无匹配场景的测试代码，再实现查找。
- 测试位于现有 `src/test/qm_new_ui_menu_branch_test.cpp`，复用已登记的测试入口。
- 验证范围为 quick 源码卫生门禁与只读代码审查；按用户要求不执行编译或测试。
- 版本：本功能将 `QMCLIENT_VERSION` 从工作区的 `3.4.4` 更新为 `3.5.0`；收口时并行任务已继续更新至 `3.5.1`，保留该更新。

### 只读审查

- Findings：本次范围内未发现阻断问题。已核对传送格类型、编号切换/循环、输入焦点释放、Shift 数字输入、Demo 快捷键冲突和等待自由视角后定位的调用顺序。
- 结论：功能代码、5 组测试代码和翻译已补齐；运行时行为尚未经过客户端编译或手动操作验证。

### 检查结果（2026-09-14）

- `python qmclient_scripts/gate/check_gate.py --mode quick --report-json-path tmp/qm_spectator_cp/gate.json --scope-report-path tmp/qm_spectator_cp/gate_scope.json`：退出码 1，10 项通过、1 项失败。格式检查失败位于本次范围外的 `players.cpp`、`skins.cpp`、`skins7.cpp`、`tclient/background_particles.h`、`tclient/statusbar.cpp`、`render.cpp`、`skin.cpp`、`skin.h`。完整输出见 `tmp/qm_spectator_cp/gate.log`。
- `python qmclient_scripts/fix_style.py -n src/game/client/components/spectator.cpp src/game/client/components/spectator.h src/game/client/components/qmclient/spectator_tele_search.h src/game/client/components/menus_demo.cpp src/test/qm_new_ui_menu_branch_test.cpp src/game/version.h`：退出码 0，本次涉及的 6 个 C/C++ 文件通过格式检查。
- `extract_strings.py` 与 `generate_all.py`：退出码 0；新增 4 条英文 source key 和 12 种语言译文，已逐项确认 48 条运行时译文存在。简中生成时按现有规则规范空格。
- `python qmclient_scripts/languages_qmclient/validate.py --incremental`：退出码 1，工作区其他文案缺译及 7 处 TOML 块间空行问题未通过；7 处格式问题对应的提醒文案在任务开始前已存在。本次 4 条文案不在失败项中。输出见 `tmp/qm_spectator_cp/i18n_validate.log`。
- `python qmclient_scripts/languages_qmclient/review_duplicate_entries.py --show-groups 0 --show-unused 0`：退出码 0，无重复 key 或空白译文。
- 本次编辑的源码与 TOML 保留原 UTF-8 BOM 状态和换行方式；工作区其他任务正在编辑的内容予以保留。

### 后续手动验收

- 在线自由视角、跟随玩家、多视角分别查找已有编号；松开 HUD 键后镜头仍落在目标。
- 同编号多个区域连续查找并循环，改号从头查找，输入 0/256/无匹配编号时保持原视角。
- 按住默认右 Shift 输入主键盘数字，回车查找；Demo 播放与暂停时均不误触数字跳转或暂停快捷键。
- 0/16/32/64/128 位玩家的 HUD 布局、触屏点击以及开关动画。
