# 禅模式移除

## 确认范围

2026-09-17 确认完整删除禅模式，并自动清理旧配置与绑定。设置卡片、搜索入口、快捷键编辑、`qm_focus_mode` 配置族、专属显示/消息过滤/静音逻辑及翻译全部移除。

HUD、名字板、聊天、特效、音效、地图进度及 Gores 的独立配置继续生效。Gores 共用的配置覆盖恢复逻辑保留为 `SQmConfigOverrideState` / `ApplyQmConfigOverride`。

## 旧配置处理

- 加载配置时消费已移除的 `qm_focus_mode` 配置族，不再作为未知配置保存；分号后其他命令仍由控制台执行。
- 加载键位绑定时移除直接设置、`toggle`、`+toggle`、`+toggle_restore`、`reset` 中的旧配置操作。组合绑定保留其他命令、引号和注释语义；纯旧绑定自动解绑，兼容 `mc;` 前缀。
- 首次正常加载设置布局时清理全局卡片顺序、旧侧栏顺序和折叠配置中的旧卡片条目。其他条目保持原样。
- 清理结果随客户端正常保存配置落盘，不重置用户独立设置，不推测旧版本覆盖前的值。

## 回归覆盖

先更新测试代码，再删除实现。覆盖旧配置识别、组合绑定清理、保留引号内文本、多命令前缀、卡片与搜索入口消失、旧布局恢复，以及消息分类和 Gores 共用配置恢复。删除只验证已移除功能的测试。

按本次约定不编译、不执行测试。

## 验证记录

- `python qmclient_scripts/gate/check_gate.py --mode quick --report-json-path tmp/zen-mode-removal-gate.json`：PASS，11 项通过，0 警告、0 失败。日志为 `tmp/zen-mode-removal-gate.log`。
- 本次代码范围的 `git diff --check` 通过；全工作树另有既有 `docs/qmclient/blank_skin_rendering.md` 末尾空行，不在本次修改范围。
- 只读审查曾发现旧配置分号后命令重复保存、纯 `mc;` 绑定未清空两项问题，已修复并补充测试输入。聊天、通知和独立模式行为审查未发现新增问题。
- 翻译流程已完成 `extract_strings`、`generate_all`、`validate`、`review_duplicate_entries`。删除 46 条专属翻译和 22 条配置帮助迁移记录，更新 12 种语言；重复 key、空译文和 unused 均为 0。`validate` 返回 1，原因是既有表情功能缺译及 3 个中文源 key 违规；覆盖、新鲜度、TOML 格式和完整性检查通过。结果摘要为 `tmp/remove-focus-i18n-validation.txt`。
- 版本按当时工作树最新值从 `3.8.0` 更新为 `3.8.1`。
