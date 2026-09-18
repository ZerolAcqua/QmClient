# 好友爱心实体化

## 已确认行为

- 好友界面里的爱心图标由中空线稿改为实体填充心形（U+2665 BLACK HEART SUIT），由默认字体 DejaVu Sans 渲染。
- 本次覆盖的显示位置：
  - 服务器浏览器好友列表的列头爱心（`COL_FRIENDS` 列标签）。
  - 服务器列表每行好友状态图标（`UI_ELEM_FRIEND_ICON`，含好友数量角标）。
  - 工具箱好友页签图标（新 UI 胶囊页签与旧 UI 分段页签两条路径）。
  - 计分板玩家右键菜单里的加好友按钮（未加好友状态）。
- 只改字形与字体预设：好友判定、点击行为、悬停提示、图标颜色（浏览器 0.94/0.4/0.4，计分板 0.95/0.3/0.3）、图标位置与动画都保持原样。
- 图标字体 Phosphor 只有中空心形，没有实心爱心码位，因此爱心改用默认字体的 U+2665；心形以外的邻近图标（过滤器页签清单图标、信息页签图标、星标、锁、钥匙、旗帜等）继续由图标字体绘制。
- 已是好友时，计分板加好友按钮悬停显示的裂心仍是图标字体的中空裂心（U+EBE8）；图标字体没有实心裂心，本次不为其拼凑替代字形，风格差异保留待定。
- 仍使用中空心形的调用点（待后续收口）：名牌好友标记 `src/game/client/components/nameplates.cpp:679`（`CNamePlatePartFriendMark::UpdateText`，当前用 `ICON_FONT` + `FontIcons::FONT_ICON_HEART` 建文本容器）。该文件在本次工作树中由其他任务占用，未改；迁移方式与已改站点一致：建容器前把预设切到 `EFontPreset::DEFAULT_FONT`，字形换成 `QM_FRIEND_HEART_ICON`。
- 旁观列表好友标记 `src/game/client/components/spectator.cpp:801` 已由并发任务改为 `QM_FRIEND_HEART_ICON` + `DEFAULT_FONT`（复用本功能新增的共享头），不计入本次改动。
- 未新增任何界面文案，无需翻译条目。

## 手动验收

1. 服务器浏览器（Internet/LAN）切到好友列，确认列头爱心是实体填充心形，与相邻的收藏星标（线稿）风格区分明显。
2. 列表里至少有一名在线好友时，确认该行好友图标同样是实体爱心；好友数大于 1 时，行内数字角标仍居中显示且不遮挡爱心。
3. 打开工具箱（服务器浏览器右侧工具箱）好友页签：分别在 `qm_new_ui 0` 与 `qm_new_ui 1` 下确认页签爱心为实体填充，且页签选中态、悬停态、滑块动画与另两个页签一致。
4. 进入服务器打开计分板，右键一名非好友玩家：加好友按钮显示实体爱心；点击后按钮变为已加好友的红色状态，再次悬停时显示裂心（仍为中空线稿），移开鼠标后恢复实体爱心。
5. 对照检查未受影响的图标：工具箱过滤器页签、信息页签、服务器列表锁/钥匙/旗帜图标，确认仍是原来的图标字体线稿。
6. 使用 `cl_message_friend` 与 `cl_message_friend_heart_color` 相关的聊天爱心（`♥`）确认行为不变（该处本来就使用默认字体的 U+2665，本次未改）。

上述步骤为待执行的手动验收清单，不代表已通过。按本次约定不进行编译、不运行测试，也未启动客户端。

## 实现记录

### 改动文件与调用点

- 新增 `src/game/client/components/qmclient/friend_heart_icon.h`：共享常量 `QM_FRIEND_HEART_ICON`（U+2665，UTF-8 字节 `E2 99 A5`），注释说明为何不能继续用图标字体码位。
- `src/game/client/components/menus_browser.cpp`：
  - 引入共享头。
  - 好友列列头（`COL_FRIENDS` 分支）：字体预设由 `ICON_FONT` 改为 `DEFAULT_FONT`，字形换成 `QM_FRIEND_HEART_ICON`；渲染标志与旧 UI 复原逻辑保持不变。
  - 行内图标绘制辅助 lambda `RenderBrowserIcons`：新增末尾参数 `EFontPreset FontPreset = EFontPreset::ICON_FONT`，内部改用该参数；好友图标调用点传 `EFontPreset::DEFAULT_FONT` 与实体爱心，其余调用点（锁、钥匙、星标、旗帜）不传参数，行为不变。
  - 工具箱好友页签两处（新 UI 胶囊页签分支与旧 UI 分段页签分支）：在绘制好友页签前临时切到 `DEFAULT_FONT`，字形换成 `QM_FRIEND_HEART_ICON`；页签之后原本就复原为默认字体，未新增额外的复原语句。
