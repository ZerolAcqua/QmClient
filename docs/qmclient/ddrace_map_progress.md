# DDRace 地图进度估算

## 已确认的范围

- 地图进度条扩展到 DDRace；覆盖优先，允许粗略估算。
- 显示当前位置进度：被队友 drag 向前时上涨，回头接人时下降。
- 沿用现有进度条配置、HUD 样式和 Gores / `NUT_race9` 算法；DDRace 使用独立的客户端估算器。
- 普通冻结、深冻是可能需要协作通过的通路。选路代价与进度长度分开，移动一格记一格长度，传送不按空间跨度计长度。
- 计时 CP 与传送 CP 分开处理。连续的计时 CP 编号只作为候选顺序；每段禁止跨越其他计时 CP，前后锚点无法连接时回退到全图估算。各段等权，段内按路线长度插值。
- 普通传送按编号连接。CP 回传使用角色当前记录的传送 CP，按服务端规则查找不大于该编号的最近 CTO；找不到时回出生点。主号和分身状态独立。
- 非连续编号、没有计时 CP、途中启用或恢复角色但未经过计时 CP 时，可以使用全图估算。
- 估算不模拟队友配合、武器、速度、调参和机关的完整状态；这些地图仍可能出现偏差。

## 实施顺序

1. 在已登记的 `src/test/qm_modes_test.cpp` 中补全路线语义测试，不新增构建入口。
2. 在 `src/game/client/components/qmclient/map_progress.h` 实现可测试的地图、距离场和角色估算状态。
3. 接入 `tclient` 的地图生命周期、分帧更新、主号/分身和调试路线。
4. 按 MMP 更新功能版本；执行 quick 源码卫生门禁，进行只读代码审查。

用户未要求编译或执行测试；本次只补测试代码，不将测试视为已经通过。

## 已实现的行为

- DDRace 使用游戏层、前景实体层和传送层建图，不从装饰图片名称推断水区。
- 普通移动选路代价为 100，普通冻结、深冻和活冻为 300；这些值只影响选路。显示长度为每次相邻格移动 1、传送 0。死亡格、实体墙和不可钩墙不可通行，静态单向阻挡保留。
- 每个角色维护全图和当前分段两个距离场，不为所有 CP 同时分配全图。实体扫描、索引建立、队列推进和传送入口展开均分批处理。
- 计时 CP 使用实体中的连续编号作为候选，每段排除其他计时 CP；连接失败时退回全图。各段等权，不声称代表耗时或难度。
- 回头离开计时 CP 时，根据目标距离的变化切回上一段。主号和分身分别记录方向、来源锚点和当前传送 CP。
- 优先读取扩展角色快照中的传送 CP；旧服务端缺少该数据时跟踪实际经过的传送 CP 格。正常短距离移动补读跨过的薄计时 CP，传送端点和大跨度位移不作沿途触碰推断。
- 途中启用、角色重新出现但尚未触碰起点或计时 CP 时，使用地图中可达起点作全图归一化基准。未完成构建或找不到可达基准时显示未知。
- 调试路线跟随实际采用的距离场；分段估算时显示到下一计时 CP 的路线。换图、角色消失和关闭功能时清理对应状态。
- 同编号多个出口按最短可达路线估算，不模拟随机出口概率；当前传送 CP 改变后重新计算，未触发的后续 CP 不做完整状态枚举。
- 版本从编辑前工作区最新的 `3.5.3` 更新为 `3.6.0`。

## 验证记录（2026-09-14）

- 已先补齐 11 组测试代码：必经 drag 水道与回退、避水绕路、深冻与死亡区别、普通传送、CP 编号向下回退、无 CTO 时回出生点、计时 CP 分段及回头、缺号/逆序 CP 降级、途中开启、主号/分身隔离、单向阻挡与无起终点。未编译、未执行测试。
- `python qmclient_scripts/gate/check_gate.py --mode quick --report-json-path tmp/qm_ddrace_progress/gate.json --scope-report-path tmp/qm_ddrace_progress/gate_scope.json`：退出码 1，10 项通过、1 项失败。失败为范围外的 `src/engine/client/backend/vulkan/backend_vulkan.cpp`、`src/game/client/components/menus.cpp`、`src/game/client/components/settings_runtime_cache.cpp` 格式问题。完整日志为 `tmp/qm_ddrace_progress/gate.log`。
- `python qmclient_scripts/fix_style.py -n src/game/client/components/qmclient/map_progress.h src/game/client/components/tclient/tclient.cpp src/game/client/components/tclient/tclient.h src/test/qm_modes_test.cpp src/game/version.h`：退出码 0，本次涉及的 5 个 C/C++ 文件通过格式检查。日志为 `tmp/qm_ddrace_progress/style.log`。
- 保留了本次既有源码文件的 UTF-8 无 BOM 和 LF 换行；未修改构建入口、协议和服务端逻辑。

### 只读审查

- Findings：本次范围内未发现阻断问题。核对了反向传送边、CTO 编号回退、长度与选路代价分离、CP 回头换段、无可达锚点降级以及角色/地图重置。
- 结论：功能实现和测试代码已补齐；尚无运行时验证或地图覆盖率统计，复杂协作与机关地图的显示精度仍需手动验收。

### 后续手动验收

- 在封闭 drag 水道中观察进度，确认入水后不会因为惩罚权重出现大幅跃升，回头接人时下降。
- 在 `Tutorial`、`Tsunami`、`Kobra 4` 等地图分别检查计时 CP 前进、跨 CP 回头和 CP 回传；分别操控主号、分身。
- 中途启用功能、死亡重生、重新连接和切换地图，确认不沿用旧图或旧角色的状态。
- 对没有计时 CP、编号不连续和候选顺序不可达的地图检查全图降级；确认 Gores 和 `NUT_race9` 行为保持原样。
