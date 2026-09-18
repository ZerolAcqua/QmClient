# Tee 皮肤设置页双栏布局重构

## 2026-09-19：按用户确认撤回布局重排

以下原重构记录作为历史保留，当前行为以本节为准。

- 恢复左侧玩家预览、右侧皮肤选项、下方整宽皮肤列表；队列与预设回到列表卡内的右侧面板，预设恢复原单列列表。
- 身体和脚的颜色控件回到皮肤选项卡，使用渲染时的实际内容矩形计算位置；预览卡恢复原高度。
- 双击左键/右键应用皮肤的独立代码、皮肤队列及预设的保存/应用/重命名/删除功能保留。
- 布局版本推进至 9，替换原 v8 重排迁移，一次性将这三张 Tee 卡恢复到原列位，覆盖已保存的错误顺序；其余卡片不重置。保存成功才推进版本。
- 客户端补丁版本更新至 3.9.3。
- 先恢复布局、队列几何与搜索目标的测试预期，补充颜色控件坐标范围及 v8 配置恢复到原布局的回归断言，再恢复实现。按要求不编译、不运行测试；quick 源码门禁结果见本节后续记录。

本次改动同时覆盖三条用户需求，交付为一次改动：

1. 开启「自定义颜色」后卡片过长，导致无法实时切换皮肤。
2. 已保存的预设是窄卡片，「窄窄的，多了要往下拖了」。
3. 双击左键改本体、双击右键改分身。

## 已确认行为

- Tee 页（Player / Dummy 子标签，`SETTINGS_TEE`）由原来的三张卡改为双栏：
  - 左列 `deck:tee-skin-list`（Skin search）：皮肤搜索框、Skin Database / Skins directory / Edit skin texture / 刷新工具栏，以及皮肤网格列表。列表**内部滚动**，高度固定为 6 个完整可见行（`TeeSkinGridVisibleRows = 6`，行高 50），不再跟随其它卡片拉伸。
  - 右列 `deck:tee-identity`（Player preview）：玩家预览、皮肤名输入、随机皮肤、**自定义颜色**勾选、随机颜色按钮，以及**身体（色码输入 + 3 条 HSLA 滑条）与脚（色码输入 + 3 条 HSLA 滑条）**。颜色控件从原 Options 卡**移到**这里，下方卡片不再重复一份。
  - 下方整宽 `deck:tee-skin-options`（Skin options）：4 个下载相关勾选、皮肤前缀区（标签 / 输入框 / kitty / santa / 皮肤排序下拉 / 显示作者日期）、默认眼睛一行，以及**轮换队列与预设面板**。
- 效果目标：打开「自定义颜色」让右列卡片变长时，皮肤列表仍在左列顶部完整可见可点，不会被推到首屏下面。
- 预设改为整宽卡内的宽卡片网格：每个预设一张卡，左侧名称（内置预设显示「Default preset」/「Server preset」，其余显示用户名称），右侧显示队列数量。列数由卡片宽度计算（`ResolveSettingsTeePresetGridColumns`，单卡最小宽度 `max(120 * UiScale, 90)`，上限 12 列），行数 = `ceil(预设数 / 列数)`。可见行数不再夹在 2~3 行。
- 预设面板高度有一道**可用高度护栏**：护栏取左列皮肤列表的可见高度减去队列列表自身占用（`ResolveSettingsTeeQueuePresetAvailableHeight`），避免预设越存越多把整页无限撑长；护栏只收敛行数，至少保留一行。
- 预设的既有行为全部保留：单击应用（`ApplySkinQueuePreset`）、重命名（`PopupSkinQueuePresetRename`）、删除（`RemoveSkinQueuePreset`）、内置预设不可删（`PresetActiveIndex >= 2` 才可删）、`ActivePresetIndex` 高亮（卡片 + 左侧高亮条）、`QueueDirty` 驱动的保存按钮可用性、队列启用开关、切换间隔、队列列表拖拽排序与单项删除。
- 皮肤列表双击：
  - **双击左键** → 把该皮肤（含自定义配色）应用到**本体**（`m_ClPlayerSkin` 一侧）。
  - **双击右键** → 应用到**分身**（`m_ClDummySkin` 一侧）。
  - 单击语义不变：单击仍然只改当前 Player / Dummy 子标签的编辑对象，右键单击不会改变编辑对象。
  - 皮肤列表项悬停提示新增说明：「Double-click: left applies to main, right applies to dummy」。