- `src/game/client/components/scoreboard.cpp`：引入共享头；右键菜单加好友按钮的图标表达式只替换「未加好友」那一态为 `QM_FRIEND_HEART_ICON`，`FONT_ICON_HEART_CRACK` 分支保持不动。该按钮由 `CUi::DoButton_FontIcon` 绘制，其内部固定使用图标字体预设，U+2665 不在图标字体中，实际由 `ITextRender::GetCharGlyph` 回退到默认字体 DejaVu Sans 渲染（`src/engine/client/text.cpp:450`，依 `m_SelectedFace` → `m_DefaultFace` → 变体字体 → 回退字体列表的顺序逐字符查找）。
- 未由本次改动：`src/game/client/components/spectator.cpp`、`src/game/client/components/nameplates.cpp`（并发任务占用，按要求留给后续统一收口）。交接时 `spectator.cpp:801` 已被并发任务迁到共享常量，`nameplates.cpp:679` 仍是图标字体 U+E2A8 中空心形，属于本次已知未覆盖项。

### 字形证据（为什么换 U+2665）

- 好友爱心原实现：`src/engine/textrender.h:76` 的 `FontIcons::FONT_ICON_HEART = "\xEE\x8A\xA8"`（U+E2A8），调用点成对使用 `SetFontPreset(EFontPreset::ICON_FONT)`。
- 图标字体为 Phosphor v2.1（`data/fonts/index.json` 的 `"icon": "Phosphor"`、`"icon bold": "Phosphor-Bold"`）。
- 直接解析两个 TTF 的 `glyf` 表：U+E2A8 在 `data/fonts/Phosphor-Regular.ttf` 与 `data/fonts/Phosphor-Bold.ttf` 中都是 2 个轮廓（中空）；相邻心形码位 U+E2AA、U+E2AC、U+EBE8 同样中空。
- 逐字形栅格化后按「外轮廓剪影与 U+E2A8 剪影的 IoU > 0.85」筛出所有心形候选，再算填充率（墨迹像素/剪影面积）：Regular 字体候选填充率 0.30~0.60、Bold 字体 0.42~0.72，**没有任何字形填充率 > 0.85**，即图标字体里不存在实心爱心（U+E2A8 自身填充率 0.30/0.42，即线稿）。
- U+2665 在 `data/fonts/DejaVuSans.ttf`（glyph id 3901）与 `data/fonts/NotoEmoji-Regular.ttf`（glyph id 99）中均为 1 轮廓实心；U+2661 是 2 轮廓中空，未采用。
- 仓库既有先例：`src/game/client/components/pie_menu.cpp:561` 与 `src/game/client/components/chat.cpp:2394` 的好友爱心就是用默认字体渲染的 `♥`，说明该码位在客户端能正常显示。

### 测试

- 在 `src/test/qm_new_ui_menu_branch_test.cpp` 末尾新增 `TEST(QmNewUiMenuBranches, FriendHeartsUseSolidHeartGlyph)`：断言共享常量等于 U+2665 的 UTF-8 字节、头文件不再引用 U+E2A8；断言 `menus_browser.cpp` 的列头与行内图标改用实体爱心且行内图标显式传 `EFontPreset::DEFAULT_FONT`（同时确认星标仍走图标字体）；用 `FunctionBody` 提取 `RenderServerbrowserTabBar`，遍历两处好友页签绘制，断言每处之前都有切到 `DEFAULT_FONT` 且切换后未再切回图标字体；断言 `scoreboard.cpp` 只在「未加好友」态使用实体爱心，裂心分支保留。
- 按当次约定未运行 `testrunner` / `run_cxx_tests`，测试代码未执行；为降低不能运行带来的风险，用 Python 复刻了同一组 `find`/位置断言并在打补丁后的源码上全部通过（脚本 `tmp/b1_verify_heart_assertions.py`，仅本地临时文件）。

### 验证与门禁

- `python qmclient_scripts/gate/check_gate.py --mode quick`（最终一次，改动与文档都已落盘）：通过 11、警告 0、失败 0，有效结果 PASS，退出码 0；日志 `tmp/b1_heart_gate.log`。中间一次运行曾出现失败 1 项（`src/game/client/components/menus_settings.cpp:3639` clang-format 违规），是工作树里其他任务的在改文件，本次改动文件不涉及；该违规随后由对方修复，最终运行已全通过。
- 本次改动文件单独核对：`clang-format --dry-run --Werror` 对 `friend_heart_icon.h`、`menus_browser.cpp`、`scoreboard.cpp`、`qm_new_ui_menu_branch_test.cpp` 退出码 0；`git diff --check` 退出码 0（无空白错误）；换行风格保持（`menus_browser.cpp` 与新增头文件 LF，`scoreboard.cpp` 与测试文件 CRLF）。

### 未执行项与待决策

- 未编译（未跑 `game-client`）、未运行 C++/Rust 测试、未跑 i18n 生成脚本、未改 `src/game/version.h`、未做游戏内验收。
- 风格不一致待决策：加好友按钮的「已加好友」态裂心仍是中空线稿，与实体爱心并排时粗细/风格不同；图标字体没有实心裂心，未擅自替换。
- 剩余调用点待收口：`src/game/client/components/nameplates.cpp:679`（`CNamePlatePartFriendMark::UpdateText` 的名牌好友标记）依旧使用 `FontIcons::FONT_ICON_HEART`，需要按同样的方式切到 `QM_FRIEND_HEART_ICON` + `DEFAULT_FONT`（该文件在本次工作树中由其他任务占用，故未改）。`spectator.cpp` 的同名站点已由并发任务用本功能的共享常量完成迁移。
