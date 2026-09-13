# 游戏内灵动岛背景模糊坐标修复

日期：2026-09-13。版本：3.3.5。

用户反馈其他界面已有高斯模糊，游戏内灵动岛未显示，并已明确批准修改引擎中的模糊绘制函数。

## 原因与实现

`CHud::PrepareMediaIslandBlur` 调用 `CGraphics_Threaded::DualBlurRenderTarget`。该函数降采样、升采样时按目标纹理像素尺寸生成顶点，但 `BeginRenderTarget` 只切换绘制目标和视口，不改变调用方的屏幕映射。两次绘制因此沿用 HUD 坐标，无法铺满目标纹理；HUD 编辑器的缩放和平移也会影响模糊底图。

每次开始重采样后，将映射设为对应纹理的 `(0, 0, width, height)`；绘制结束后立即恢复保存的 HUD 映射，再进入下一个可能失败的阶段。现有模糊参数、刷新频率、透明度合成、全局开关和灵动岛形状保持原行为。

## 回归与验收

在 `src/test/render_target_test.cpp` 补充回归用例，约束两次纹理像素映射的使用顺序，并检查每次绘制结束后、进入可能失败的下一阶段前恢复 HUD 映射。按用户要求先补测试代码再实现，未执行测试，不声称已验证红绿结果。

画面验收标准：开启全局高斯模糊，使用半透明的非原版灵动岛，在有明显纹理的游戏背景上观察模糊；移动、缩放灵动岛后背景仍对应当前位置，文字和图标保持清晰。关闭模糊、完全不透明背景与原版样式仍沿用既有行为。尚未进行客户端画面验收。

## 审查与验证结果

只读审查 findings：本次补丁未发现新的阻断问题。两次 `BeginRenderTarget` 均先提交调用方待绘制顶点，随后才切换映射；每次重采样完成后恢复原映射，后续高斯模糊或目标开始失败时不会遗留临时坐标。两套图形后端消费相同的绘制命令映射，无需修改着色器。

结论：源码修复和回归测试代码已完成，未编译、未运行测试，游戏内视觉效果尚待验收。

- `python qmclient_scripts/gate/check_gate.py --mode quick --report-json-path tmp/island-blur-mapping/gate-report.json`：退出 1，10 项通过、1 项失败。失败来自本次未修改的 `src/engine/client/backend/vulkan/backend_vulkan.cpp:8906:125` 既有格式问题。日志：`tmp/island-blur-mapping/gate.log`。
- `python qmclient_scripts/fix_style.py -n src/engine/client/graphics_threaded.cpp src/test/render_target_test.cpp src/game/version.h`：退出 0，本次修改文件格式检查通过。
- `git diff --check -- src/engine/client/graphics_threaded.cpp src/test/render_target_test.cpp src/game/version.h`：通过。
- 本次修改前快照与独立差异位于 `tmp/island-blur-mapping/before/`、`tmp/island-blur-mapping/task.diff`，与工作区已有其他修改分开记录。
