# 「梦的小功能」卡片底部空白行修复（2026-09-18）

## 问题确认

用户反馈「梦的小功能」卡片末尾多出两行空白。核对后确认不是多了两个开关，而是卡片高度比实际渲染行数多算了两行：

- 渲染：`RenderQmFunctionMiniFeaturesContent` 实际输出 18 行（含「新版 IME」「赞助提醒」两个单独渲染行）。
- 测量：`MeasureContentHeight` 仍是 `Rows(20.0f)`，比真实行数多 2 行，多出的高度显示为卡片底部空白。

历史原因：`Rows(20.0f)` 对应「实时通道」开关删除时的行数，也顺应了当时卡片开头的两个粒子效果开关；本次之前粒子开关与配置已在工作区移除，行数降到 18，但测量数字没有一起更新——仅删 UI 行时不会报错，只会留下空白。

## 改动

- `src/game/client/components/qmclient/menus_qmclient.cpp`
  - 新增 `SQmMiniFeatureRow` 与 `s_aQmMiniFeatureRows`（16 条普通开关：绑定、文案 key、配置指针），渲染改为遍历该表，仍是 `RenderQmFunctionCheckbox(..., Localize(key), ..., PrewarmOnly)`，按钮 id 与文案与改前逐行一致。
  - 「新版 IME」（独立文案）与「赞助提醒」（关闭时走灵动岛问话）保留单独渲染，新增 `constexpr size_t QmMiniFeatureSpecialRowCount = 2` 计入行数。
  - `case EQmModuleId::MiniFeatures` 改为 `Rows(s_aQmMiniFeatureRows.size() + QmMiniFeatureSpecialRowCount)`，即 18 行；渲染与高度从此共用同一份行定义，增删开关不会再让两者错位。
  - 行为不变：所有 18 个开关的配置项、顺序、文案 key、勾选写入与预热（PrewarmOnly / RenderOnly 回滚）逻辑均与改前一致。

- 测试
  - `src/test/qm_new_ui_menu_branch_test.cpp`：新增 `CountOccurrences` 辅助；`GaussianBlurSettingReplacesBetterScoreboardAndIsVersioned` 改为断言行表 16 条、渲染行 18 行，并断言高度表达式取自行表；`ProcessPriorityAndImeHaveVisibleSettings` 改为在行表中核对「高进程优先级」「自动管理 IME」，函数体内核对「新版 IME」；两处旧 `RenderCheckbox(...)` 断言改为行表条目断言。
  - `src/test/qm_chat_interactions_test.cpp`：「消息合并」断言改为行表条目（仍从 `RenderQmFunctionMiniFeaturesContent` 之后查找，覆盖顺序为「卡片内出现」）。

## 验证记录

- `clang-format --dry-run --Werror`：上述三个文件通过。
- MSVC 语法检查（`cl /Zs`，取自 `cmake-build-release/compile_commands.json`，经 `vcvars64.bat` 环境）：三个文件 exit=0。
- `py -3 qmclient_scripts/gate/check_gate.py --mode quick`：PASS，11 通过、0 警告、0 失败。
- 未执行：未编译游戏客户端、未运行 `testrunner`、未在界面里目视确认空白消失。按开发期工作流本次只跑 quick 源码卫生门禁；界面效果需重新构建后确认。工作区当前 `cmake-build-release/DDNet.exe` 构建于 2026-09-18 22:16，早于本次改动且仍含已删除的「实时通道」行，不能代表改后状态。

## 证据

- 行数核对：`s_aQmMiniFeatureRows` 16 条 + 特殊行 2 条 = 渲染 18 行 = `Rows(18)`。
