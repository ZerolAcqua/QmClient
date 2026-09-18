# 名牌文字特效开销与自适应降级

## 范围

用户报「即使 Vulkan，手动缩小镜头后同屏 90+ 玩家会掉帧」，并追问是否与文字相关。存档配置同时开着坐标名牌、称号样式与双方钩线。

本次只做三件事：把名牌文字特效的每帧绘制次数查清、加一层按同屏名牌数自适应的档位、把这个过程与没动的东西写清楚。不改协议、物理、预测、序列化，不改 `src/engine/`，不降低阈值以内人数时的画质（逐层一致，一个绘制都不少）。

## 结论：是否和文字相关

**是。** 掉帧的主因是 QmClient 自己加的名牌文字特效：同一个字形几何被反复重绘，每次只换 uniform 颜色和 XY 偏移，同屏人一多就线性堆成几万次绘制/帧。

### 单行最坏绘制次数推算

每个带特效的名牌文本行调用一次 `CRenderTools::RenderTextContainerWithEffects`（`src/game/client/render.cpp:167`），内部对同一份 `m_vCharacterQuads` 反复调用 `TextRender()->RenderTextContainer`：

| 层 | 每圈方向数 | 圈数上限 | 最坏绘制次数 | 代码位置 |
| --- | --- | --- | --- | --- |
| 外层辉光 | 4（上下左右） | `clamp(round(GlowRange), 1, 6)` = 6 | 24 | `src/game/client/render.cpp:206`-`217` |
| 描边 | 8（四正 + 四斜） | `clamp(round(BorderRange), 1, 4)` = 4，仅 `BorderRange > 1` 时才有 | 32 | `src/game/client/render.cpp:222`-`233` |
| 本体 | 1 | 1（永远保留） | 1 | `src/game/client/render.cpp:242` |
| 合计 | | | **57** | |

前提是配置把 `qm_nameplate_text_glow_range` 拉到 6 以上、`qm_nameplate_text_border_range` 拉到 4；默认 `4 / 1` 时是 16 + 0 + 1 = 17 次/行。

### 与「90 人同屏」相乘

- 带特效层的只有**昵称行**与**战队行**：全仓库只有 `CNamePlatePartText::Render` 一处调用特效函数（`src/game/client/components/nameplates.cpp:499`），而把 `m_UseTextEffects` 传下去的只有 `CNamePlatePartName` 与 `CNamePlatePartClan`。其余文字行（坐标、ID、好友标记、皮肤名、原因、强钩编号）虽然是文字，但 `m_UseTextEffects = false`，`m_Effects = 0`，每行只有 1 次本体绘制（引擎自带的 1 像素描边在同一次绘制里），量级可以忽略。
- 用户给的口径是只算昵称行：90 名牌 × 1 行 × 57 次 = **5,130 次文字绘制/帧**，按 60 FPS 约 **30.8 万次/秒**——与用户报的问题完全对得上。
- 若同时打开战队行（`cl_nameplates_clan`），特效行翻倍到 **10,260 次/帧**；称号行走另一套函数（见下）。
- 坐标名牌**没有特效层**，但它本身是文字行（每行 1 次本体绘制），所以「坐标名牌」不是非文字开销，只是不含叠加层。

这就是「缩小镜头 → 同屏名牌线性变多 → 帧率暴跌」的完整因果：成本随同屏名牌数线性上升，与地图、物理、网络无关。

### 复核：一次绘制等于几次 GPU draw

用户的推算里有一条需要修正：文本缓冲路径下引擎**不会**为描边额外画一遍。

- Vulkan 与 OpenGL 3.3 起默认开启文本缓冲（`m_GLTextBufferingEnabled = (m_GLQuadContainerBufferingEnabled && m_pBackend->HasTextBuffering())`，`src/engine/client/graphics_threaded.cpp:3745`）。
- 该路径下 `RenderTextContainer` 只发起一次 `Graphics()->RenderText(...)`（`src/engine/client/text.cpp:2556`-`2568`），后端一次 `glDrawElements` 同时画出描边层与填充层（描边/填充两个采样器 + 两个 uniform 颜色）：`src/engine/client/backend/opengl/backend_opengl3.cpp:1627`-`1678`。
- 「描边色非透明时会额外画一遍」只成立于不支持文本缓冲的旧路径（`src/engine/client/text.cpp:2570`-`2612`：描边 `QuadsEndKeepVertices` 一批 + 填充一批 = 2 批）。

所以单行最坏是 **57 次绘制**而不是 114 次；结论不变，量级不变。这一条也决定了本次优化的方向：削减的是同一次 `RenderTextContainer` 调用的**层数**，每层省下的是 1 次 draw + 1 次 uniform 更新。

