# 主动断开绑定入口

## 已确认行为

- `qm_timeout_disconnect` 的按键绑定入口移到「设置 → 控制 → 杂项」，显示为「主动断开」。
- 删除「Qm 功能 → 按键绑定」中的原入口，不指定默认快捷键；已有绑定由控制页按相同命令读取。
- 沿用现有命令行为与 `Active disconnect` 翻译。
- 搜索命令名、「主动断开」及原有「异常断开 / timeout disconnect」关键词时，定位到控制页的杂项卡片。
- Qm 按键绑定卡片由 8 行缩为 7 行。

## 实现与验证记录

- 先调整入口归属断言并补充搜索定位测试代码，再移动入口和搜索关键词。
- 测试代码位于 `src/test/qmclient_monitoring_test.cpp` 与 `src/test/qm_card_registry_test.cpp`；按本次要求未编译或运行测试。
- 只读审查发现共享工作区版本已更新到 `3.5.0`，本次在此基础上递增到 `3.5.1`。
- 本次入口迁移的只读审查无遗留发现，相关文件的 `git diff --check` 通过。
- 执行 `python qmclient_scripts/gate/check_gate.py --mode quick --report-json-path tmp/timeout_disconnect_controls/gate.json --scope-report-path tmp/timeout_disconnect_controls/gate-scope.json`：10 项通过、1 项失败。失败项为本次未修改文件中的格式问题；设置页 UI 检查通过。完整日志位于 `tmp/timeout_disconnect_controls/gate.log`。
