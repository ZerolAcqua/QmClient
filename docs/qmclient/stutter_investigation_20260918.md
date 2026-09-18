# 卡顿根因排查（2026-09-18）

接续 `client_runtime_performance.md`、`ui_motion_performance.md` 的客户端性能调查路径。
本轮证据为**用户反馈录屏** `我平常就是这么卡呀.mp4`（1920x1080@60，60.19s，非本机录像）
与本机 `QmClient_Perf` 日志。只做只读分析，未编译、未启动客户端、未跑测试。

## 结论摘要

1. 录屏中"画面冻住 50~233ms"发生时，**客户端的渲染循环没有停**：录屏里客户端自己显示
   1210 / 1217 / 1274 / 1310 / 1575 FPS，卡顿前后逐帧读数不变，也没有出现掉帧后必然出现的
   低位读数。因此这类卡顿发生在客户端之后（呈现 → DWM 合成/扫描输出 → 显示器）。
2. 该录屏处于"**未限帧**"状态。这与本机当前配置（自 2026-09-14 23:28 起 `gfx_refresh_rate 404`）
   无关：录屏是用户反馈，不代表本机设置；本机配置时间线仅用于说明"限帧后症状会变"。
3. 当前配置下客户端**自身**的长帧仍然存在且可量化：两小时内 656 帧 >16.67ms、112 帧 >100ms、
   22 帧 >200ms。这部分是客户端可优化的目标。
4. 发现一条与"显示卡顿"高度相关、且纯属客户端行为的路径：`gfx_refresh_rate != 0` 且
   `cl_refresh_rate == 0` 时，主循环没有 sleep 分支，被门控掉的帧是**空转等待**；叠加
   `qm_process_high_priority 1`（HIGH_PRIORITY_CLASS），进程会持续与 DWM 争抢 CPU。

## 证据一：录屏里客户端的帧率从未下跌

判定方法：HUD/状态栏的 FPS 都是 `1/FrameTimeAverage`，且 `FrameTimeAverage` 是**按帧**更新的
EWMA（`m = m*0.9 + dt*0.1`，`client.cpp:4614`、`statusbar.cpp:201`）。一次 200ms 长帧会让之后
约 20 个渲染帧显示 ~48 FPS（两位数），随后逐步回升；60fps 录屏必然拍到这些帧。

逐帧量取（对录屏左上角读数区域做亮度点阵统计，位数变化会同时改变文本宽度与亮像素数）：

| 卡顿区间 | 逐帧观测 |
| --- | --- |
| 42.750–42.950s（200ms 冻结） | 42.90~42.97 连续 5 帧完全相同；恢复后仍是 4 位数（宽度 67→63→67px，亮像素 121~123），全程未出现低位读数 |
| 9.083–9.317s（233ms 冻结） | 冻结期间与恢复后均为 `1575 FPS`（4 位），无下跌 |

全片 60s 内共 21 段 ≥50ms 的完全静止画面（12 段 ≥100ms，最长 233ms），时长都是 16.7ms 的整数倍。

## 证据二：呈现路径的代码事实

- `gfx_vsync 0` → Vulkan 后端优先选 `VK_PRESENT_MODE_IMMEDIATE_KHR`
  （`backend_vulkan.cpp:5031-5039`、`5204-5206`、`5246`）。IMMEDIATE 意味着"立刻呈现、允许撕裂、
  不排队、不等待显示器"，`vkQueuePresentKHR` 不会替调用者节流。
- `gfx_fullscreen 3` = windowed fullscreen（`config_variables.h:453`），画面经 DWM 合成后再输出，
  `compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR`。
- 显示器 144Hz（`gfx_screen_refresh_rate 144`），未限帧时客户端每秒投递 1200~1600 帧，
  即每个刷新周期 8~11 帧。

结论：未限帧 + 关垂直同步时，"用户看到哪一帧"完全由合成/扫描输出路径决定；该路径一旦连续
错过若干刷新周期，画面就停住一段，而客户端自己的计时器完全察觉不到——这正是证据一的现象。

