# Demo 简易剪辑工具

## 剪辑 HUD 与显示选项扩展

- 用户确认：Demo 预览和视频导出使用独立显示设置，不影响正常游戏。选择保存在客户端配置，不写入 `.demo` 文件。
- HUD 使用圆角悬浮卡片、胶囊按钮和蓝色强调色；文件标题、时间轴、播放与剪辑操作分层。保留已有的快捷键、标记、拖动和区间预览。
- 显示面板提供方向键、强弱钩模式与范围，以及游戏 HUD、聊天开关。预览、裁剪导出窗口和视频导出窗口共用选择；方向键与强弱钩只读取快照并各自控制显隐，不覆写角色输入和瞄准方向。
- 实现顺序：先补配置隔离测试代码，再接入渲染与 HUD；按仓库约定不编译、不运行测试，仅运行 quick 源码卫生门禁。

### 使用方式

1. 打开 Demo，显示播放器后点标题行的「Demo 显示」。同一面板也出现在「导出剪辑」和「渲染 Demo」窗口。
2. 方向键可选关闭、其他玩家、全部、自己；强弱钩可选关闭、图标、图标与数字，并单独选择显示范围。
3. 游戏 HUD 与聊天使用各自开关。修改立即作用于回放画面，视频使用相同选择，退出回放后继续使用原来的游戏配置。
4. 选择写入 `qm_demo_show_direction`、`qm_demo_show_strong_weak`、`qm_demo_strong_weak_scope`、`qm_demo_show_hud`、`qm_demo_show_chat`；默认值依次为 1、0、4、0、1。

### 审查与边界

- 审查发现并修复：Demo 的强弱钩数字没有遵循图标范围；时间轴标记绘制相对寻址位置存在半径偏移。
- 后续只读审查：显示选择不覆写正常游戏配置，不修改角色输入、瞄准角、快照或 `.demo` 格式。方向键与强弱钩沿用工作区已有的独立行布局。
- 已先补 `PreviewAndVideoUseTheSameProfile`、`ChangingDemoOptionsDoesNotChangeGameplay` 两项回归测试代码；没有执行红/绿测试，也没有编译或启动客户端。
- 「方向歪掉」的具体复现现象尚未得到补充；本次确认配置与显示布局的隔离，尚未验证武器朝向异常或实机画面。
- 本次扩展通过脚本将版本从 3.5.2 更新到 3.5.3；后续并行任务的版本更新以 `src/game/version.h` 为准。

### 本次扩展验证记录

- `python qmclient_scripts/gate/check_gate.py --mode quick --report-json-path tmp/demo_hud_quick_final.json`：最终复查 10 项通过、1 项失败；失败为工作区格式检查，例如本次未修改的 `src/engine/client/backend/vulkan/backend_vulkan.cpp:8893`。日志：`tmp/demo_hud_quick_final.log`。
- `python qmclient_scripts/fix_style.py -n`：对 `menus_demo.cpp`、`nameplates.cpp`、`hud.cpp`、`menus.h`、`demo_display.h`、Qm 配置头、测试文件和版本头检查通过，日志 `tmp/demo_hud_focused_style.log`。加入 `menus.cpp`、`chat.cpp` 的完整范围检查被并行修改位置 `menus.cpp:1093`、`chat.cpp:1476` 的格式问题阻断，日志 `tmp/demo_hud_style.log`。
- 本次文件 `git diff --check` 通过；原有 UTF-8、无 BOM、LF 换行保持不变。
- `extract_strings.py`：退出码 0。`generate_all.py` 两次直接写文件遇到 Windows `OSError: [Errno 22]`；随后调用同一生成器，在 `tmp/demo_hud_languages/` 生成并原子替换全部 12 个语言文件，退出码 0，日志 `tmp/demo_hud_generate_atomic.log`。
- `validate.py --incremental`：退出码 1。12 个运行时语言文件覆盖检查通过；全仓仍有 11 个范围外缺失译文、7 处旧 TOML 空行问题，以及并行修改造成的提取缓存过期。日志：`tmp/demo_hud_validate_final.log`。
- 新增 8 个 key × 12 种语言逐项核对通过，日志 `tmp/demo_hud_translation_check.log`；`review_duplicate_entries.py --show-groups 0 --show-unused 0` 退出码 0，日志 `tmp/demo_hud_duplicates.log`。

## 本次范围

