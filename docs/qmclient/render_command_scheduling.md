# 2D 绘制命令与 CPU/GPU 调度（3.5.3）

## 已确认范围

2026-09-14：用户确认以减少卡顿、稳定帧时间并兼顾输入延迟为目标，允许更高占用，并要求持续推进到完成。

本轮只处理相邻普通 2D 绘制批次和 Vulkan 命令录制任务分配。保持顶点、透明叠加顺序、动画刷新率、输入采样、物理、预测和 GPU 排队帧数。主任务负责设计与实现；子代理仅定位配置、测试和计数来源。先补测试代码，不编译、不运行测试，只运行 quick 源码门禁。

## 实现约束

- 合并仅发生在当前尚未提交的 `CCommandBuffer` 尾部。只接受相同普通绘制命令、图元类型和完整渲染状态，且顶点存储连续、合计不超过既有 `MAX_VERTICES`。资源操作、渲染目标切换、读回、清屏及其它命令均隔断合并。
- 保留原先每次 Flush 的顶点复制与 KeepVertices 行为。只延长已有尾命令，不额外复制顶点，不跨命令缓冲合并；渲染调用计数按合并后的命令记录。
- Vulkan 使用已有绘制次数估计分配连续的工作段，保留按工作线程顺序、最后主渲染线程的 GPU 执行顺序。单条较重命令不拆分。
- 初始策略为每个工作线程至少预计 64 次绘制，少于两个工作段时由主渲染线程录制；这是待实机比较的粒度起点，不是已测得的最佳阈值。保留原线程配置上限和 AMD 单线程兼容路径。
- 帧内一旦进入主渲染线程收尾，后续命令缓冲也不重新分配到较早的工作线程，直到原有帧提交边界重置；外部渲染通道仍沿用现有强制单线程路径。

## 回归与验收

测试覆盖相邻顶点与颜色顺序、状态差异、命令屏障、存储间隙、图元/容量边界和缓冲复用；调度覆盖轻负载、加权分配、单条重命令、估计偏差、帧内收尾和下一帧恢复。

最终执行 quick 源码卫生门禁和只读代码审查。性能收益需要同地图、人数、设置和后端的实机帧时间对比；本轮不以静态检查替代性能证据。

参考：[Khronos 命令缓冲与多线程录制示例](https://docs.vulkan.org/samples/latest/samples/performance/command_buffer_usage/README.html)，用于工作段粒度与负载平衡原则。实现按本仓库现有缓冲生命周期和提交顺序设计。

## 实现结果

- `src/engine/client/graphics_threaded.h` 增加尾批次合并，并接入普通顶点 Flush。普通 Quad、Triangle、Line 仅在符合上述约束时合并，Tex3D 和专用 SDF/MSDF 命令保持各自路径。合并后不重复增加命令数和预计绘制次数。
- `src/engine/client/backend/vulkan/backend_vulkan.h` 的调度逻辑按已有绘制次数估计选择连续工作段；实现不依赖 Vulkan 设备，回归用例可以直接覆盖调度行为。
- `src/engine/client/backend/vulkan/backend_vulkan.cpp` 接入调度，单条重命令跨过多个工作段时，唤醒实际持有前一段命令的线程。分组 Quad 的后端预计绘制次数改为 1，与前端和实际绘制一致。
- 复用既有 `src/test/render_target_test.cpp`，新增 8 个批次测试和 8 个调度测试，无需变更构建清单。先写测试后实现，依用户约束未取得红/绿运行证据。
- 使用 `python qmclient_scripts/bump_version.py --version 3.5.3` 更新当前工作区的版本定义。

## 只读审查

Findings：未发现本轮改动的遗留问题。

核对了合并状态字段、对齐间隙、尾命令屏障、缓冲切换与顶点复制顺序；Vulkan 工作线程索引保持单调，主线程尾段直到既有帧边界才复位。轻负载仍使用现有主线程二级命令缓冲，未切换渲染通道类型。资源同步、截图/读回、录制、AMD 单线程路径和 GPU 帧队列沿用既有逻辑。

结论：本轮命令合并与调度实现已完成，静态审查通过；画面等价和帧时间收益仍需实机验证。

## 验证结果

- `python qmclient_scripts/gate/check_gate.py --mode quick`：FAIL，10 项通过、0 警告、1 项失败；失败项是代码格式干跑检查。日志为 `tmp/perf_render_scheduling/quick_gate.log`。
- 门禁报告的格式问题位于既有的 `chat.cpp`、`chat.h`、`menus.cpp`、`player_points.cpp`、`music_lyrics_integration.cpp`、`perf_diagnostics.h`，以及 `backend_vulkan.cpp` 的 `CanCapture` 表达式换行。Vulkan 该行已与本轮修改前副本核对一致；其它文件不属于本轮改动，未为门禁改写并行任务内容。
- 本轮五个代码文件的 `git diff -U0` 经 `clang-format-diff -p1` 检查无格式修正输出；新增实现及测试头文件的全文件 clang-format 检查通过。记录为 `tmp/perf_render_scheduling/changed_lines_format.log`。
- `git diff --check`：通过。本轮修改文件的 BOM 和 LF 换行保持不变。
- 未编译、未运行测试、未启动客户端或进行 GPU/帧时间采样。没有本轮性能提升百分比或 1% low 改善的实测结论。