## 证据三：客户端自身的长帧（当前配置）

来源 `qm_perf_2026-09-17_20-53-12.log`（2 小时对局，274 万帧样本，`gfx_refresh_rate 404`）：

| 指标 | 数值 |
| --- | --- |
| 帧间隔 | P50 2.47ms / P95 4.45ms / P99 6.27ms / P99.9 10.58ms / max 1722ms |
| >16.67ms | 656 帧（0.024%） |
| >100ms | 112 帧 |
| >200ms | 22 帧 |

对局内（context=online/page=game，418 个卡顿窗口、168875 帧）组件 CPU 排行的前三：
`trails/on_render` **1.013 ms/帧**、`players/on_render` 0.624 ms/帧、`tclient/on_render` 0.174 ms/帧；
该类窗口内组件合计 2.354 ms/帧——在 404 上限（2.475ms 预算）下已接近满预算。

单帧尖峰（多组件同帧一起膨胀，指向被抢占/IO 等待而非单一组件自身）：

| 组件 | 单帧最大 | 帧号 | 时间 |
| --- | --- | --- | --- |
| `monitoring/on_render` | 99.82ms | 145640073 | 22:09:05 |
| `nameplates/on_update` | 95.92ms | 73978361 | 21:23:10 |
| `tclient/on_update` | 91.47ms | 145640073 | 22:09:05 |
| `translate/on_update` | 61.99ms | 152576894 | — |
| `skins/on_update` | 40.51ms（p95 窗口峰值 11.8ms） | 301762 | 20:53:35 |

主线程阶段：`update_pump_network` 1720.96ms（换图/加载，state=2）、`component_menus` 1366.81ms
（设置页 general/theme_selection）、`update_gameclient` 94.57ms、`update_serverbrowser` 142.87ms、
`input_update` 99.61ms、`sdl_poll_events` 83.58ms。

## 证据四：诊断自身常开的开销

`qm_perf_debug 1` 一直开在正常游玩配置里。日志规模与写入速率：

- `QmClient_Perf` 目录现有 **67 份日志、合计 26.45GB**；最大单份 12.98GB。
- 9/18 20:19 的 6 分钟会话：`text_runtime_budget` 139747 条 + `feature_snapshot` 104877 条
  + `settings_text_usage` 49340 条 + `component_sample` 46070 条 ≈ 每秒 990 条日志。
- 2 小时会话约 410 万条事件、1.3GB。

`ShouldLogTextRuntimeBudget()`（`text.cpp:1123-1132`）在"本帧有任何文字容器上传"时即返回 true，
HUD 的一次性文字容器每帧重建，因此该路径在实际游玩中接近**每帧一条**。

## 证据五：主循环没有 sleep 分支（待确认项）

`client.cpp:4560-4570` 只在 `gfx_vsync == 0 && gfx_refresh_rate == 0` 时才启用空闲节流；
`client.cpp:4687-4704` 的 sleep 依次只看 `cl_refresh_rate_inactive`（窗口失活）、
`cl_refresh_rate`、`IdleRenderThrottleRate`。当前配置 `gfx_refresh_rate 404`、
`cl_refresh_rate 0`、`cl_refresh_rate_inactive 0`、空闲节流 0（日志 `idle_render_throttle
rate=0 requested=0 configured=404`），三条都不成立：被 404 门控掉的帧是**空转等待**。
叠加 `qm_process_high_priority 1` → HIGH_PRIORITY_CLASS（`client.cpp:144`），进程会持续与
DWM 争抢 CPU。

## 配置时间线（由每份日志开头的 config 快照重建）

| 会话 | gfx_refresh_rate | gfx_vsync | gfx_fullscreen | 屏幕刷新率 |
| --- | --- | --- | --- | --- |
| ≤ 2026-09-14 23:01 | 无快照（旧版日志） | — | — | — |
| 2026-09-14 23:28 / 23:39 | 404 | 1 | 3 | 144 |
| 2026-09-15 起全部会话（含 09-17、09-18） | 404 | 0 | 3 | 144 |