- 本次已按补丁版本递增，保留并行修改后续的版本更新；当前版本以 `src/game/version.h` 为准。
- 用户确认优先优化现有剪辑工具；本次取消 50 tick 录制改动。
- 联网时客户端 Demo 保存服务器快照。本地服务器可用 `sv_high_bandwidth 1` 提供每秒 50 份快照，现有录制器已经支持；本次不加入插值或预测录制。
- 在现有 `.demo` 裁剪流程上提供明确的起止点按钮、快捷键、百分之一秒时间显示、区间预览，以及更顺畅的导出与片段列表操作。
- 保留现有多段导出、移除聊天、可选视频导出；不改 Demo 文件格式和录制数据。

## 验收行为

1. `I` 设置起点，`O` 设置终点；按钮右键清除对应标记。单个标记使用 Demo 首尾补足区间，两端均未标记时不视为已选区间。
2. `P` 预览当前区间，再按一次停止；播放到终点自动暂停，跳转、逐 tick 移动、修改选区或打开导出时取消预览。
3. 时间显示保留百分之一秒，例如 50 tick/s 下 1 tick 显示为 `0.02` 秒。
4. 导出默认使用带 `_cut` 后缀的新名称；失败只提示一次，用户可修改后重试；成功后清除已导出的选区。
5. 多段列表可滚动查看、单段预览，并删除后面的片段。

## 使用方法

1. 打开 Demo，按 `Esc` 显示播放面板。拖动时间轴定位，需要精调时使用原有的逗号、句号键向前或向后移动一个录制 tick。
2. 到起点按 `I`，到终点按 `O`；也可以点击下方的起点、终点按钮。右键按钮清除对应标记。
3. 按 `P` 或点击“预览片段”，从起点播放并在终点暂停。再次按 `P` 停止预览。
4. 点击“导出”，使用默认的 `原名称_cut` 或自定义名称保存新的 `.demo`。可沿用“移除聊天”和“将片段渲染为视频”选项，后者需要客户端包含视频录制支持。
5. 需要多段时，标记每段后点击 `+` 加入列表。有已添加片段时按列表导出，新选区需先加入列表。导出窗口中可滚动列表、点击播放按钮单独预览，或点击叉号删除某段。

`I/O/P` 与原有逐 tick 快捷键遵循 Demo 播放器的“键盘快捷键”开关；文字按钮始终可用。无有效选区时无法预览，未选区且列表为空时无法导出。

## 验证记录

- 先在 `src/test/qm_new_ui_menu_branch_test.cpp` 补全 5 个行为测试，覆盖选区边界、时间精度和预览停止/取消，再实现。按用户要求未编译、未执行 C++ 或 Rust 测试，因此没有运行时通过结论。
- `python qmclient_scripts/languages_qmclient/extract_strings.py`：通过，提取 5401 个 key，阻断性字符串审计违规为 0。
- `python qmclient_scripts/languages_qmclient/generate_all.py`：通过，生成 12 种语言文件。
- `python qmclient_scripts/languages_qmclient/validate.py`：未通过。其他工作区修改有 11 个 key 缺少各语言维护译文，涉及实时通道及昵称作用域；`translations/i18n/qmclient.toml` 另有 7 处条目间空行缺失。本次 `demo.toml` 新增的 6 个 key 已单独核对 12 种语言、占位符和生成文件内容，均一致。
- `python qmclient_scripts/languages_qmclient/review_duplicate_entries.py --show-groups 0 --show-unused 0`：通过，无重复 key、无空译文。
- `python qmclient_scripts/gate/check_gate.py --mode quick`：10 项通过、1 项失败。失败为本次未修改的 `src/game/client/components/qmclient/local_saves.h` 和 `src/test/qm_chat_interactions_test.cpp` 的格式检查；未扩展修改这些文件。
- `python qmclient_scripts/fix_style.py -n src/game/client/components/menus.cpp src/game/client/components/menus.h src/game/client/components/menus_demo.cpp src/game/client/components/qmclient/demo_cut.h src/test/qm_new_ui_menu_branch_test.cpp src/game/version.h`：通过。
- 本次文件的 `git diff --check`：通过。
- 检查日志：`tmp/demo_cut_extract.log`、`tmp/demo_cut_generate.log`、`tmp/demo_cut_validate.log`、`tmp/demo_cut_duplicates.log`、`tmp/demo_cut_gate_quick.log`、`tmp/demo_cut_style.log`。

## 只读审查

Findings：输入文件名时列表响应键盘、相同起止点意外变成开放区间、预览终点被普通播放的回到开头逻辑覆盖、导出失败重复触发，这些边界已处理；列表修改在结束裁剪区域之后执行，换 Demo 时清理选区和预览状态。

结论：本次补丁未发现尚未处理的阻断问题。界面实际显示、Demo 播放与导出仍需在用户允许编译和运行后验证；全仓门禁与翻译校验的上述失败保持如实记录。