### 证据位置汇总

- `src/game/client/render.cpp:167` 特效文字入口（本次改这里）
- `src/game/client/render.cpp:245` 称号「抛光」特效、`src/game/client/render.cpp:317` 称号「灾变」特效（本次没改）
- `src/game/client/components/nameplates.cpp:499` 名牌文本行调用点
- `src/game/client/components/nameplate_text_effects.h` 特效参数与本次新增的档位纯函数
- `src/engine/client/text.cpp:2556` `RenderTextContainer`（缓冲/非缓冲两条路径）
- `src/engine/client/backend/opengl/backend_opengl3.cpp:1627` `RenderText` → 单次 `glDrawElements`

### 为什么「阈值以内人数」和「人多」可以分开对待

同屏 10 个名牌时单帧成本约 10 × 17~57 = 170~570 次绘制；用户只在「装下 90+ 玩家」时才报掉帧，说明这个量级本来是能接受的。因此档位设计成：**人数不超过阈值时一个绘制都不少**，超过后按人数把人头预算摊薄。这里没有实机采样，量化收益仍待手动验收。

## 热点分析

### 一、`RenderTextContainerWithEffects`（昵称行与战队行）

所有名牌文字行都会调用它（`src/game/client/components/nameplates.cpp:499`），但只有昵称行与战队行把 `m_UseTextEffects` 传成真值，其余行的 `m_Effects = 0`，不会进入下面任何一层。

逐项：

1. 辉光：`GlowPasses = clamp(round_to_int(Style.m_GlowRange), 1, 6)`，每圈 4 个方向 → `4 × GlowPasses`，最坏 24。第 `Pass` 圈的半径与 alpha 都按圈序衰减（半径递增、alpha 递减），也就是**越靠后的圈半径越大、alpha 越低**——最外圈视觉贡献最小。（本次改动后半径与 alpha 的分母固定为满档圈数，降级时消失的就只有这些最外圈，见《实现》。）
2. 描边：仅当 `BorderRange > 1` 时进入，`BorderPasses = clamp(round_to_int(Style.m_BorderRange), 1, 4)`，每圈 8 个方向 → `8 × BorderPasses`，最坏 32。同样越靠后 alpha 越低，半径越大。
3. 本体：1 次，且永远保留。描边色非透明时引擎还会在同一批次里补画 1 像素描边，不额外增加绘制次数。
4. 每层绘制的都是同一份字形几何，只有颜色与偏移不同，因此**无法合并批次**：层与层之间 uniform 颜色不同、位置不同，顶点缓冲里没有多余自由度。

### 二、称号两套特效

- `RenderTitleContainerWithPolishedEffects`（`src/game/client/render.cpp:245`）：投影 1 + 柔光 2 圈 × 4 方向 × 2 色（同色 + 暗色）= 16 + 高光 1 + 本体 1 = 最坏 **19** 次/行。
- `RenderTitleContainerWithCalamityEffects`（`src/game/client/render.cpp:317`）：8 方向固定半径描边 8 + 加法混合环形辉光 `m_BloomDraws`（配置 6 或 16）= 最多 **25** 次/行。

只有「有称号」的玩家才会画这一行（`m_Visible = Data.m_aQmTitle[0] != '\0'`），所以它的总量取决于服务器上有多少人有称号。

### 三、与特效无关但同样逐帧的文字开销（本次没动）

名牌文本容器在动画、渐变、彩虹、逐字符浮动时每帧重写顶点并重新上传 GPU 缓冲（`RecreateTextContainerSoft`，`src/game/client/components/nameplates.cpp` 的 `CNamePlatePartText::Update` 路径）。这一项与「特效层数」无关，本次未改。

## 实现

### 档位规则

预算按「名牌数」摊分，货币是**一次文字绘制**（一次 `RenderTextContainer` 调用）：

```
满档次数 FullDraws = 4 × clamp(GlowRange,1,6) + (BorderRange > 1 ? 8 × clamp(BorderRange,1,4) : 0)   // 本体不计入
同屏名牌数 N（平滑后）不超过阈值 T  → 不降级（QM_TEXT_EFFECT_DRAWS_UNLIMITED，逐层与改动前一致）
N > T                              → 每行允许次数 = ceil(FullDraws × T / N)
实际圈数 = 先按 8 次/圈配满描边（内圈优先），剩余次数再按 4 次/圈配辉光（内圈优先）
```

