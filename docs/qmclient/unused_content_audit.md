# QmClient 客户端「未在使用的内容」审计

> 只读审计，未改动任何源码。证据脚本与原始结果：`tmp/unused_audit/`
> （`audit2.py`、`verify_configs.py`、`final_config.py`、`config_audit.json`、`verify.json`、`final_config.json`）。
> 死函数完整清单另见 `docs/qmclient/unused_content_audit_dead_functions.md`。

## 一、结论摘要

客户端**没有大规模死文件 / 死组件**：仓库自带门禁在 quick 模式下全部通过
（`check_unused_header_files.py`：无未使用头文件；`check_gate.py --mode quick`：11 通过 0 失败）。
但门禁不查函数级引用，补充审计后确认**死函数是本次最大的一类**（95 个，详见 §五）。
「注册了但客户端用不上」的配置项是第二大类。

| 类别 | 数量 | 判定口径 |
| --- | --- | --- |
| 配置项在生产代码中 0 处或仅 1 处引用 | 45（Qm 33 / Tc 12） | 全 `src/` 词边界统计，只算非测试代码；独立复算得 Qm 37 / Tc 22，差异来自「是否计入头文件声明与测试」 |
| 其中：用户完全无法在设置页配置 | 41 | 成员名不出现在任何设置页 / UI 源文件 |
| 其中：开关无效（设置页可改但无消费方） | 6 | 见 §四；含 `qm_better_scoreboard` 与 `qm_voice_group_mode` |
| 成员名在整个 `src/` 一次都不出现 | 1 | `tc_regex_chat_ignore` |
| 上游 DDNet 遗留真死配置 | 3 | 另 3 项是仓库自检误报，见 §六 |
| **死函数** | **95 + 1 传递性** | 见 §五与 `unused_content_audit_dead_functions.md` |
| 资源文件（777 个 / 41.6 MB）中确认无用 | 1（2,011 B） | 逐个回溯加载点；详见 §七 |
| 图集内无人请求的图标条目 | 每个图集 1/17 | `"satellite-swap"`，详见 §7.3 |
| 构建配置残留 | 9 行 | `datasrc/qm_icons/tabler` 目录已删，详见 §7.4 |

单处引用**不等于**功能一定坏：多数是「代码读一次即用」，只是**缺少设置页入口**（见 §三）；
真正「有开关但开关无效」的只有 §四。

## 二、配置项本身完全无引用（1 项）

| 配置 | 位置 | 描述 |
| --- | --- | --- |
| `tc_regex_chat_ignore` | `config_variables_tclient.h:134` | 成员 `m_TcRegexChatIgnore` 在 `src/` 中 0 次出现。**注意不是功能缺失**：同名控制台命令经 `Console()->Chain("tc_regex_chat_ignore", ...)`（`tclient/tclient.cpp:1697-1711`）被拦下，校验后写入成员 `CTClient::m_RegexChatIgnore`，而真正消费它的是 `chat.cpp:1271`。也就是说功能走的是控制台命令 + 独立成员，配置项本身成了影子，且没有设置页入口。 |

## 三、用户完全无法配置（41 项）

这些配置**保留在 `config_variables_*.h` 中、有默认值、部分确实被代码读取**，
但成员名不出现在任何设置页文件里，也没有对应的界面入口 —— 实际永远等于默认值。

### 3.1 语音相关（7）

`qm_voice_audio_backend`、`qm_voice_whitelist`、`qm_voice_blacklist`、`qm_voice_mute`、
`qm_voice_vad_allow`、`qm_voice_name_volumes`、`qm_voice_off_nonactive`。
后 6 项都在 `voice_core.cpp` 被读取（名单项经 `SRClientVoiceConfigSnapshot` 注入），
**功能是通的，纯粹缺设置入口**；`qm_voice_audio_backend` 在 `voice_core.cpp:261` 被读入音频配置。
其中 5 个名单项在 `voice_core.cpp:1952-1956` 拷进 `SRClientVoiceConfigSnapshot`，
再由 `voice_core.cpp:1755-1758 / 1794` 消费。

### 3.2 音乐 Hook（6）

