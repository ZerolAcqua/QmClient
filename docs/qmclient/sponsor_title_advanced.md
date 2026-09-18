# 赞助头衔高级模式与成品预览

## 2026-09-17 颜色与风格选择修复（3.6.4）

本轮确认：风格列表始终展示各风格原色；选择风格时自动切换为「使用风格颜色」，成品预览立即展示当前草稿。用户之后仍可手动选择单色或彩虹。保留现有布局和仅展示「[赞助者]」的列表，配色入口仍在高级模式中。

- 原因：列表调用通用风格解析后仍继承全局单色/彩虹覆盖标志，且没有传入替代色，最终取默认白色。列表现在显式使用风格自身颜色。
- 名牌与成品预览原先只要存在风格就使用白色渲染基色，导致单色未参与。现在仅在实际使用风格颜色或彩虹色段时采用顶点配色。
- 成品预览补回覆盖模式的彩虹色段，按 UTF-8 字节偏移跳过左括号；风格解析优先使用当前选择的草稿，服务器旧风格不再覆盖草稿预览。
- 区分「服务器分配风格」与「使用风格颜色」两项，补齐十二种语言。头衔资料和风格仍通过原有保存按钮提交；实际玩家风格仍保留服务端优先的规则。
- 先补充 `src/test/qm_title_style_test.cpp` 的源码接线回归用例，再修改实现，覆盖列表原色、选择时恢复配色、草稿风格优先和名牌/预览的自定义颜色分支。按要求不编译、不运行测试。

本轮只读审查：未发现阻断问题。已核对风格原色绕过覆盖、草稿与已保存风格的优先级、单色渲染基色、彩虹字节偏移和保存路径。

验证记录：

- `python qmclient_scripts/fix_style.py -n src/game/client/components/qmclient/menus_qmclient.cpp src/game/client/components/nameplates.cpp src/test/qm_title_style_test.cpp src/game/version.h`：退出码 0。
- 本轮涉及文件的 `git diff --check`：退出码 0。
- `python qmclient_scripts/gate/check_gate.py --mode quick --report-json-path tmp/title-color-fix-gate.json`：10 项通过、1 项失败；唯一失败为本轮未修改的 `src/test/qm_realtime_test.cpp:188` 格式问题。设置页合同通过。
- i18n 提取、十二种语言生成与 `review_duplicate_entries.py --show-groups 0 --show-unused 0`：退出码 0；无重复 key、空译文或未使用条目候选。生成器另提示既有的 `DDRace HUD Pro` 简中占位文案未翻译。
- `python qmclient_scripts/languages_qmclient/validate.py --incremental`：退出码 0，十二种语言覆盖及 TOML 完整性通过，保留三条其他菜单文案长度告警。新文案使用 `Title color mode` 上下文，避免与其他模块的同名 key 产生译文冲突。日志：`tmp/title-color-validate.log`。
- 未编译、未执行 C++/Rust 测试、未启动客户端，实际画面尚未验收。

日期：2026-09-13

## 已确认行为

- 补回现有十项外观配置：配色模式、单色颜色、透明度、色带相位、空间效果、经典光晕、掠光速度、波浪波长、波浪速度、整数像素对齐。
- 新增 `qm_title_advanced`，默认关闭并保存开关状态。关闭只收起十项控件，保留已设置效果；风格选择与波浪幅度保持直达。
- 赞助头衔卡片内始终显示成品预览框，实时使用当前填写的头衔；空文本使用示例。预览使用当前选择的草稿风格，跟随服务器时使用已有服务端风格。
- 预览不提交资料。头衔文字、昵称绑定和风格仍由原有保存按钮提交。

## 实现