- 老配置升级：新增布局迁移 `qm_card_layout_version` 8，把旧分组（左=预览、右=选项、整宽=列表）迁移到新分组（左=列表、右=预览、整宽=选项）。已自定义过列位的用户保持不动。

## 手动验收

以下为待执行的手动验收清单，**不代表已通过**。按本次约定不进行编译、不运行 testrunner。

1. 打开 设置 → Tee → Player。确认左列是搜索框 + 皮肤列表（列表可滚动），右列是预览 + 皮肤名 + 自定义颜色/随机颜色，下方整宽卡是下载勾选 + 前缀 + 默认眼睛 + 皮肤队列 + 预设面板。
2. 勾选右列的「自定义颜色」。确认右列卡片变高、出现身体/脚的色码输入与 3+3 条滑条，且**左列皮肤列表仍然完整可见**、可以直接点击换皮肤（这是需求 1 的核心）。
3. 取消勾选「自定义颜色」，确认两组颜色控件消失、右列卡片变回原高度，列表位置不变。
4. 切换 Player / Dummy 子标签，确认右列的预览、皮肤名、颜色控件与下方队列/预设都跟随当前子标签；皮肤列表选中态也跟随。
5. 点击 `Save as` 连续保存多个预设（≥8 个）。确认预设以**整宽卡片网格**排布，名称与数量一目了然，一屏内能看到全部（或远多于 3 行）而**不需要往下拖**（需求 2）。
6. 把窗口宽度拉窄 / 拉宽，确认预设列数随之变化（窄→1~2 列，宽→4~8 列），卡片高度重新测量后仍完整显示、没有被裁切。
7. 点击某个预设，确认它被应用（当前队列内容变化、`Queue preset: xxx` 标签更新、该卡片出现高亮条）；点 `Rename` 改名、点 `Delete` 删除（前两个内置预设的 Delete 按钮应为禁用）。
8. 在皮肤列表里**双击左键**点一个皮肤。确认**本体**换成了该皮肤（Player 子标签的皮肤名同步、预览更新），分身不受影响（需求 3）。
9. 在皮肤列表里**双击右键**点另一个皮肤。确认**分身**换成了该皮肤，本体保持上一步的结果。
10. 单击一个皮肤，确认只有当前子标签的编辑对象改变；用右键单击一个皮肤，确认编辑对象**没有**被切换（单击语义不变）。
11. 悬停皮肤列表项，确认提示里出现「双击左键改本体 / 双击右键改分身」的说明。
12. 老配置升级：用一份 `qm_card_layout_version = 7` 且卡序为旧分组（`deck:tee-identity|tee|left|0`、`deck:tee-skin-options|tee|right|0`、`deck:tee-skin-list|tee|full|0`）的配置启动，确认 Tee 页落到新布局，且 `qm_card_layout_version` 变成 8、后续启动不再重复迁移。
13. 老配置升级（已自定义）：把其中一张卡（例如 `deck:tee-identity`）手工改到别的列，确认启动后**没有**被强制迁移（保留用户的列位），但版本号仍推进到 8。

## 实现记录

### 改动文件

