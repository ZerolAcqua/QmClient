# 统一性能诊断

## 已确认范围

- 保留 `qm_perf_debug` 一个总开关，自动决定采样规则。
- 覆盖客户端采集、日志、分析脚本、HTML 和 JSON 报告。
- 记录 DDNet、QmClient、TClient 的全部运行时配置变量，包括默认值、关闭项、字符串和颜色；不含按键绑定。
- 开始采集时记录全量配置，采集中定期记录变化，关闭时补齐最终配置；密码、Token、密钥和 Cookie 的值脱敏。
- 只修改诊断相关行为及必要调用点；补充测试代码，执行 quick 源码门禁，不编译、不运行测试。

## 已实现

- `qm_perf_debug` 是唯一总开关；另外两个开关和可调阈值已移除，设置页和采集点同步统一。
- 主循环在渲染完成后接入卡顿诊断；关闭、退出或重启先输出剩余窗口和配置，再关闭文件。日志打开失败不会被标记为正在采集。
- 渲染间隔批量写入 `perf/frame`，保留全部有效帧；阶段明细自动使用 300 FPS 预算并限制每秒数量，限流计数写入日志。保留工作区已有的图标窗口汇总，补齐关闭时收尾。
- 开始采集时写入配置清单数量与完整快照，每秒检查一次变化，关闭时补齐最终值。全量包含整数、颜色、字符串、默认值与关闭项，按声明来源区分 DDNet / Qm / Tc，不包含按键绑定。
- 密码、Token、翻译服务 API Key、SecretId/SecretKey、Spotify Cookie 等在客户端写日志前脱敏。长字符串按 UTF-8 边界切分，解析器重组；缺失变量或分段会标记配置不完整。
- 分析器限制驻留样本数量，保留配置初始值和最新完整值；大日志抽样时标记估计结果并跳过严格会话对比。原始日志保留完整采集记录。
- HTML 和 JSON 摘要共享配置数据；HTML 可搜索配置名和值，显示开始时/当前值及修改状态，字符串经 HTML 和脚本嵌入转义。
- 修正 CLI 单独指定日志路径时忽略首个参数的问题；新增 `--no-compare`，默认日志路径覆盖 Windows、Linux、macOS。
- QmClient 版本更新至 `3.6.0`。完整操作说明见 `qmclient_scripts/perf/README.md`。

## 只读审查

Findings：没有剩余的本次核心逻辑修改项。审查中已修复日志打开失败状态、翻译服务密钥脱敏覆盖，以及配置清单/分段不完整的识别；确认报告帧指标优先使用渲染间隔，组件阶段只用于归因。

结论：本次源码改动及配套测试代码已完成。没有进行编译、执行测试、启动客户端或测量实际帧率收益。

## 验证证据（2026-09-14）

- 已补充 C++ 测试：单开关和自动阈值、明细限流计数、配置类型和敏感值、长 UTF-8 字符串、帧批次边界/尾部和会话 ID。
- 已补充 TypeScript 测试：完整帧与组件耗时分离、配置初始值/最新值及缺失识别、HTML 转义和脱敏、有界采集器。
- `python qmclient_scripts/fix_style.py -n <本次涉及的 15 个 C++ 文件>`：退出码 0。日志：`tmp/perf_integration/scope_format.log`。
- `git diff --check`：退出码 0。日志：`tmp/perf_integration/diff_check.log`；Git 提示部分工作区文件未来 checkout 时换行转换，未报告空白错误。
- `python qmclient_scripts/gate/check_gate.py --mode quick --report-json-path tmp/perf_integration/quick_gate_final.json --scope-report-path tmp/perf_integration/quick_scope_final.json`：10 项通过、0 警告、1 项失败。日志：`tmp/perf_integration/quick_gate_final.log`。剩余格式错误来自本次未修改的 `backend_vulkan.cpp` 和 `settings_runtime_cache.cpp`。
- 翻译流程已执行 `extract_strings.py` → `generate_all.py` → `validate.py --incremental` → `review_duplicate_entries.py --show-groups 0 --show-unused 0`。提取、生成和重复项审查退出码 0；全库校验退出码 1，原因是并行工作区的 WebSocket/名称牌等 11 项缺译和 `qmclient.toml` 的 7 处条目间空行格式问题，本次复用已有诊断翻译键。日志：`tmp/perf_integration/i18n_*.log`。

## 数据边界

配置每秒检查一次，短于检查间隔且已恢复的瞬时变化可能不被记录。大日志报告保留初始值和最新完整值，完整变化见原日志；抽样后的统计不作为严格通过证据。旧版日志继续解析，但缺少新配置快照或完整渲染间隔时会使用对应的历史统计口径。
