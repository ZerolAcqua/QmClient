# 赞助头衔卡片布局修复

日期：2026-09-13。版本：3.3.2。

## 范围与确认

修复赞助头衔卡片中保存/刷新越界、展开风格列表重叠和预览不可见的问题。
用户确认保留只显示“[赞助者]”动态效果的列表内容；保留卡片内展开、最多六项可见、内部滚动和选择后收起。

## 原因与修复

- 卡片测量按 10 行，实际有 11 行，漏算了“自定义头衔”标签。测量补齐后，底部按钮完整计入内容高度。
- 展开面板只消费一行高度，再单独改背景高度，后续控件仍在面板内部。改为一次消费完整面板高度，测量只增加面板高度与行距。
- 预览容器在屏幕横坐标创建，生成文本后光标又移到行末，旧代码把行末位置作为渲染偏移再次叠加。改为局部原点生成容器，独立计算居中屏幕位置，渲染时只叠加一次，并恢复调用前文字颜色。
- 测量和渲染捕获同一展开状态，点击变化从下一帧统一生效，避免展开当帧的新内容使用旧卡片尺寸。
- 滚动中可能同时露出七个条目，原先按六个槽位取模的点击 ID 会重复。改为按风格序号固定 ID；“跟随服务器”的选中标记仅依据是否启用本地选择。

实现：`src/game/client/components/qmclient/menus_qmclient.cpp`。
回归代码：`src/test/qm_title_style_test.cpp`，覆盖测量行数与真实绘制行数一致、完整面板占位、展开状态一致和预览坐标只定位一次。

## 审查与验证

上轮只读审查未识别出颜色读取接口误用；随后用户编译暴露了该问题，见下方跟进。
结论：已完成源码修复；按用户要求未编译、未运行测试，尚未进行客户端画面验收。

- `python qmclient_scripts/gate/check_gate.py --mode quick --report-json-path tmp/qm-title-layout-fix/gate-report.json`：退出码 1，10 项通过、1 项失败。失败项为代码格式干跑检查，位于本次未修改的 `src/engine/client/backend/vulkan/backend_vulkan.cpp:8906:125`。
- `python qmclient_scripts/fix_style.py -n src/game/client/components/qmclient/menus_qmclient.cpp src/test/qm_title_style_test.cpp src/game/version.h`：退出码 0，本次涉及的代码文件格式检查通过。
- quick 日志：`tmp/qm-title-layout-fix/gate.log`。修复前局部文件副本保存在同一临时目录，供区分已有未提交改动。

## 编译错误跟进

用户编译报告 `menus_qmclient.cpp:505` 出现 C2661/C2440。`ITextRender::TextColor` 只有设置颜色的重载，读取当前颜色应使用 `GetTextColor() const`。
已将预览的颜色保存调用改为 `pTextRender->GetTextColor()`，恢复颜色仍使用 `TextColor(PreviousTextColor)`，与 `SettingsCard.cpp` 的现有模式一致。
只读核对确认接口签名匹配；保留原定布局和预览行为，未编译、未运行测试。

本轮 `python qmclient_scripts/fix_style.py -n src/game/client/components/qmclient/menus_qmclient.cpp` 与 `git diff --check -- src/game/client/components/qmclient/menus_qmclient.cpp` 均退出 0。
重新执行 quick：10 项通过、1 项失败，仍为未修改的 Vulkan 文件第 8906 行格式问题；日志为 `tmp/qm-title-layout-fix/get-text-color-gate.log`，JSON 报告为 `tmp/qm-title-layout-fix/get-text-color-gate-report.json`。