- `src/game/client/components/menus_settings.cpp`
  - `RenderSettingsTee`（原 ~1367 行起）：重建高度常量与卡片装配。
    - 新增/改写：`TeeSkinGridVisibleRows = 6`、`TeeSkinGridRowHeight = 50.0f`、`ListContentHeight = ResolveSettingsTeeSkinListContentHeight(...)`、`TeeCustomColors`、`IdentityContentHeight = ResolveSettingsTeeIdentityHeight(TeeMetrics, *pUseCustomColor != 0)`、`TeeQueuePresetAvailableHeight`。
    - `BuildDefinitions`：`vCards.reserve(3)`，三张卡分别以 `deck:tee-identity`（右列）、`deck:tee-skin-list`（左列）、`deck:tee-skin-options`（整宽）装配；选项卡的 Measure 使用 `ContentWidth`（预设列数依赖宽度）。
  - `RenderOptions`（原 ~1532 行）：移除身体/脚颜色控件；保留勾选、前缀、默认眼睛；新增原 `RenderList` 的整个队列面板（启用开关、切换间隔、队列列表拖拽、清空），并以就地 lambda `RenderPresetPanel` 渲染预设宽卡片网格。
  - `RenderIdentity`（原 ~1724 行）：保留预览/皮肤名/随机皮肤/自定义颜色/随机颜色；新增从 Options 卡移来的颜色码输入与 `RenderHslaScrollbars` 两组。
  - `RenderList`（原 ~1918 行）：删除队列面板与 `QueuePanelWidth` 的横向切分，`MainView` 现在整块给列表；新增共享 lambda `ApplySkinListEntry`（单击与双击共用）与网格项的双击处理。
- `src/game/client/QmUi/SettingsPageLayout.h`：新增/改写 Tee 布局纯函数（详见下节）。
- `src/game/client/QmUi/QmCardRegistry.cpp`：三张卡的默认列归属改为左/右/整宽，标题与关键词更新（`colors` 关键词归到 `deck:tee-identity`）。
- `src/game/client/components/qmclient/tee_skin_apply.h`（新增，header-only）：`ETeeSkinApplyTarget`、`QmTeeSkinApplyTargetForButton`、`QmTeeSkinApplyTargetDummy`、`QmApplyTeeSkinToTarget`，仿 `scoreboard_skin.h` 的 `QmCopyScoreboardSkin`。这是**新文件但不进 CMake**（header-only，被 `menus_settings.cpp` 与测试同时 include）。
- `src/game/client/components/menus.cpp`：新增 `version < 8` 布局迁移块。
- `src/engine/shared/config_variables_qmclient.h`：`QmCardLayoutVersion` 的 max 由 7 提到 8（否则 `= 8` 会被 `CConfig` 的 clamp 逻辑压回 7，导致每次启动重复迁移）。**这一处不在任务给出的「可以改」文件清单里**，是版本推进能否生效的必要条件；只改了这一个数字，未动其它配置项，且 `menus.cpp` 本身就是允许改的文件，迁移逻辑仍落在 `menus.cpp`。
- 测试：`src/test/QmAnimTest.cpp`、`src/test/QmLayoutTest.cpp`、`src/test/qm_card_registry_test.cpp`、`src/test/settings_card_deck_logic_test.cpp`、`src/test/qm_new_ui_menu_branch_test.cpp`、`src/test/skins_test.cpp`。

### 新增卡与 stable id

**最终没有新增卡。** 中间版本曾注册 `deck:tee-skin-presets`，但在装配时发现把 `RenderOptions` 同时挂到两张卡会让下载选项/前缀/默认眼睛被渲染两遍；改为「队列与预设并入 `deck:tee-skin-options`（整宽）」后该卡被删除。因此本次没有新增 stable id、没有新增 registry 条目，`QmCardRegistry.cpp` 只改了既有三张卡的默认列归属：

| stable id | 旧默认 | 新默认 |
| --- | --- | --- |
| `deck:tee-identity` | Left 0 | **Right 0** |
| `deck:tee-skin-options` | Right 0 | **Full 0** |
| `deck:tee-skin-list` | Full 0 | **Left 0** |

### 布局迁移 version 8

`src/game/client/components/menus.cpp` 中新增 `if(g_Config.m_QmCardLayoutVersion < 8)` 块（紧跟 `< 7` 块之后）：