录屏显示 1200~1600 FPS，与 `gfx_refresh_rate 404` 不可能共存（`client.cpp:4591-4596` 的
`RenderFrameTicks` 门控）；因此该录屏处于未限帧状态。本机自 2026-09-14 23:28 起一直是
`404 / vsync 0 / fullscreen 3 / 144Hz`，两者不是同一设置，本表只用于说明"限帧前后症状不同"。

## 本轮实现（用户已授权三项）

### 1. 呈现对齐：关垂直同步且未设上限时按显示器刷新率门控

`client.cpp` 的空闲节流判定内新增分支：`gfx_vsync == 0 && gfx_refresh_rate == 0` 且空闲节流
无要求时，把 `GfxRefreshRate` 设为 `g_Config.m_GfxScreenRefreshRate`。新增配置
`qm_present_align`（默认 1，可设 0 关闭）。显式 `gfx_refresh_rate`、录像路径
（`IVideo::Current()`）仍优先，不会被覆盖。

不改画面内容、玩法、物理与预测；改变的是投递速率（FPS 读数会随之变化）。

### 2. 主循环：门控掉的帧从空转等待改为等待

`client.cpp` 新增 `RenderGateRate`（仅当 `IsRenderActive && GfxRefreshRate > 0` 时赋值），
在 `else if(IdleRenderThrottleRate > 0)` 之后新增 `else if(RenderGateRate > 0)` 分支，
用与既有分支相同的 `WaitWithNetwork` 与 `LastTime` 时间补偿语义等待到同一时间片。
门控判据（`RenderFrameTicks <= Now - LastRenderTime`）、丢帧补偿（`AdditionalTime`）与渲染时机
均未改动；窗口失活、`cl_refresh_rate`、录像路径行为保持原样。

### 3. 文字热路径：诊断计时改为仅在采集时采样

`text.cpp` 的 glyph 上传 / glyph 光栅化 / 文本容器创建 / 文本容器上传四处高精度时钟与耗时累加
改为 `QmPerfEnabled()` 门控：关闭诊断时不再读时钟，开启时字段与口径完全一致。

## 未完成/待定

- **拖尾（`trails/on_render` 1.013 ms/帧，对局内第一大 CPU 项）本轮未改**。该路径的耗时由
  采样数×层数×每段 3 个四边形与逐四边形 `SetColor4` + `QuadsDrawFreeform` 提交构成，
  在不改几何（不改画面）的前提下需要引擎侧批量提交接口，或减少细分（属于视觉改动）。
  两条都需要单独决策，未纳入本轮。
- 皮肤与铭牌的单帧尖峰（`skins/on_update` p95 11.8ms、`nameplates/on_update` 95.9ms 一次）
  多为多组件同帧膨胀，指向进程被抢占或 IO 等待，需要"关掉 `qm_perf_debug` 之后"的新一轮
  实机数据再判断归属。

## 验证证据（2026-09-18）

- `python qmclient_scripts/gate/check_gate.py --mode quick`：退出码 0，PASS，11 项通过、
  0 警告、0 失败。日志：`tmp/perf_video/quick_gate_stutter.log`。
- 新增源码边界测试（`src/test/qmclient_monitoring_test.cpp`）：
  `RenderGateWaitReplacesBusyWaitWithoutChangingGate`（等待分支位置与语义、门控判据未被改动、
  对齐配置默认值、对齐分支不得覆盖显式上限与录像路径）、
  `TextRuntimePerfSamplingRunsOnlyWhenDiagnosticsAreEnabled`（四处采样门控与字段保留）。
- 未编译、未运行 testrunner、未启动客户端；因此**没有实测收益数据**，本轮结论限于源码可证范围。

## 本轮已执行的动作

- 用户配置 `qm_perf_debug 1 → 0`（用户确认"忘了关"；目录内已累积 67 份日志、26.45GB）。
- 未改显示、玩法、协议、物理、预测；未改 `gfx_refresh_rate` 等既有配置的语义。
