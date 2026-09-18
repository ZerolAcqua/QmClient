# 三跳 HUD 中文默认文案

## 行为约定

- 默认文案固定为中文，数值保持不变：

  ```text
  三格边缘跳:
  左起跳: .34|.31|.16
  左二段跳: .41|.28|.25|.13
  右起跳: .63|.66|.81
  右二段跳: .56|.69|.72|.84
  ```

- `qm_jump_hint` 的默认值继续为 `0`。
- 经用户确认，首次应用本次迁移时，将已有开启状态关闭一次；此后用户可重新开启。
- 先完成旧 `tc_jump_hint*` 配置迁移，再执行中文默认文案迁移。
- 仅将与旧英文默认文案完全一致的配置替换为中文；自定义文案保持不变。空文案继续由 HUD 和编辑器回退到中文默认文案。
- 使用保存到配置的 `qm_jump_hint_defaults_migrated` 标记避免重复迁移。位置、颜色、字号沿用现有设置。
- 版本从工作区原有的 `3.6.3` 更新为 `3.6.4`。

## 验证记录

- `src/test/qm_modes_test.cpp` 补充默认文案、默认关闭、升级关闭一次、自定义文案保留及迁移后用户选择保留的测试代码。
- 按本次要求，未编译、未运行测试。
- 只读审查未发现需要修复的问题；已核对启动迁移顺序、配置保存规则及 HUD／编辑器调用点。
- `python qmclient_scripts/gate/check_gate.py --mode quick`：10 项通过、1 项失败；失败为本次未修改的 `src/test/qm_realtime_test.cpp:188` 格式问题。三跳相关文件未报告格式错误。
- 国际化链已执行 `extract_strings.py`、`generate_all.py`、`validate.py --incremental`、`review_duplicate_entries.py --show-groups 0 --show-unused 0`，最终均退出 `0`。新增迁移标记帮助文本在 12 种生成语言中各有一条匹配译文；保留原有 `Use style colors` 译文。
