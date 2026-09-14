# 昵称页范围选择按钮样式

日期：2026-09-14。版本：3.4.6。

## 范围

按用户提供的截图，将“钩索强度范围”和“显示按键”两组按钮统一为“显示昵称”的样式，仅调整外观。

- 新 UI 宽度足够时，复用相同的圆角底条、选中滑块、字体、悬停反馈与切换动画；旧 UI 和窄窗口沿用原分段按钮。
- 钩索范围保持自身、他人、强钩、弱钩、全体五项；按键范围保持禁用、自身、他人、全体四项及原配置值映射 `0, 3, 1, 2`。
- 行高测量、绘制和预布局共用布局解析；按键预布局的点击区域同步使用绘制时的分段槽位。

实现位置：`src/game/client/components/menus_settings.cpp`。版本定义：`src/game/version.h`。

## 审查与验证

只读审查未发现本次改动引入的配置映射、行高或点击区域问题。

- `python qmclient_scripts/fix_style.py -n src/game/client/components/menus_settings.cpp src/game/version.h`：退出码 0。
- `git diff --check -- src/game/client/components/menus_settings.cpp src/game/version.h`：退出码 0。
- `python qmclient_scripts/gate/check_gate.py --mode quick --report-json-path tmp/nameplate-scope-style/gate-report.json`：退出码 1，10 项通过、1 项失败；设置页 UI 检查通过，失败项为其他文件的代码格式检查，包括 `players.cpp`、`local_saves.h`、`skins.cpp`、`skins7.cpp`、`tclient/background_particles.h`、`tclient/moving_tiles.cpp`、`tclient/outlines.cpp`、`tclient/tclient.cpp`、`render.cpp`、`skin.cpp`、`skin.h`、`src/test/qm_chat_interactions_test.cpp`，均不在本次修改范围。
- 日志：`tmp/nameplate-scope-style/gate.log`；修改前文件副本保存在同一临时目录。

未执行编译、C++/Rust 测试及客户端画面验收。