`qm_soda_hook_timeout_ms`、`qm_soda_hook_helper_path`、`qm_kugou_hook_timeout_ms`、
`qm_kugou_hook_helper_path`、`qm_qqmusic_hook_timeout_ms`、`qm_qqmusic_hook_helper_path`

在 `music_lyrics_integration.cpp:188,191` 被读取，但帮助程序路径实际硬编码，
错误提示写死 `qm-music-helper.exe`（同文件 440 行）。

### 3.3 实时 WebSocket（6）

`qm_websocket_protocol`、`qm_websocket_heartbeat`、`qm_websocket_backoff_base_ms`、
`qm_websocket_backoff_max_ms`、`qm_websocket_log`、`qm_websocket_allow_insecure_tls`

均在 `qmclient.cpp:1613-1682` 被读用于连接参数，但没有设置页入口。
最后一项的 help 文本自己写明是「Legacy option」，可视为待删。

### 3.4 皮肤队列 / 聊天动画 / 其它（22）

- 皮肤队列：`qm_skin_queue_length`、`qm_skin_queue_rotate_map`、
  `qm_dummy_skin_queue_length`、`qm_dummy_skin_queue_rotate_map`
- 聊天动画：`qm_chat_anim_slide_out`、`qm_chat_anim_fade_duration_ms`、`qm_hide_chat_bubbles`
- UI：`qm_rect_corner_segments`
- 运行状态：`qm_launch_count`（赞助提示计数）、`qm_jump_hint_defaults_migrated`（一次性迁移标记）
- TClient 侧（11）：`tc_allow_any_res`（`config_variables_tclient.h:11/13` 按平台各定义一次，各平台只生效一条，
  但客户端代码里仅在 `if(g_Config.m_TcAllowAnyRes == 0)` 处读一次且无 UI 入口）、
  `tc_hook_coll_cursor`（唯一引用是 `players.cpp:372` 被注释掉的 `//if(Local && g_Config.m_TcHookCollCursor)`，
  **功能已停用**）、
  `tc_color_freeze_darken`、`tc_color_freeze_feet`、`tc_old_team_colors`、
  `tc_showhud_dummy_position`、`tc_showhud_dummy_speed`、`tc_showhud_dummy_angle`、
  `tc_mod_weapon`、`tc_discord_rpc`、`tc_regex_chat_ignore`

## 四、开关无效 / 读进无人消费的快照（6 项）

**4.1 设置页能改，但除设置页渲染那一行外，产品代码再无读取点**（勾选后无任何行为变化）：

| 配置 | 唯一引用点 | 影响 |
| --- | --- | --- |
| `qm_show_outdated_version_warning` | `menus_qmclient.cpp:2413` | 勾选后无行为变化 |
| `qm_better_scoreboard` | `menus_qmclient.cpp:2414` | 勾选后无行为变化（Enhanced scoreboard 未生效） |
| `dbg_qm_ui_dogfood` | `menus_qmclient.cpp:6165` | 调试用 UI 样板页门控（本身即调试项） |
| `qm_assets_preview_budget_mb_override` | `menus_settings_assets.cpp:4888` | 传入预览预算计算 |
| `qm_assets_preview_budget_percent` | `menus_settings_assets.cpp:4889` | 同上 |

**4.2 值被写进快照后无人消费**（值不产生任何效果）：

| 配置 | 位置 | 依据 |
| --- | --- | --- |
| `qm_voice_group_mode` | `config_variables_qmclient.h:277` | 仅 `voice_core.cpp:1946` 写入 `m_ConfigSnapshot.m_QmVoiceGroupMode`（字段声明 `voice_core.h:87`），该字段从无读取点。对照：同批的 `m_QmVoiceGroupGlobal:83`、`m_QmVoiceVisibilityMode:84`、`m_QmVoiceListMode:85` 都在 `voice_core.cpp:1755-1758` 被消费，唯独 `GroupMode` 没有 |

## 五、函数级死代码（95 个 + 1 个传递性）

完整清单见 **`docs/qmclient/unused_content_audit_dead_functions.md`**。判定口径：`.cpp` 中有定义，
且该标识符在 `src/`（含测试）+ `datasrc/` + 仓库其余文本中除定义/声明外无任何调用点。
已排除生成代码调用（`CUnpacker::*OrDefault`）、JS 胶水调用（`EmscriptenCallback*`）、
按地址注册的回调等假阳性。