- **优先级**：先砍最外层辉光，再砍最外层描边，本体永远保留；`BorderRange = 1` 时引擎自带的 1 像素描边也始终在。
- **每帧特效总量上限**：按人均摊后总量不超过「满档时 T 个名牌」的开销（每个名牌有几个特效行都已算在里面），与用户的特效配置自动同步缩放。
- **阈值 T**：`qm_nameplate_effect_lod_threshold`，默认 20，范围 4~64。
- **平滑与滞回**：档位用的是「平滑后的同屏名牌数」。死区 ±2 个名牌：屏幕边缘单个玩家进出造成的 1~2 个抖动不改变档位（阈值附近 19/21 交替 60 帧，档位保持满档，见测试）。死区之外上升每帧最多 +8（尽快保帧率），下降每帧最多 −2（避免回升时特效闪烁，最坏约 0.7 秒回到满档）。
- **同帧一致**：档位在每帧渲染段开场算一次（`QmNameplateEffectLodBeginFrame()`，`src/game/client/components/nameplates.cpp:2899`），当帧所有名牌共用同一个值，不存在同屏不同玩家效果不一致。
- **人数取样**：用上一帧真正画出特效层的名牌数（昵称/战队行且 `m_ShowName` 为真时才算），因此不需要为了数数再扫一遍全部客户端；代价是最多一帧延迟，且人数变化本身已经被平滑限制。
- **保留内圈一致**：档位只限制循环次数，半径与 alpha 的分母仍取满档圈数，所以降级时消失的确实只有最外圈，留下的内圈与改动前逐层像素一致。
- **布局不变**：名牌尺寸/内边距仍按 `QmNameplateTextEffectPadding()` 的配置值计算，与档位无关，切档不会引起名牌位置或大小跳动。
- **单帧开销**：只多一次整数运算和一个计数器自增，无堆分配、无新增全表扫描。

举例（阈值 20）：

| 配置 | 名牌数 N | 每行允许次数 | 实际圈数（描边 + 辉光） | 本体 |
| --- | --- | --- | --- | --- |
| 默认（辉光 4 圈 = 16） | ≤ 20 | 不限制 | 4 | 1 |
| 默认 | 40 | 8 | 0 + 2 | 1 |
| 默认 | 90 | 4 | 0 + 1 | 1 |
| 辉光 6 圈 + 描边 4 圈 = 56 | ≤ 20 | 不限制 | 4 + 6 | 1 |
| 辉光 6 圈 + 描边 4 圈 | 30 | 38 | 4 + 1 | 1 |
| 辉光 6 圈 + 描边 4 圈 | 90 | 13 | 1 + 1 | 1 |
| 辉光 6 圈 + 描边 4 圈 | 128 | 9 | 1 + 0 | 1 |

按用户那套「辉光 6 圈 + 描边 4 圈」的配置，90 名牌 × 2 行从 10,260 次/帧降到约 2,160 次特效绘制/帧（另加每行 1 次本体绘制），约 4.4 倍削减。

### 新配置项

| 配置 | 默认 | 范围 | 作用 |
| --- | --- | --- | --- |
| `qm_nameplate_effect_auto_lod` | 1（开） | 0~1 | 名牌文字特效自适应降级总开关。关闭后任何人数都保持原样逐层绘制。 |
| `qm_nameplate_effect_lod_threshold` | 20 | 4~64 | 仍然保持满档画质的同屏名牌数阈值；超过后特效层数按人数比例缩减。调大 = 保留更多特效层，调小 = 更激进地省性能。 |

两项都只在 `src/engine/shared/config_variables_qmclient.h` 里加了描述，没有加到设置页 UI（设置页文件本次不在可改范围），因此**本轮没有新增任何 `Localize()` 源串**。

## 手动验收

以下为待执行清单，不代表已通过。按本次约定不编译、不运行测试、不启动客户端。

1. 记录当前配置与显卡；进一个满人服务器，站在人群可见的位置，`cl_zoom` 保持默认，用帧率显示（`cl_showfps 1`）记录基准帧率与画面。
2. 把同屏名牌控制在 20 个以内（收小视野或找空旷位置），确认名牌文字画质与改动前完全一致：辉光圈数、描边圈数、本体、颜色、位置均无差别；把 `qm_nameplate_effect_auto_lod` 设为 0 再对比一次，两者应逐帧一致。
3. 手动缩小镜头（`cl_zoom` 调大或用滚轮拉远）到同屏 90+ 名牌，观察帧率相对第 1 步的改善；同时确认最外层辉光/描边被削掉，而文字本体、引擎自带的 1 像素描边、名牌位置与大小不变。
4. 让同屏名牌数在阈值附近来回抖动（例如在人群边缘前后微调镜头，或让一个人反复进出视野边缘），确认特效**不闪烁**：档位不应逐帧在满档与降级之间跳。
5. 快速拉远、再快速拉回，确认恢复过程是平滑的（下降每帧最多 2 个名牌的跟随速度），且回到阈值以内后恢复满档画质。
6. 把 `qm_nameplate_effect_lod_threshold` 分别设为 4 与 64，重复第 3 步，确认数值越大保留的特效层越多、越省性能的档位对应更小的值。
7. 打开设置页的铭牌预览，确认预览里的特效仍是满档（预览不参与降级）。
8. 检查掉帧是否仍存在：若 90 人同屏仍不达标，对照《未处理的候选》逐项判断剩余开销来源（称号特效、文本容器逐帧重建、钩线、玩家皮肤绘制等）。

