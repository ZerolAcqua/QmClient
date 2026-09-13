# 聊天头衔动态效果

日期：2026-09-13。版本：3.3.4。

## 已确认行为

- 聊天消息中的头衔沿用名牌的动态配色、掠光配置和逐字上下浮动。
- 服务端分配的风格优先，本地配置继续控制浮动与掠光；普通消息与合并消息中的各位作者都适用。
- 玩家名、消息正文和图片表情保持静止，继续使用聊天自身的绘制效果。

## 实现与边界

- 直接向聊天头衔传入完整风格及 `QmTitleShimmerFromConfig()`，绘制头衔后清除字符偏移与颜色分段。
- 按实际作者的最大浮动范围为每个文本行预留上下空间，测量与绘制共用行距，换行和图片表情计入同一高度预算。
- 浮动空间向上对齐屏幕像素，行高不随动画相位变化；幅度或可见头衔变化时清除两种聊天宽度的高度缓存。
- 无动态风格或浮动幅度为零时不增加浮动留白。开启浮动后，同一聊天区域可容纳的消息会相应减少。

## 验证

- 先补充 `src/test/qm_title_style_test.cpp` 的回归用例，再实现：覆盖浮动范围、上下与换行边界、共享掠光、合并作者以及测量/绘制接线。
- 按用户要求不编译、不运行测试；测试代码尚未执行。
- 只读审查 findings：未发现本次改动的阻断问题；核对了高度缓存、字符偏移清理、背景锚点和头像对齐。
- `python qmclient_scripts/gate/check_gate.py --mode quick --report-json-path tmp/chat_title_animation_gate.json --scope-report-path tmp/chat_title_animation_scope.json`：10 项通过、1 项失败；失败来自本次未修改的 `src/engine/client/backend/vulkan/backend_vulkan.cpp:8906` 格式问题。完整报告在 `tmp/chat_title_animation_gate.json`。
- `python qmclient_scripts/fix_style.py -n src/game/client/components/chat.cpp src/game/client/components/chat.h src/game/client/components/qmclient/qm_title_style.cpp src/game/client/components/qmclient/qm_title_style.h src/test/qm_title_style_test.cpp src/game/version.h`：退出码 0，本次涉及的代码文件格式检查通过。