成簇的死代码（优先清理）：

| 簇 | 规模 | 位置 |
| --- | --- | --- |
| `CUiEffects` 整套平滑/脉冲动画辅助 API | 8 个方法 | `components/ui_effects.cpp:205-271` |
| 旧版资源列表装载器（含传递性死模板 worker） | 9 个 + `AssetScan<T>` | `components/menus_settings_assets.cpp:1205-4104` |
| TClient 外围（交换倒计时、练习判定、战争列表、绑定聊天、版本比较、缓存分区） | 21 个 | `components/tclient/*` |
| QmClient 外围（新闻载荷、识别服务、实时重启、翻译、音乐、语音叠加层、积分请求计数、资源预览元数据、HUD 通知、脚本 `AddGlobal`） | 18 个 | `components/qmclient/*`、`gameclient.cpp` |
| 菜单 / 编辑器 / Demo | 10 个 | `menus*.cpp` |
| 渲染 / 数据 / 缓存 / 引擎 | 18 个 | 见清单 §1.6 |

另：`SMenuAssetScanUser LazyLoadUser;`（`menus_settings_assets.cpp:4727-4728`）构造后再无使用。

### 5.1 与本报告早期版本的差异（重要）

初版 §六 曾单独列出 5 个死函数，现已并入清单，归属如下：

| 函数 | 位置 | 在清单中的归属 |
| --- | --- | --- |
| `DrawTextFieldPlate` | `QmUi/UiForms.cpp:63` | 独立条目（精确重扫确认无引用点） |
| `SyncSkinQueueEntriesInPlace` | `components/skins.cpp:412` | 独立条目；仅被 `src/test/skins_test.cpp:2203/2225` 断言「源码中不应出现该调用」 |
| `FormatTrafficStatsValue` | `qmclient/monitoring/monitoring.cpp:244` | 独立条目 |
| `InitAssetList`、`LoadAsset` | `components/menus_settings_assets.cpp:4104, 1515` | §1.2 整簇旧资源列表装载器 |

初版的 5 条结论本身成立，差异只在于当时未发现它们分别属于更大的死代码簇。

## 六、上游 DDNet 遗留（3 项真死 + 3 项被仓库自检误报）
`python qmclient_scripts/check_config_variables.py --ddnet` 返回 1 并列出 6 项，但**其中 3 项是误报**：

| 配置 | 判定 | 依据 |
| --- | --- | --- |
| `cl_menu_panel_opacity` | **真死** | `config_variables.h:243`；除测试断言「菜单不得使用它」外无读取点 |
| `cl_menu_panel_elevated_opacity` | **真死** | `:244`，同上 |
| `cl_settings_tabbar_opacity` | **真死** | `:245`，同上（`qm_new_ui_menu_branch_test.cpp:2324-2326, 3045-3049`） |
| `cl_video_showhud` | 误报，**在用** | `components/qmclient/demo_display.h:26` 以 `Config.m_ClVideoShowhud` 读取 |
| `cl_video_showchat` | 误报，**在用** | 同文件 `:27` |
| `cl_video_show_direction` | 误报，**在用** | 同文件 `:23` |

误报原因：`check_config_variables.py:45-53` 的正则只认 `g_Config.m_X` / `Config()->m_X` /
`m_pConfig->m_X` / `Console()->(Register|Chain)("script")` / `ExecuteLine(...)`，
认不出 `Resolve(const CConfig &Config, ...)` 这种**换个名字的 const 引用**读取
（QmClient 引入只读配置快照后新增的写法）。这是工具本身的缺口，不是配置问题。

## 七、资源文件：777 个资源中只有 1 个确认无用

审计范围：`data/` + `datasrc/` 下 777 个资源文件（41,641,122 B），排除 `data/languages/`、
`data/data_version.txt` 与 vendor 目录。每个候选都回溯到具体加载点（字面量 / 目录扫描 / 配置表）。

### 7.1 确认无用（1 个，2,011 B）