## 实现记录

- 改动文件：
  - `src/game/client/components/nameplate_text_effects.h`：新增预算/档位纯函数 `QmNameplateEffectFullDraws()`、`QmNameplateEffectLodSmoothCount()`、`QmNameplateEffectLodIdealDraws()`、`QmNameplateEffectResolvePasses()` 与常量；原有 `QmNameplateTextEffectPadding()` 未改。
  - `src/game/client/render.h`：`SQmTextEffectRenderStyle` 增加 `m_MaxEffectDraws`（默认 `QM_TEXT_EFFECT_DRAWS_UNLIMITED` = 满档），枚举增加该哨兵值。
  - `src/game/client/render.cpp`：`RenderTextContainerWithEffects` 改为按档位限制辉光/描边循环次数，半径与 alpha 分母改用满档圈数；新增 include。
  - `src/game/client/components/nameplates.cpp`：新增帧内档位状态与 `QmNameplateEffectLodBeginFrame()/EndFrame()`；`BuildQmNameplateTextStyle()` 写入 `m_MaxEffectDraws`；`RenderNamePlateGame()` 统计本帧画出的带特效名牌数；`OnRender()` 在名牌渲染段前后包住档位计算。`CNamePlatePartFriendMark::UpdateText()` 与好友爱心相关代码一字未动。
  - `src/engine/shared/config_variables_qmclient.h`：定点新增 `qm_nameplate_effect_auto_lod`、`qm_nameplate_effect_lod_threshold` 两行加注释。
  - `src/test/qm_new_ui_menu_branch_test.cpp`：新增 `NameplateTextEffectAutoLodCutsOuterLayersWhenCrowded`。
  - 本文档。
- 测试：新增用例覆盖满档次数换算、阈值内不降级、按人数摊分（含向上取整与极端人数）、砍层优先级（先辉光后描边、本体保留）、平滑死区与步长、阈值附近 19/21 抖动 60 帧不改变档位，以及渲染侧与配置项的接线断言。
- 只读审查（自查）：先列 findings，再给结论。
  - findings 1（已修）：最初按「`Data.m_UseTextEffects` 为真」计数，但特效层其实只出现在昵称行与战队行（只有 `CNamePlatePartName` / `CNamePlatePartClan` 传 `m_UseTextEffects`）。当玩家的「特效范围」开着而「昵称范围」关着时，屏幕上没有特效行却仍按人数降级，会白白压低本机名牌画质。已改成 `Data.m_UseTextEffects && Data.m_ShowName` 才计数，与该行是否真的画特效层一致。
  - findings 2（接受，不修）：`QmNameplateEffectFullDraws()` 只看配置位掩码，不看颜色 alpha。用户把辉光/描边颜色设成全透明时，算出的满档次数会偏大，档位随之偏保守（保留更多层），方向是「少省一点性能」而不是「多降画质」，不需要修正。
  - findings 3（接受，不修）：人数取样用上一帧的计数，最多一帧延迟。这是为了不额外扫一遍 `MAX_CLIENTS`（扫全表本身被本次约束禁止），且人数变化已被平滑限制，看不见差异。
  - 结论：数值与 `render.cpp` 的圈数上限一致（4/8 次每圈、6/4 圈上限）；档位在帧外被复位为满档，因此设置页预览与其它调用方不受影响；每帧只增加一次整数运算与一次计数器自增，无堆分配、无新增全表扫描；未改动协议/物理/预测/序列化与 `src/engine/`；未改动好友爱心（`CNamePlatePartFriendMark::UpdateText()`）与文本容器更新时机。
