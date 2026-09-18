# 设置页下拉弹层对比度

2026-09-17 确认范围：所有设置页的下拉列表弹层。未展开按钮保留当前主题表面，设置页之外的下拉框保持原有样式和动画。

## 原因与修复

设置页统一入口 `CMenus::DoSettingsDropDown` 使用 `QmSettingsDropdownVisualStyle`。旧样式直接复用普通卡片的 `Theme.m_Surface` 和 `Theme.m_Border`，边框只有 `0.10 × 设置透明度`，弹层背景也跟随设置透明度降低。

- 背景使用现有深色表面 `SURFACE_ELEVATED` 的 RGB（0.10、0.11、0.14），透明度固定为 1。
- 边框使用当前主题强调色，透明度固定为 1。
- 活动条目使用主题选中色的轻量覆盖，避免亮色主题的卡片背景覆盖深色底面。
- 设置页下拉弹层关闭表面透明度淡入，首次打开和打开期间刷新都传递这一策略，确保首帧即遮住底层内容。
- 保留选择、滚动、键盘、定位和关闭行为。

历史提交 `b7fef1849` 将设置下拉框边框改为普通主题边框，`472dd7a51` 将所有设置页下拉框统一到该样式。已确认配色策略的统一，未确认删除了另一套下拉组件。

## 测试与验证边界

先更新 `src/test/QmAnimTest.cpp` 的回归测试，再修改实现。覆盖明亮主题及 0%、5%、75%、100% 设置透明度下的深色不透明背景、未展开按钮样式保留、关闭弹层透明度淡入，以及自定义强调色和活动条目在零设置透明度下仍可见。

按工作约定只补充测试代码，不编译、不运行 C++ / Rust 测试，也未启动客户端进行视觉验收。版本按修复递增补丁号。

本次版本从工作区已有的 `3.6.6` 递增至 `3.6.7`。只读代码审查未发现需修复的问题。

| 检查 | 结果 |
| --- | --- |
| `python qmclient_scripts/gate/check_gate.py --mode quick --report-json-path tmp/settings_dropdown_gate.json --scope-report-path tmp/settings_dropdown_scope.json` | 10 项通过、1 项失败。失败是本次未修改的 `src/game/client/components/chat.cpp:1512` 与 `src/test/qm_realtime_test.cpp:188` 的 clang-format 格式问题，整体未通过。 |
| `python qmclient_scripts/fix_style.py -n src/game/client/QmUi/QmDropdown.h src/game/client/ui.cpp src/game/client/ui.h src/test/QmAnimTest.cpp src/game/version.h` | 通过。 |
| 本次修改范围的 `git diff --check` | 通过。 |

完整门禁日志：`tmp/settings_dropdown_gate.log`。
