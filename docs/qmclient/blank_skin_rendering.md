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
