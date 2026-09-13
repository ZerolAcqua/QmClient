# 灵动岛位置标题精简

日期：2026-09-13。版本：3.3.3。

用户明确将需求收窄为删除“显示位置”标题。保留“跟随 Tee”“显示在灵动岛内”两个开关及原有顺序和行为。

实现移除标题绘制及预布局中的对应占位；卡片高度按四行常驻开关计算，启用开关倒计时后增加两个位置开关，非原始样式另计背景颜色行。此前关闭开关倒计时时漏算总开关的高度也已补齐。
已更新现有卡片高度测试和位置选择 UI 合同测试代码，未执行测试。

只读审查 findings：本次补丁未发现新的阻断问题。
结论：源码修改完成，未编译、未运行测试、未做客户端画面验收。

验证：

- `python qmclient_scripts/gate/check_gate.py --mode quick --report-json-path tmp/qm-island-location-label/gate-report.json`：退出 1，10 项通过、1 项失败；失败来自本次未修改的 `src/engine/client/backend/vulkan/backend_vulkan.cpp:8906:125` 格式问题。日志：`tmp/qm-island-location-label/gate.log`。
- `python qmclient_scripts/fix_style.py -n src/game/client/components/qmclient/menus_qmclient.cpp src/game/client/QmUi/SettingsPageLayout.h src/test/settings_card_deck_logic_test.cpp src/test/qm_new_ui_menu_branch_test.cpp src/game/version.h`：退出 0。
- `git diff --check` 限定上述五个修改文件：退出 0。
