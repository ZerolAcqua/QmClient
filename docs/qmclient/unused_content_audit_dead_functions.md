# QmClient 客户端死函数清单（已核实）

> 只读审计产物。判定口径：`.cpp` 中存在定义，且该标识符在整个 `src/`（含 `src/test/`）+ `datasrc/`
> + 仓库其余文本中，除定义处与头文件声明外**没有任何调用点**。
> 已排除的假阳性：由生成代码调用的 `CUnpacker::*OrDefault`（`datasrc/datatypes.py`）、
> 由 JS 胶水调用的 `EmscriptenCallback*`（`other/emscripten/minimal.html`）、
> 按地址注册的回调（`CurlDebug`、`SdlCallback`、`websocket_log_callback`、`Conchain*` 等）。
>
> 原始证据：`tmp/unused_audit/final_dead_list.txt`、`verify_report.txt`、`outside_src_hits.txt`。

## 汇总

| 分组 | 数量 |
| --- | --- |
| 直接核实为死函数 | 95 |
| 传递性死代码（死回调的模板 worker） | 1（`AssetScan<T>`） |
| 死非函数产物 | 1（`SMenuAssetScanUser LazyLoadUser;`） |

## 1. 已成整簇的死代码（优先清理对象）

### 1.1 `CUiEffects` 的整套动画辅助 API —— 8 个方法全死

`components/ui_effects.cpp:205, 217, 225, 233, 251, 261, 266, 271`

`CreateSmoothValue`、`SetSmoothValue`、`GetSmoothValue`、`UpdateSmoothValue`、
`PulseColor`、`GetPulse`、`GetWave`、`GetBounce`

组件本身仍活着（`gameclient.cpp:644` AddComponent、`:3031` StartScreenshotAnimation），
但这 8 个方法没有任何调用点 —— 整套缓动/脉冲 API 是历史遗留。

### 1.2 旧版资源列表装载器 —— 10 处 + 模板 worker

`components/menus_settings_assets.cpp`

`InitAssetList:4104`、`LoadAsset:1515`（各仅 1 次出现 = 只有定义）、
7 个扫描回调 `LoadEntities:1205`、`EntitiesScan:1276`、`GameScan:1573`、`EmoticonsScan:1580`、
`ParticlesScan:1587`、`HudScan:1594`、`ExtrasScan:1601`（各 2 次 = `menus.h:1281-1288` 声明 + 定义）

该文件里唯一的 `ListDirectory(..., pfnCallback, ...)` 就在已死的 `InitAssetList:4113` 内；
现役路径走 `ScanCallback` / `RecursiveMapScanCallback`（`:928/:983`）+ `InitSearchList`（9 处调用）。
传递性死代码：模板 worker `AssetScan<T>`（`:1536`），其 5 个调用点全在上面这组死回调里。

### 1.3 TClient 侧外围功能

| 函数 | 位置 |
| --- | --- |
| `HasSwapCountdown`、`HasBlockingGoresWeapon`、`IsFastInputOthersActive`、`IsGoresMapProgressDebugRouteEnabled` | `components/tclient/tclient.cpp:1509, 3658, 3648, 3782` |
| `PracticeDummyId`、`IsPracticeDummy`、`FindNearestSafeRescuePosition`、`AdvanceBaseWorldToTick` | `components/tclient/fast_practice.cpp:441, 446, 2302, 1313` |
| `UpdateWarEntry`、`GetNameWar`、`GetClanWar`、`GetReason`、`SortWarEntries` | `components/tclient/warlist.cpp:230, 469, 475, 487, 497` |
| `CheckBindChat` | `components/tclient/bindchat.cpp:181` |
| `IsRemoteVersionNewer` | `components/tclient/version_compare.cpp:91` |
| `BuildTClientLeftCacheSections`、`BuildTClientRightCacheSections` | `components/tclient/menus_tclient.cpp:1599, 1608` |

### 1.4 QmClient 侧

| 函数 | 位置 |
| --- | --- |
| `GetFastInputPredictionAmountMs`、`GetFastInputPredictionTicks`、`GetFastInputRenderAmountMs`、`IsQmVoiceSupportedClient`、`OpenSponsorPage` | `gameclient.cpp:6961, 6971, 6976, 8775, 1062` |
| `ApplyQmNewsPayload`、`HasQmClientRecognitionService`、`QmClientDistributionSyncing`、`QmRealtimeRestart` | `components/qmclient/qmclient.cpp:947, 1407, 1412, 1645` |
| `TryTranslateOutgoingChat`、`ContainsChinese` | `.../translate/translate.cpp:1645, 1789` |
| `ITranslateBackend::CompareTargets` | `.../translate/translate.cpp:623`（虚函数，但树内无覆写与调用点） |
| `ActiveSource` | `.../music_lyrics/music_lyrics_integration.cpp:429` |
| `FindMinLiveQueuedSeq`、`RenderSpeakerOverlay` | `.../voice/voice_core.cpp:145, 2395` |
| `CountRequestsForPlayer` | `.../axiom_scores.cpp:170` |
| `MarkMetadataReady` | `.../settings_resource_preview.cpp:47` |
| `MeasureEditorPreviewRect`、`ShouldSuppressServerChat` | `.../hud_notifications/hud_notifications.cpp:102, 251` |
| `FinishSettingsQmScrollContainer` | `.../menus_qmclient.cpp:1437` |
| `AddGlobal`（模板，从未实例化） | `.../scripting/impl.cpp:149` |