- 用 `ClassifyExplicitLayout(m_QmGlobalCardOrder, vPriorTeeLayout, {"deck:tee-identity"})` 判定旧配置是否仍处于 v4 目标列位（identity=1/0、options=2/0、list=0/0）。`INVALID` 时直接返回并保持版本待处理（与既有迁移写法一致）。
- 再对 `Candidate` 做**逐卡列位校验**：三张卡必须都处在「旧默认（0/0,1/0,2/0）」或「v4 目标（1/0,2/0,0/0）」的已知列位之一，且 `TabContainsOnlyStableIds(Candidate, "tee", {三张卡})` 成立；否则视为用户自定义，不迁移。
- 迁移动作按 `MoveToTab` 的 erase+insert 语义顺序执行：`tee-skin-list → (tee,1,0)`（左列）、`tee-identity → (tee,2,0)`（右列）、`tee-skin-options → (tee,0,0)`（整宽列）。三张卡各占一列，目标列内顺序与插入次序无关。
- 通过 `PersistCandidate` 写回，然后置 `m_QmCardLayoutVersion = 8`。

### 预设宽卡与可见行数

纯函数（`SettingsPageLayout.h`）：

- `ResolveSettingsTeeQueuePresetChromeHeight(Metrics)` = `4*LineSpacing + LineHeight + ButtonHeight`（Surface 上下内边距 + 标题 + 操作按钮 + 标题前间距）。
- `ResolveSettingsTeeQueuePresetRowHeight(Metrics)` = `max(ListRowHeight, 2*LineHeight + LineSpacing)`（名称一行 + 数量一行）。
- `ResolveSettingsTeePresetGridColumns(AvailableWidth, Metrics)` = `clamp(int(AvailableWidth / max(max(120*UiScale, 90), AvailableWidth * 0.24)), 1, 12)`。
- `ResolveSettingsTeePresetGridGeometry(Metrics, PresetCount, AvailableWidth, AvailableHeight)` → `{Columns, Rows, RowHeight, ListHeight, PanelHeight}`；`Rows = ceil(PresetCount / Columns)`（0 个预设保留 1 行），传入 `AvailableHeight > 0` 时把行数夹到 `max(1, floor((AvailableHeight - Chrome) / RowHeight))`。
- `ResolveSettingsTeeQueuePresetAvailableHeight(Metrics, QueueCount, SkinListHeight)` = `max(0, SkinListHeight - QueueChrome)`，其中 `QueueChrome = 6*LineSpacing + 2*LineHeight + QueueViewport`。
- `ResolveSettingsTeeQueuePanelGeometry(..., AvailableWidth, PresetAvailableHeight)`：`ContentHeight = max(440*UiScale, QueueChrome + PresetPanelHeight)`，并回传 `m_PresetColumns` / `m_VisiblePresetRows` / `m_PresetPanelHeight`。
- 测量与渲染共用同一条算式：卡片 Measure 用 `ResolveSettingsTeeOptionsContentHeight(...)`（= Top + Eyes + SectionGap + 面板高度），渲染阶段用同一套参数再算一次 `ResolveSettingsTeeQueuePanelGeometry`（`ContentWidth` 取卡片内容宽度 `MainView.w`），列数直接取 `m_PresetColumns`，可见行数由 `m_PresetPanelHeight` 反推列表高度后除以行高。

数值示例（`ResolveSettingsContentMetrics(1920)`，即 1920 宽、UiScale=1）：单卡最小宽 120 → 1200 宽卡片得 6 列，6 个预设 = 1 行；18 个预设无护栏 = 3 行，护栏 120px 时收敛到 1 行。

### 双击实现方式

