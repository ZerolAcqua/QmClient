# 方向键与强弱钩显示解耦

用户确认的行为：两项各自开关、调整大小和拖动；操作其中一项不改变另一项的位置或显隐，游戏内与设置预览一致。

## 实现边界

- 继续使用各自已有的开关、显示范围、大小和拖动偏移配置。
- 方向键基线不再累计强弱钩的实际行高，而是使用默认大小对应的固定 29 单位间距。强弱钩关闭后仍保留这段间距，放大强弱钩也不推开方向键；需要更多间隔时分别拖动。
- 渲染、拖动命中框与基准矩形共用位置计算。昵称、战队、坐标及强弱钩判定语义保持原有行为。
- 预览测量为两项保留允许的最大尺寸空间，避免开关或缩放其中一项时改变预览框高度和另一项的屏幕位置。
- 不新增配置或翻译，不改协议、物理、预测及构建文件。

## 回归与验证

已先补充测试代码，覆盖强弱钩开关及大小变化、方向键开关及大小变化，以及预览空间稳定性和尺寸上限。随后实现共享布局计算；本次修复将版本从 3.5.0 更新为 3.5.1。

依用户要求，不编译或执行测试；没有红/绿运行证据。

只读审查未发现本次改动的遗留问题：

- 普通显示、自由移动、拖动命中框和拖动边界共用同一组基线。
- 强弱钩与方向键的显示条件、范围及各自拖动偏移保持独立。
- 预览空间覆盖两项允许的尺寸上限，开关和缩放不改变预览框高度。

验证结果：

- `python qmclient_scripts/gate/check_gate.py --mode quick --report-json-path tmp/nameplate_indicators/quick_gate.json`：10 项通过、1 项失败，失败项为代码格式。报错来自本次范围外的工作区改动，例如 `players.cpp`、`local_saves.h`、`skins.cpp`、`skins7.cpp`、`background_particles.h`、`moving_tiles.cpp`、`outlines.cpp`、`statusbar.cpp`、`tclient.cpp` 等；本次未修改这些文件。
- `python qmclient_scripts/fix_style.py -n src/game/client/components/nameplates.cpp src/game/client/components/qmclient/nameplate_layout.h src/test/qm_new_ui_menu_branch_test.cpp src/game/version.h`：通过，退出码 0。
- `git diff --check -- src/game/client/components/nameplates.cpp src/test/qm_new_ui_menu_branch_test.cpp src/game/version.h`：通过。
- 原有文件的 UTF-8、无 BOM 和 LF 换行保持不变。
- 本次未编译、执行测试或启动客户端；实机画面仍未验证。

门禁日志：`tmp/nameplate_indicators/quick_gate.log`；本次文件格式检查日志：`tmp/nameplate_indicators/style_check.log`。
