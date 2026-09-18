# 纯白皮肤（白块 Tee）渲染缺陷

## 症状

同一名玩家在两张截图里都渲染成纯白色块，颜色为 (255,255,255) 且轮廓仍是 Tee 的 alpha（躯干、底部收尖、两只脚），没有黑边、没有眼睛、没有任何图案：

- 世界截图（288x281）：躯干为 x∈[95,157]、y∈[126,182] 的恒宽 63px 白块，底部 y∈[184,196] 收尖；块内深色像素为 0（右侧 x≥158 的深色像素属于地图砖块）。
- 界面行截图（325x78）：白块是 58x26 与 69x30 两个矩形，盖住文本 "822 … 13: 海上一只猫" 的中段，且被盖住处没有半透明文字残留，说明白块是后画的实心矩形。

只有部分玩家出现，且在同一会话内持续存在。

## 根因

6.x 皮肤贴图被卸载后，引用它的渲染信息仍然保存着旧句柄，而 `IGraphics::CTextureHandle::IsValid()` 只检查 `Id() >= 0`，所以游戏侧的 `IsDrawableTexture*`（`src/game/client/render.h:71-89`）判定为可绘制：

1. 皮肤容器被资源预算卸载：`CSkins::UpdateUnloadSkins()` → `TryUnloadContainer()` 走 LOADED 分支，`m_OriginalSkin.Unload()` / `m_ColorableSkin.Unload()` 释放纹理并把 `m_pSkin` 置空。
2. 目录扫描发现皮肤类型变化（LOCAL ↔ DOWNLOAD）时同样在 `ProcessSkinDirectoryScanJob()` 里释放纹理。
3. 两处都只改容器，没有通知客户端，`CGameClient::CClientData::UpdateRenderInfo()` 不会重跑；玩家的 `m_RenderInfo`、聊天头像与击杀提示的 `CManagedTeeRenderInfo` 快照里都是已释放的句柄。
4. 绘制时 `CGraphics_Threaded::TextureSet()` 发现句柄未分配，只在 `m_Debug` 下打日志并把它置为 -1（`graphics_threaded.cpp:1719-1729`），绘制照常提交，于是 Tee 被画成没有贴图的实心块。
5. 即使刷新一次，`UpdateRenderInfo()` 里"异步资源未就绪时复用上一份渲染信息"的分支（`gameclient.cpp:5610`）也会把这份失效句柄的渲染信息原样复制回去，缺陷因此稳定复现。

`cl_skins_loaded_max` 越大、服务器上皮肤越多，字节预算越容易触发，所以"谁变成白块"取决于谁在 LRU 尾部，表现为只有一部分人。

## 修复

- `src/game/client/components/skins.cpp`、`skins.h`：
  - 新增 `CSkins::UnloadLoadedSkinTextures()`，作为 6.x 贴图卸载的唯一入口（释放两套贴图、清 `m_pSkin`、清字节统计），并登记皮肤名。
  - 新增 `CSkins::QueueSkinTexturesUnloaded()`（按帧去重）与 `m_vSkinsTexturesUnloadedThisFrame`，在 `OnUpdate()` 末尾与未解析通知一样调用 `GameClient()->OnSkinUpdate(name)`，让引用这些贴图的渲染信息重新解析。
- `src/game/client/gameclient.cpp`：`UpdateRenderInfo()` 复用上一份渲染信息时增加 `CSkins::CanReusePreviousSixSkin()` 判定——旧描述符是 6.x、皮肤名有效、但对应皮肤当前不驻留时不再复用，改走 default 皮肤回退；皮肤重新加载完成后 `OnSkinUpdate` 会把真实皮肤换回来。
- 0.7-only 描述符与无效皮肤名不受该判定影响，避免影响六人转译玩家的部件加载流程。

## 验证

- `src/test/skins_test.cpp` 新增 `Skins.UnloadedSkinTexturesInvalidateDependentRenderInfos`：覆盖 `CanReusePreviousSixSkin()` 真值表，并断言两处卸载点都经过统一入口、`OnUpdate()` 末尾发出通知、`UpdateRenderInfo()` 使用新的判定。
- 既有 `Skins.SkinTransitionUsesDefaultKeyWhenInitialDescriptorIsNotReady`、`Skins.SkinTransitionKeepsPreviousSkinBaseWhileDescriptorIsPending`、`Skins.DirectoryScanMergesLocalAndDownloadedSkinsWithLocalPriority` 的分支断言同步为新条件。
- 可复现路径（人工）：把 `cl_skins_loaded_max` 调小或加载大量皮肤触发预算驱逐/目录类型重建，被卸载皮肤的玩家 Tee 应回退成默认皮肤而不是白块，皮肤重新加载后恢复。
- `clang-format --dry-run --Werror src/game/client/components/skins.h src/game/client/components/skins.cpp src/game/client/gameclient.cpp src/test/skins_test.cpp`：退出码 0。
- `git diff --check`（四个文件）：退出码 0，仅提示 `skins_test.cpp` 工作区行尾将在提交时归一化。
- `py -3 qmclient_scripts/gate/check_gate.py --mode quick --report-json-path tmp/blank_skin_white_tee/quick_gate_final.json`：10 项通过、1 项失败；失败项为本次未修改的 `src/engine/client/backend/vulkan/backend_vulkan.cpp:8893` clang-format 问题（与本轮无关，仓库其他文档也记录过同一处）。日志见 `tmp/blank_skin_white_tee/quick_gate_final.log`。
- 按开发期规则本轮未编译客户端、未运行 C++ 测试；编译与测试留到提交前的 `--mode default`。