- 用的是 `CUi::DoDoubleClickLogic(const void *pId)`（`src/game/client/ui.h:974`、`src/game/client/ui.cpp:1095`）。**`IInput` 里没有 `MouseDoubleClick()`**，全仓库唯一的双击检测就是这个函数；它没有按键参数，同一个 `pId` 每帧只能调用一次（内部是有状态机，无条件每帧调用会在第二帧起持续返回 true）。
- 按键区分来自 `DoButtonLogic` 的返回值（`ui.cpp:996` 的 `1 + 按键序号`）：`Ui()->DoButtonLogic(SkinListEntry.ListItemId(), 0, &Item.m_Rect, BUTTONFLAG_LEFT | BUTTONFLAG_RIGHT)`，`1 = 左键`、`2 = 右键`。不用 `Ui()->MouseButton(1)`：双击在**释放帧**触发，那时右键已经抬起，`MouseButton(1)` 必然为 false。
- 调用形态（点击门控 + 每帧每项一次）：
  ```cpp
  const int ItemButton = Ui()->DoButtonLogic(SkinListEntry.ListItemId(), 0, &Item.m_Rect, BUTTONFLAG_LEFT | BUTTONFLAG_RIGHT);
  if(ItemButton != 0 && Ui()->DoDoubleClickLogic(SkinListEntry.ListItemId()))
  {
      const ETeeSkinApplyTarget Target = QmTeeSkinApplyTargetForButton(ItemButton);
      ApplySkinListEntry(SkinListEntry, Target, QmTeeSkinApplyTargetDummy(Target) != (m_Dummy ? 1 : 0));
  }
  ```
- 左侧列表项原单击路径（`DoNextItem` 内部的 `BUTTONFLAG_LEFT`，鼠标释放时把 `m_ListBoxNewSelected` 置为新项）保持不动；应用动作统一走 `ApplySkinListEntry(vSkinList[NewSelected], m_Dummy ? DUMMY : MAIN, false)`。同一个列表项现在会调**两次** `DoButtonLogic`，但用的是同一个 `pId` 且按键集合一致、参数相同，热项/激活项状态不冲突。
- `ApplySkinListEntry` 先把皮肤（含颜色键）写到当前子标签对应的指针（`pSkinName` / `pUseCustomColor` / `pColorBody` / `pColorFeet`），再在「目标角色 ≠ 当前子标签」时额外调用 `QmApplyTeeSkinToTarget(g_Config, Target, ...)` 写另一侧；最后 `SkinList.ForceRefresh()` + `SetNeedSendInfo()`，仅当目标就是当前编辑对象时才 `m_SkinListScrollToSelected = true`。
- 列表容器仍是 `CListBox`，所以每次迭代仍然调一次 `DoNextItem`，滚动区域内容高度与滚动条不受影响。

### 测试位置

- `src/test/QmAnimTest.cpp`
  - `SettingsPageLayout.TeeQueueListViewportUsesCompleteRowsAndPrioritizesQueueSpace`（改写）：队列 viewport/surface/内容高度新算式、布局 revision。
  - `SettingsPageLayout.TeePresetGridFillsWideCardInsteadOfClampingToThreeRows`（新增）：预设列数/行数/面板高度、可见高度护栏、`ResolveSettingsTeeVisiblePresetRows` 不再夹 2~3 行。
  - `SettingsPageLayout.TeeIdentityPreviewReservesSemanticHeight`（改写）：自定义颜色开关对身份卡测量的影响。
  - `SettingsPageLayout.TeeSkinListCardKeepsVisibleGridRows`（新增）：列表卡固定高度、眼睛行高、勾选/前缀区高度。
  - `SettingsPageLayout.TeeOptionsCardReservesQueueAndPresetPanel`（新增）：整宽卡高度 = Top + Eyes + SectionGap + 面板高度；护栏生效/失效对比。
