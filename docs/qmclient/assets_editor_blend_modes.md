# 资源编辑器部件混合模式

## 已确认的范围

- 只扩展现有部件调色：选择颜色，再选择混合模式；素材拖放仍用于替换部件。
- 正常、滤色、叠加改用标准算法，移除原有同名自定义染色效果。
- 每个部件独立保存颜色、混合模式和 0–100% 强度，设置只保留在当前编辑会话中。
- 预览与导出复用同一 CPU 像素处理路径；沿用现有 PNG 和皮肤部件导出方式。

## 调研与算法依据

- [Adobe 混合模式说明](https://helpx.adobe.com/photoshop/desktop/repair-retouch/adjust-light-tone/blending-mode-descriptions.html)：效果分类与名称。
- [W3C Compositing and Blending Level 1](https://www.w3.org/TR/compositing-1/#blending)：以下 16 种模式的公开公式。

| 类别 | 模式 |
| --- | --- |
| 基础 | 正常 |
| 变暗 | 变暗、正片叠底、颜色加深 |
| 变亮 | 变亮、滤色、颜色减淡 |
| 对比 | 叠加、柔光、强光 |
| 差值 | 差值、排除 |
| 颜色分量 | 色相、饱和度、颜色、明度 |

RGB 使用现有 8 位 PNG 通道值归一化到 0–1 后计算。逐通道模式按照公开公式运算；颜色分量模式使用 W3C 的 Lum、SetLum、SetSat 和 ClipColor，保留明度并处理超出色域的颜色。

强度 t 的结果为 `原色 + (混合色 - 原色) × t`；保留源像素 Alpha，全透明像素保持原始数据。正常模式在 100% 强度使用所选颜色。滤色和叠加保留纯白高光；白色混合色在滤色模式下会提亮，不作为全模式的“未调色”标记。

## 操作与状态

点击右侧部件打开取色器，同时选择该部件。关闭取色器后选择高亮保持，顶部模式与强度只编辑该部件。未选择部件时显示选择提示。拖入素材保留目标部件的调色设置；右键重置和“重置全部”恢复白色、正片叠底、100% 强度。切换主素材或资源类别重建部件并清除选择。

混合模式下拉列表按上述效果类别排列。弹层打开或关闭的当前帧不响应下方画布操作；Esc 优先关闭弹层。

## 验收与验证

测试代码覆盖 16 种模式的固定参考颜色、强度插值、透明像素与矩形边界、减淡与加深端点、柔光分段、颜色分量裁剪后明度和部件设置独立性。

2026-09-14 完成代码与测试补充，工作区版本为 `3.6.0`。先补充表达标准行为的测试，再修改实现；按本次工作规则未编译、未运行 C++ / Rust 测试，也未启动客户端进行界面验证。

只读审查发现下拉列表点击可能穿透画布，已在本次修改中隔离弹层输入；未发现本次改动剩余的可定位问题。

| 检查 | 结果 |
| --- | --- |
| `python qmclient_scripts/gate/check_gate.py --mode quick --report-json-path tmp/assets_editor_blend_gate.json --scope-report-path tmp/assets_editor_blend_scope.json` | 10 项通过、1 项失败。失败位于本次未修改的 `src/engine/client/backend/vulkan/backend_vulkan.cpp:8893`，为 clang-format 格式问题；整体未通过。 |
| 本次修改范围的 `clang-format --dry-run --Werror` 与 `git diff --check` | 通过。 |
| `python qmclient_scripts/languages_qmclient/extract_strings.py` | 完成提取，阻断级字符串违规为 0。 |
| `python qmclient_scripts/languages_qmclient/generate_all.py` | 完成 12 种语言文件生成；新增 15 个条目逐语言检查无缺失。 |
| `python qmclient_scripts/languages_qmclient/validate.py` | 未通过。并行修改期间提取缓存过期；WebSocket、铭牌功能相关 11 个条目缺译；本次未修改的 `translations/i18n/qmclient.toml` 有 7 处条目间空行格式问题。 |
| `python qmclient_scripts/languages_qmclient/review_duplicate_entries.py --show-groups 0 --show-unused 0` | 执行成功，重复键为 0、空译文为 0；相似键与相同译文仍有审阅提示。 |

完整日志保存在 `tmp/assets_editor_blend_gate.log`、`tmp/assets_editor_blend_generate.log`、`tmp/assets_editor_blend_validate.log` 和 `tmp/assets_editor_blend_duplicates.log`。上述仓库级检查失败未扩展到本功能之外处理。