## 第二轮（3.6.1）：句柄存活查询 + 绘制期兜底

### 复现

含上一轮修复的构建（工作区自编 master，2026-09-15 00:39 链接，PDB 里已有 `UnloadLoadedSkinTextures` / `CanReusePreviousSixSkin`）上同一症状再次出现：计分板 Tee 列与右上角计分框的 Tee 都是**硬边纯白矩形**，世界里该玩家同样是一整块白，且**一直持续到那个玩家换皮肤/离开**。

- 硬边矩形（按行统计白色游程：27 行恒宽 + 29 行恒宽，没有皮肤轮廓的锯齿收边）说明画的是「没有贴图的 quad」，即句柄已失效，而不是「皮肤本来就是白的」。
- 一直不恢复说明上一轮只堵住了 6.x 皮肤 LRU 卸载这一条通知路径，**还有别的路径没送到**。
- 该会话（`%APPDATA%/DDNet/dumps/QmClient_Perf/qm_perf_2026-09-15_20-09-11.log`）里 `evict` 733 次 / 21 分钟：`downloadedskins` 已有 2070 张皮肤，而本地 `skins/` 里是 2560x1280、4096x2048 的 HD 皮肤（一张就记 25MB），默认字节预算 `cl_skins_loaded_max(512) x 64KB = 32MB` 很快被打满，卸载/重载通知被反复触发。

### 修复

- `src/engine/graphics.h`、`src/engine/client/graphics_threaded.{h,cpp}`：把 `IsTextureHandleAllocated()` 提升为 `IGraphics` 的只读接口（`CGraphics_Threaded` 加 `override`，语义不变：纪元 + 槽位代数匹配且槽位仍在使用中）。设备重建或槽位释放后旧句柄在这里判定为失效。
- `src/game/client/render.h`：新增 `CTeeRenderInfo::IsLiveDrawableTextureState()` / `IsLiveDrawableTexture()` 与 `HasStaleTexture()`（覆盖 6.x 两套贴图、0.7 各部件、帽子与机器人贴图）。
- `src/game/client/render.cpp`、`src/game/client/components/players.cpp`：`RenderTee` / `RenderTee6` / `RenderTee7` 内所有皮肤纹理判据改走 `IsDrawableTextureAlive(Graphics(), ...)`，`RenderHand` 同样处理。失效句柄一律当作不可绘制，白块在绘制期不再可能出现。
- `src/game/client/gameclient.cpp`：`CClientData::UpdateRenderInfo()` 复用上一份渲染信息前先判 `HasStaleTexture()`，失效就改走 default 皮肤回退，而不是继续复用死句柄；新增 `RepairStaleTeeRenderInfos()`（`OnUpdate()` 每帧调用）校验托管渲染信息与客户端副本，失效就重新解析并记录 `stale tee render info repaired`（每个渲染信息最多 3 条，避免刷屏）。因此白块最多存在一帧。
- `src/game/client/components/qmclient/qm_skin_outline.h`：轮廓贴图失效时按「需要重建」处理，而不是直接绘制成实心块。

### 验证

- `src/test/skins_test.cpp` 新增 `Skins.StaleTeeHandlesAreRepairedAndNeverDrawn`：覆盖判据真值表、引擎接口声明与实现、`HasStaleTexture()` 实现、三个 Tee 渲染函数里不再出现不查存活状态的判据、轮廓自愈、复用前判据与每帧自愈调用点；既有 `Skins.DefaultFallbackNeverAppliesTheUntexturedPlaceholder`、`Skins.HandRenderingNeverBindsNullOrIncompleteTextureSets` 的分支断言同步为新判据。
- 改动翻译单元做了 MSVC `/Zs` 语法检查（`render.cpp` / `gameclient.cpp` / `players.cpp` / `graphics_threaded.cpp` / `skins_test.cpp` 全部退出码 0，脚本 `tmp/syntax_check_changed_tus.py`）；按开发期规则未编译客户端、未运行 C++ 测试，留到提交前 `--mode default`。
- `clang-format --dry-run --Werror`（上述全部改动文件）：退出码 0。
- `py -3 qmclient_scripts/gate/check_gate.py --mode quick`：11 项通过、0 失败。

### 仍未确定

具体是哪条通知路径漏了仍未定位（上一轮怀疑：`SkinPrefix()` 事件前缀变化后 `OnSkinUpdate()` 的前缀剥离会失配；`CanReusePreviousSixSkin()` 只覆盖 `FLAG_SIX` 分支而 `RenderTee()` 优先走 0.7 分支）。两条都已被本轮的「存活判据 + 每帧自愈」兜住，但根因要等 `stale tee render info repaired` 日志里出现皮肤名与描述符 flags 才能确认。