- `src/test/QmLayoutTest.cpp`（新增，纯单元测试）：`QmTeeSkinApply.ButtonResultSelectsTheTargetRole`、`QmTeeSkinApply.WritesOnlyTheRequestedRoleAndKeepsTheOtherSideUntouched`、`QmTeeSkinApply.EntryWithoutColorKeyKeepsExistingColorsAndTogglesOff`。
- `src/test/qm_card_registry_test.cpp`：`CoversCurrentSettingsDeckIds`、`TeeStandardPageUsesTwoColumnCards`（改写自 `TeeStandardPageUsesThreeFunctionalCards`）、`TeeFunctionalSearchTargetsSplitCards`（`colors` 改指 `deck:tee-identity`）。
- `src/test/settings_card_deck_logic_test.cpp`：`SettingsCardDeck.ProductionPagePlacementsPreserveWideColumnsAndNarrowReadingOrder` 的 tee 期望列与阅读顺序。
- `src/test/qm_new_ui_menu_branch_test.cpp`（源码结构测试）：
  - `TeeStandardPageUsesUnifiedSettingsStack`（既有断言随新实现更新）。
  - `TeeTwoColumnLayoutKeepsListVisibleAndRendersEachCardOnce`（新增）：不再横向切队列面板、列表卡按固定行数测量、整宽卡承载队列/预设、**每张卡只挂一次渲染器**、预设宽卡片网格、双击调用形态与单击语义。
  - `TeeTwoColumnLayoutMigratesLegacyCardPlacementAtVersionEight`（新增）：v8 迁移识别旧分组、三条 `MoveToTab` 的顺序、config 上界为 8。
- `src/test/skins_test.cpp`：`Skins.SkinQueuePresetsAreSelectableEditableQueues` 的队列几何调用字符串随签名更新。

### 只读审查结论

- 装配阶段自查发现的真实缺陷（已修）：曾把 `RenderOptions` 同时挂给 `deck:tee-skin-options` 与新增的 `deck:tee-skin-presets`，会导致下载选项/前缀/默认眼睛渲染两遍；已合并为单张整宽卡，并补了「每张卡只挂一次渲染器」的结构断言。
- `QmCardLayoutVersion` 的 config 上界是 7，写 8 会被 `CConfig` clamp 回 7 并使迁移每次启动重复执行；已把上界提到 8。
- 单击语义核对：`ApplySkinListEntry` 复刻原 3237-3257 的字段赋值，但不设置 `m_SkinListScrollToSelected`（原实现也没有），避免单击后页面被强行滚动；`SkinList.ForceRefresh()` 与 `SetNeedSendInfo()` 保持原样。
- 双击按键判定核对：`DoDoubleClickLogic` 在释放帧返回 true，因此不能用 `Ui()->MouseButton(1)`；改用 `DoButtonLogic` 的 `1 + 按键序号` 返回值。
- 未发现未修正的遗留问题。

### quick 门禁原始数字

- 首次运行（`tmp/c4_tee_two_column_gate.log` 已覆盖为最终结果）：`有效结果: FAIL`，通过 10 / 警告 0 / 失败 1，唯一失败为 `代码格式干跑检查` → `src\game\client\components\menus_settings.cpp:3639:123: error: code should be clang-formatted`。
- 对该文件执行 `clang-format -i` 后复跑：**`通过: 11 / 警告: 0 / 失败: 0`，`有效结果: PASS`**，日志 `tmp/c4_tee_two_column_gate.log`（命令 `python qmclient_scripts/gate/check_gate.py --mode quick`）。
- 设置页统一 UI 迁移合同 `check_settings_ui_migration.py --all`：24 个页面全部 `clean`（含 `tee: clean`）。
- 逐文件 `clang-format --dry-run --Werror` 对本次改动的 11 个 C++/头文件全部 ok。

### 未执行项与 gaps