| 文件 | 大小 | 证据 |
| --- | --- | --- |
| `data/input overlay-Zac/mouse-no-movement.json` | 2,011 B | `grep -r "mouse-no-movement\|no-movement\|no_movement" src/` → 无匹配（1277 个源文件）；全仓 1802 个文本文件扫描只命中 `CMakeLists.txt:1705` 的安装清单。`data/input_overlay.json` 只声明 `wasd.json` 与 `mouse.json` 两个布局，而 `input_overlay.cpp:26/:553` 只加载配置里点名的布局，并按 `<layout>.png` 推导贴图（`:1221-1226`）—— `mouse-no-movement.png` 也不存在，即使被选中也无法渲染。**但它确实被打包发布**（`EXPECTED_DATA:1705` + `copy_directory:2218`），属于纯白带体积 |

### 7.2 无运行时引用、但不随客户端发布（不计入无用体积）

- `datasrc/qm_icons/phosphor_{bold,fill,regular,thin}/` 共 **68 个 SVG**（30,970 B）：
  `src/` 运行时零引用，由 `CMakeLists.txt:2351-2431` 与 `qmclient_scripts/qm_build_icon_{atlas,msdf}.py`
  消费，`src/test/QmIconAtlasTest.cpp:248-255` 另点名 8 个。安装 glob 只覆盖 `data/`，所以不发布。

### 7.3 图集里有一个永远取不到的图标（全部 16 个图集）

每个 shipped 图集 JSON 有 17 个条目，其中名字恰为 `"satellite-swap"` 的那一条**没有任何运行时代码请求**：
`grep '"satellite-swap"' src/` → 无匹配，`EQmIcon` 只有 `SATELLITE_SWAP_INCOMING` /
`SATELLITE_SWAP_OUTGOING`（`qm_icon_manager.h:34-35`、`qm_icon_manager.cpp:59-61`）。
即每个图集约 1/17 的图集面积不可用 —— 属构建输入冗余，不是发布体积问题。

### 7.4 构建配置残留：`datasrc/qm_icons/tabler` 已不存在

`CMakeLists.txt` 仍有 9 行引用已删除的 Tabler 图标集（`:2356` glob，`:2362/2371/2380/2389/2407/2413/2419/2425`
的 `--source datasrc/qm_icons/tabler`），而 `datasrc/qm_icons/tabler/` 目录已不存在。
不报错：`qm_build_icon_atlas.py:55` 只在**所有** source 目录都收不到 SVG 时才 `SystemExit`，
Phosphor 目录仍能收到文件，所以 Tabler 是被静默丢弃的。

### 7.5 看像重复但都在用（不要误删）

- `data/gui_logo.png`（78,786 B）与 `data/qmclient/gui_logo.png`（437,700 B）：后者才是真横幅
  （`IMAGE_BANNER` → `menus_start.cpp:177`）；前者仅作旧安装探测的 fallback
  （`tclient.cpp:612-614` 的 `FileExists("gui_logo.png")`），单独删文件会让「跳过 QmClient.zip」的告警行为变化，需改代码。
- 字节相同对：`shader/media_island_sdf.vert` == `shader/prim.vert`；`icon-eye.svg` == `icon-satellite-spectator-eye.svg`（4 组），后者映射到不同 `EQmIcon`，都保留。
- 两套图标管线并存：`data/fonts/Phosphor-{Regular,Bold}.ttf`（983,944 B）与
  `data/qmclient/icons` 图集（413,783 B PNG + 28,272 B JSON），都在用；合并可省约 1 MB，但属功能取舍。
- `editor/entities/F-DDrace.png` 与 `editor/entities_clear/f-ddrace.png`：消费方不同（编辑器下拉 vs 运行时 mod 实体贴图），都在用。
- 其余全部澄清：251 个国家旗（与 `index.txt` 1:1）、103 个 `skins`、93 个 `skins7`、
  54 个 `mapres`、7 个 `data/fonts` + 9 个 `qmclient/fonts`、24+24 个 shader、
  15 张由 `datasrc/content.py` → 生成 `client_data.cpp` 的根图（`IMAGE_*` 枚举全部在 `src` 被引用）、
  16 张聊天表情、8 个主题、5 张菜单图、1 个社区图标。

### 7.6 未覆盖