### 1.5 菜单 / 编辑器 / Demo

`AssetsEditorReloadAssets`（`menus_assets_editor.cpp:703`）、
`AudioPackEditorCopyAbsoluteFileToStorage`（`menus_settings.cpp:4822`）、
`DoButton_CheckBox_Number`（`:5635`）、`RequestMenuTextContainerBuild`（`menus.cpp:2082`）、
`RequestSettingsCardFocus`（`menus.cpp:5938`）、`RenderLanguageSettings`（`menus.cpp:5431`）、
`EnsureDemoSize`（`menus_demo.cpp:1484`）、`PopupConfirmDeleteDemo`（`:3494`）、
`PopupConfirmDeleteFolder`（`:3507`）、`DoSettingsControlsMenuLabel`（`menus_settings_controls.cpp:163`）

### 1.6 渲染 / 数据 / 缓存 / 引擎

| 函数 | 位置 |
| --- | --- |
| `InvalidateLineTranslation` | `components/chat.cpp:595` |
| `PrewarmByCountryCodes`、`PrewarmByIndices` | `components/countryflags.cpp:318, 327` |
| `SkinListReady`、`SkinListSkeletonReady`、`LoadedSkinLimit` | `components/skins.cpp:2511, 2522, 1383` |
| `LoadSkinPart` | `components/skins7.cpp:246` |
| `GetTextureScale` | `components/mapimages.cpp:495` |
| `GetSectorPosition`、`IsMouseInCenter` | `components/pie_menu.cpp:530, 614` |
| `DbgRender` | `components/flow.cpp:22` |
| `DoSubheader` | `ui_listbox.cpp:285` |
| `ContentAreaPos` | `ui_scrollregion.cpp:202` |
| `RecordDrawCall`、`RecordClipPush` | `QmUi/QmRender.cpp:11, 16` |
| `ResolveRoot` | `QmUi/QmLayout.cpp:56` |
| `ResolveUiAnimValueRect` | `QmUi/QmAnimResolve.cpp:134` |
| `SettingsSectionCacheKey`、`SettingsTextCacheKey`、`SettingsResourceCacheKey` | `components/settings_runtime_cache.cpp:278, 283, 288` |
| `SettingsSkinListSelectionStillValid`、`SettingsSkinListScrollResetNeeded` | `components/settings_resource_jobs.cpp:374, 379` |
| `CopyBufferObjectInternal` | `engine/client/graphics_threaded.cpp:3570` |
| `CGLSLProgram::DetachShader`（只用 `DetachShaderById`） | `engine/client/backend/opengl/opengl_sl_program.cpp:42` |
| `SortCompareNumClients` | `engine/client/serverbrowser.cpp:438` |
| `CHttpRequest::ResultLastModified` | `engine/shared/http.cpp:705` |
| `CConfigTagsManager::HasTag`、`GetAllAvailableTags` | `engine/shared/config_tags.cpp:343, 349` |

### 1.7 非函数死产物

`SMenuAssetScanUser LazyLoadUser;`（`components/menus_settings_assets.cpp:4727-4728`）
构造后 `m_pUser` 被赋值，此后再无使用。

## 2. 平台条件编译下的重复定义（不是死代码，但易误判）

同一 `ConfigName` + 同一脚本名在平台/架构守卫下各定义一次，每个平台只生效一条：

`cl_touch_controls`（`config_variables.h:30/:32`，Android/else）·
`cl_skins_loaded_max`（`:162/:164`，IA32/else）· `gfx_borderless`（`:451/:458`）·
`gfx_fullscreen`（`:453/:455/:459`，3 变体）· `gfx_backend`（`:811/:813/:815/:817`，4 变体）·
`tc_allow_any_res`（`config_variables_tclient.h:11/:13`，Windows/else）

## 3. 明确「不是死代码」

- `#if 0`：`src/game/client`、`src/engine/client`、`src/engine/shared` 内**一个都没有**；
  仅存在于第三方捆绑库（kcp / rnnoise / wavpack）。
- `if(false)`：仅 `src/game/mapbugs.cpp:84`，是 DDNet 用来锚定 `MAPBUG`/`else if` 链的惯用法。
- 无「未实现」桩分支；只有 `src/base/fs.cpp:633` 等平台的 `#error` 守卫。