- **没有编译**（未跑 `game-client`）、**没有运行任何测试**（未跑 `testrunner` / `run_cxx_tests` / `run_rust_tests`）。新增与改写的断言只做了源码级核对，未由编译器或 testrunner 验证过。
- **没有运行 i18n 脚本**（extract_strings / generate_all / validate / review_duplicate_entries 均未执行）。
- 游戏内手动验收全部未执行（见上方清单）。
- 断言的编译通过与断言本身成立与否都未验证；`QmAnimTest.cpp` 中新增用例使用的 `maximum(...)`（来自 `base/math.h`）与 `TeeMetrics.m_UiScale` 数值关系未实测。
- 视觉细节（预设卡片与名称/数量的实际观感、`deck:tee-skin-options` 标题在整宽卡下是否仍准确、窄窗口下选项卡的堆叠效果）未做视觉验证。
- `RenderList` 的离屏预暖（`TeeSectionVisible` / `AdvanceListOffscreen`）现在只在左列列表卡被裁剪时触发；左列卡比原来的整宽卡更短，因此预暖触发时机比改动前略早，未实测其对加载节奏的影响。
- 预先存在的、本次未修的细节：右列底部按钮行里「随机颜色」宽度 110、随机皮肤按钮宽度 30（沿用原布局，未调整）；`SSettingsTeeQueuePanelGeometry::m_QueuePresetHeight` 与 `m_PresetPanelHeight` 现在取值相同，保留旧字段避免扩大改动面。

### 新增/改动的 `Localize` 源串

新增 1 条：

- `Double-click: left applies to main, right applies to dummy`（皮肤列表项悬停提示）

其余沿用既有源串，未新增；`deck:tee-skin-options` 的标题 / 描述 / 关键词改动发生在 `QmCardRegistry.cpp`，属于非 `Localize` 的注册表默认文本（标题仍是既有源串 `Skin options`，描述与关键词只用于搜索）。

补充：随卡片描述改动还新增了 3 条注册表默认描述源串，译文已由收口方补齐到 12 种语言（见
`qmclient_scripts/languages_qmclient/translations/i18n/tee_page_layout.toml`）。

### 收口修正：删除孤儿 registry 条目，否则 v8 迁移会静默失效（2026-09-18）

交付后复核发现本节上面「最终没有新增卡、没有新增 registry 条目」的结论与代码不符：`QmCardRegistry.cpp` 里仍留着
中间版本的 `{"deck:tee-skin-presets", "tee", ECardColumn::Full, 1, ...}` 注册项，`qm_card_registry_test.cpp` 的 id
清单里也列了它。该条目从不被 `BuildDefinitions` 装配（deck 在 `SettingsCardDeck.cpp:111` 与 `251-252` 对无定义的
条目有 `nullptr` 守卫，会直接跳过，所以不会崩、也不会画出空卡），但它会让**迁移完全失效**：

1. `CModel::LoadMerged`（`QmCardOrderModel.cpp:568-577`）会把 registry 默认里**任何**不在已存配置中的条目补进模型，
   因此 `deck:tee-skin-presets` 对每个用户都恒定存在于 `tee` 页；
2. `TabContainsOnlyStableIds`（`QmCardOrderModel.cpp:591-605`）只要发现该页存在任何不在白名单里的条目就返回 `false`；
3. 于是 `menus.cpp` 的 `if(g_Config.m_QmCardLayoutVersion < 8)` 迁移体被整段跳过，三张卡列位保持不变；
4. 但 `PersistCandidate(Candidate, /*CandidateChanged=*/false)` 在「无改动且模型不脏」时直接返回 `true`，
   紧接着 `g_Config.m_QmCardLayoutVersion = 8;` 照常写入；
5. 版本已到 8 → 迁移此后永不重试。**结果是所有已有配置的用户（包括发起本次需求的用户）永远停在旧布局**，
   而代码与文档都以为已经迁移完成。

修复：删除该孤儿注册项与测试清单里的对应 id，并在 `src/test/qm_card_registry_test.cpp` 新增
`TeeTabHasNoExtraCardThatWouldSilentlySkipVersionEightMigration`——用真实 `qm_card_registry::BuildDefaultEntries()`
经 `LoadMerged` 建模后断言 `TabContainsOnlyStableIds(Model, "tee", {三张卡})` 为真。原有的
`TeeTwoColumnLayoutMigratesLegacyCardPlacementAtVersionEight` 是**纯源码文本断言**，只 grep 字符串、不执行迁移逻辑，
因此抓不到这个问题；新用例守住的是迁移的前置条件本身。删除后 `Configure the skin rotation queue and saved presets`
这条描述也从抽取结果中消失。