未审计扩展名：`data/` 下 131 个 `.wv`、36 个 `.map`、16 个 `.rules`、10 个 `.txt`、1 个 `.chai`。

## 八、已排查并确认仍在使用（避免误判）

| 曾怀疑 | 结论 |
| --- | --- |
| `data/qmclient/fonts/*.ttf`（9 个） | 在用：`text.cpp:1422` 用 `Storage()->ListDirectory("qmclient/fonts")` 动态枚举，供自定义字体下拉 |
| `data/fonts/*`（30.5 MB） | 在用：由 `data/fonts/index.json` 注册，含语言变体与 fallback |
| `data/qmclient/icons/*_msdf.*` | 在用：`qm_icon_manager.cpp:332` 在 `HasTexturedMsdf()` 时优先加载 |
| `data/qmclient/chat_emojis/*.png`（16 张） | 在用：`chat_emoji.h` 定义表逐个引用 |
| `data/qmclient/builtinscripts/sayemoticon.chai` | 在用：`bindchat.cpp:29-33` 绑定 `!shrug` / `!flip` 等 |
| `src/qm-music-hook`、`src/qm-soda-hook`、`src/qm-nmt-hook` | 在用：CMake 构建 helper 可执行文件，`game-client` 依赖 `qm-nmt-hook64` |
| `src/antibot` | 默认不构建：`option(ANTIBOT ... OFF)`，仅服务端可选 |

## 九、可删 / 建议处理

1. **可直接删**：`tc_regex_chat_ignore` 配置项本身（功能已由同名控制台命令接管）；
   `qm_websocket_allow_insecure_tls`（help 文本自述 legacy）；
   `data/input overlay-Zac/mouse-no-movement.json`（无引用且缺同名 PNG，但仍被打包）。
2. **确认功能已停用、配置是残留**：`tc_hook_coll_cursor`（唯一引用为注释代码）；
   3 个 `cl_menu_panel_*` / `cl_settings_tabbar_opacity`。
3. **成簇删死函数**：§五 的 95 个，建议按簇处理 —— `CUiEffects` 8 个动画辅助、
   旧资源列表装载器 9+1 个、TClient 外围 21 个优先（彼此无依赖，可整簇移除）。
4. **建议补 UI 而非删除**：§三 的语音名单、皮肤队列长度、聊天动画时长等 —— 功能已实现，只缺入口。
5. **修开关或删开关**：§4.1 的 5 项（`qm_better_scoreboard`、`qm_show_outdated_version_warning` 等）
   与 §4.2 的 `qm_voice_group_mode`。
6. **构建侧清理**：`CMakeLists.txt` 中 9 行 `datasrc/qm_icons/tabler`（目录已不存在，静默丢弃）；
   图集里无人请求的 `satellite-swap` 条目。
7. **工具缺口**：`check_config_variables.py` 认不出 `Config.m_*` 形式的读取，会误报 3 个 `cl_video_*`，
   建议扩正则（§六）。
8. **保留**：`qm_launch_count`、`qm_jump_hint_defaults_migrated` 属状态记录，不是开关；
   `data/gui_logo.png` 删除需同时改 `tclient.cpp:612-614`。

## 十、未能确定性判定的部分（诚实标注）

- 函数级结论限定「树内无同名声明/调用」的普通函数；虚函数仅在确认树内无覆写与调用点时才计入
  （`ITranslateBackend::CompareTargets` 属此类，若仓库外有子类实现则可能被调用）。
- 未做逐平台构建：条件编译分支（Emscripten / Android / macOS）未逐一验证，
  审计过程中已因此修正 2 个假阳性（`EmscriptenCallback*`）。
- 资源审计未覆盖扩展名：`data/` 下 131 个 `.wv`、36 个 `.map`、16 个 `.rules`、10 个 `.txt`、1 个 `.chai`。
- 配置项「单处引用」的绝对数量对口径敏感：本报告口径得 45（Qm 33 / Tc 12），
  独立复算得 Qm 37 / Tc 22（差异在是否计入头文件声明与测试引用），两者都远小于「整片功能未接线」的量级。
- 「某调试项是否仍需要」「两套图标管线是否合并」属产品判断，本审计只陈述事实。
