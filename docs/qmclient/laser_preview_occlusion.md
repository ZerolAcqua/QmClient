# 激光预览的武器遮挡

2026-09-17 用户确认：仅修复 HUD → 激光页面中激光枪、霰弹枪预览的遮挡顺序，保留位置、颜色、光效以及其余三行预览。

`CMenus::DoLaserPreview` 原先先绘制武器、再绘制激光，导致光束覆盖枪身。现在先调用 `RenderLaser`，再绘制武器贴图，由贴图遮住重叠部分。这与仓库现有 `ddnet/master` 中 `m_Items` 先于 `m_Players` 的游戏绘制层级一致。

先纠正现有回归测试 `QmMonitoringHelpers.LaserPreviewDrawsWeaponBodiesAfterPreviewLaser`，要求两种武器都在光束之后绘制，再调整实现。版本从工作区的 `3.7.1` 递增至 `3.7.2`。

只读审查未发现本次改动引入的问题：武器分支的 `QuadsBegin` 已重置颜色、旋转和 UV，不会继承激光碰撞头的渲染状态。实体激光与角色分支保持原样。

按开发期约定未编译、未运行测试，未启动客户端做视觉验收。本次三个代码文件的 `git diff --check` 通过。

`python qmclient_scripts/gate/check_gate.py --mode quick --report-json-path tmp/laser-preview-quick-gate.json`：10 项通过、1 项失败。失败来自本次未修改的 `src/game/client/components/qmclient/friends_category_drag.h:79–81` 的 clang-format 格式问题，整体门禁未通过。