- `src/game/client/components/qmclient/menus_qmclient.cpp`：十项控件归入高级模式，配置变化刷新聊天缓存。颜色弹窗在绘制控件之后写回，因此跨帧检测颜色变化。
- 测量和绘制捕获同一帧展开状态：基础十二行、高级模式二十二行，另计独立预览框与展开的风格列表高度。
- 成品预览复用头衔逐字渲染和现有空间效果绘制入口，按名牌规则施加配色、透明度、掠光、波浪与经典光晕；裁剪限定在预览框内。
- 2026-09-13 修复「选单色没用，起效的还是预设渐变颜色」：
  - `qm_title_render.cpp/h`：`SQmTitleRenderStyle` 新增 `m_ColorOverride`，单色/彩虹档下不再采样风格颜色；新增 `QmTitleRenderFillMotionOffsets` 供覆盖档单独取浮动偏移；`QmTitleResolveRenderStyle` 增加显式本地兜底的重载，预览与名牌走同一条解析路径。
  - `chat.cpp`、`nameplates.cpp`、`menus_qmclient.cpp`：三条绘制路径都按 `m_ColorOverride` 分支，颜色交给本地配色档，浮动与掠光保留；预览的彩虹分段只跨头衔本体，与游戏内 `[]` 单独绘制的行为一致。
  - `qmclient.cpp`：`title/list` 的清空旧 presence 移到「拿到本服务器有效名单」之后，请求失败或服务器地址变化不再让头衔与风格瞬间消失（此前表现为颜色在两种样式之间来回闪）。
  - 设置项文案改为「头衔颜色」，提示改为「单色与彩虹覆盖动态风格的颜色，风格保留动态效果」。
- `src/test/qm_title_style_test.cpp`：先更新回归代码，再实现；覆盖高级区配置入口、保存状态、卡片占位、草稿预览与效果接线。按用户要求未编译、未运行测试。
- 新增及补齐十九条相关文案的全部十二种语言翻译，并重新生成运行时语言文件。
- 版本由 `3.3.5` 更新到 `3.3.6`。

## 保留的生效边界

- 配色优先级：本地配色档（单色/彩虹）压过动态风格自带的渐变颜色；「跟随服务器」档保持既有表现。风格被覆盖时仍保留逐字浮动与掠光。
- 跟随服务器配色时沿用原有透明度规则；经典光晕只对经典档动态风格生效，掠光只在抛光档生效。
- 预览展示名牌头衔外观。聊天保留当前渲染规则，包括动态头衔透明度与名牌不同的既有行为。
- 本次不修改认证、服务端、协议、实际名牌或聊天的渲染规则。

## 配色优先级修复的验证（2026-09-13）

- `python qmclient_scripts/fix_style.py -n <本次改动的 8 个文件>`：退出码 0。
- `python qmclient_scripts/gate/check_gate.py --mode quick`：通过 11、警告 0、失败 0，`PASS`。
- i18n 主链：`extract_strings.py` → `generate_all.py` → `validate.py` → `review_duplicate_entries.py`。
  - `validate.py` 退出码 1，唯一失败项为**既有**（HEAD 同样存在）的 `translations/i18n/qmclient.toml` 七处
    `missing blank line before [[message]]`，与本次改动无关；另有 3 条既有长度告警。
  - `review_duplicate_entries.py` 退出码 0，无候选未使用条目。
- 新增回归用例：`LocalColorModeOverridesStyleColors`（三条绘制路径的覆盖分支与优先级）与
  `TitlePresenceSurvivesFailedRefresh`（清空旧 presence 必须落在成功分支内）。
- 未编译游戏、未编译或执行 C++/Rust 测试，未启动客户端；仅用录屏逐帧核对了修复前的现象。

## 只读审查与验证（3.3.6 当时）

- 审查发现并修正颜色弹窗跨帧写回的刷新遗漏；本次代码未发现剩余阻断项。核对了卡片行数与预览占位、字体边界、预览坐标、渲染状态恢复和离线玩家编号边界。
- `python qmclient_scripts/fix_style.py -n src/game/client/components/qmclient/menus_qmclient.cpp src/test/qm_title_style_test.cpp src/engine/shared/config_variables_qmclient.h src/game/version.h`：退出码 0。
- `python qmclient_scripts/gate/check_gate.py --mode quick`：退出码 1，十项通过、一项失败。唯一失败为本次未修改的 `src/engine/client/backend/vulkan/backend_vulkan.cpp:8906` 的格式；设置 UI 合同通过。日志：`tmp/sponsor_title_advanced_gate.log`。
- 未编译游戏、未编译或执行 C++/Rust 测试，未启动客户端；实际窗口交互和像素效果尚未实测。