- quick 门禁（命令与原始数字见下）：10 项通过、0 项警告、1 项失败。唯一失败是**他人并行改动**的 `src/game/client/components/menus_settings.cpp:3639` clang-format 违规；本轮改动文件全部落在格式检查的批次里，第二次运行时格式检查在第一批就因该文件失败退出，位于后续批次的 `src/test/qm_new_ui_menu_branch_test.cpp` 没有被门禁覆盖，因此单独用同一条格式检查命令补跑，退出码 0。
- 未执行项：不编译、不运行 `testrunner` / `run_cxx_tests` / `run_rust_tests`、不跑 i18n 脚本、不做实机帧率采样。因此**实际帧率收益、跨平台编译、视觉回归均未验证**；新增测试只补了代码，未运行。

### 门禁原始输出

命令（仓库根目录）：

```
python qmclient_scripts/gate/check_gate.py --mode quick
```

日志：`tmp/b3_nameplate_lod_gate.log`；格式补充检查日志：`tmp/b3_nameplate_lod_format.log`。

```
==> 配置变量使用检查（Qm/Tc/栖梦）
  [tclient] 通过：未发现未使用配置项（共 196 个）
  [qmclient] 通过：未发现未使用配置项（共 486 个）
通过：所有配置项均已被使用。

==> 头文件 guard 检查
All header guards are correct.

==> 标准头文件检查
通过：未发现标准 C 头文件误用。

==> 未使用头文件检查
Success: No header files are unused.

==> 代码格式干跑检查
src\game\client\components\menus_settings.cpp:3639:123: error: code should be clang-formatted [-Wclang-format-violations]
                AddCard(OptionsSpec, [TeeMetrics, TeeQueuePresetAvailableHeight, QueueItemCount, QueuePresetCount](float ContentWidth) {

==> ruff format
45 files already formatted

==> ruff check
All checks passed!

==> shellcheck
（无输出）

==> 设置页统一 UI 迁移合同
（24 个页面全部 clean）

==> 检查汇总
[INFO] 模式: quick
[INFO] 通过: 10
[INFO] 警告: 0
[INFO] 失败: 1
[INFO] 跳过: 0
[INFO] 不适用: 0
[INFO] 有效结果: FAIL

失败清单：
[FAIL] 代码格式干跑检查: 代码格式干跑检查 失败，退出码 1
src\game\client\components\menus_settings.cpp:3639:123: error: code should be clang-formatted [-Wclang-format-violations]
```

补充的格式检查（因为上面的批次在他人文件上提前失败）：

```
python qmclient_scripts/fix_style.py -n src/test/qm_new_ui_menu_branch_test.cpp src/game/client/components/nameplate_text_effects.h src/game/client/components/nameplates.cpp src/game/client/render.cpp src/game/client/render.h src/engine/shared/config_variables_qmclient.h
EXIT=0（无输出）
```

## 未处理的候选

| 候选 | 观察到什么 | 本次为什么没动 |
| --- | --- | --- |
| 称号特效（抛光 19 次/行、灾变最多 25 次/行） | 与名牌文字特效同源、同样每帧每行重绘，用户配置里称号样式是开着的 | 两套函数的层次语义不同（加法混合环形 bloom、投影/高光属于观感必需），需要单独设计档位映射，且只有有称号的玩家才画。本次先解决每个玩家都会命中的昵称/坐标行，避免一次改动同时改两套视觉语义。 |
| 名牌文本容器逐帧重建 | 动画/渐变/彩虹名牌每帧重写顶点并重新上传缓冲 | 属于文本容器生命周期而非特效层数，需要单独评估，且与并行任务正在改动的名牌代码相邻。 |
| 双方钩线（`cl_show_hook_coll_own` / `cl_show_hook_coll_other`） | 每个可见玩家每帧一条钩线（线段四边形 + 尖端），是真实开销但**与文字无关** | 属于玩家渲染，不在名牌文字范围；另一任务已在《客户端运行期开销优化》里把逐帧 vector 分配改为复用缓冲。 |
| 坐标名牌本身的行数 | 坐标名牌**是文字行但没有特效层**（`CNamePlatePartCoordinates` 不设 `m_UseTextEffects`），每行 1 次本体绘制 | 本次档位管的是叠加层，坐标行本来就没有叠加层可削；剩下的本体绘制是显示内容的必要成本，只能靠用户少开几行来省。 |
| 真正的批量绘制 | 每层特效都是一次独立 draw + uniform 更新，顶点数据无法跨层合并 | 要彻底解决需要引擎侧的批绘制或 SDF 扩边着色器（改 `src/engine/`），超出本次授权。 |
| 玩家皮肤/装饰绘制 | 90 人同屏时每个 Tee 本体、描边、拖尾、表情也是逐帧绘制 | 与文字完全无关，属于另一类优化。 |
