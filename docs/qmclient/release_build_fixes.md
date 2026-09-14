# Release 编译修复记录（2026-09-14）

范围：修复当前工作区 Release 客户端的编译错误，不调整功能行为。

- `qm_title_render.cpp` 引入枚举定义所在的 `qmclient_utils.h`。
- `qmclient.h` 补全已有 `ApplyQmNewsPayload` 实现的成员声明。
- `menus.cpp` 的颜色过渡改为 RGBA 各通道插值，避免实例化不支持颜色减法的 `mix` 模板。

验证：

- `cmd /c qmclient_scripts\cmake-windows.cmd --build cmake-build-release --config Release --target game-client -j 14`：最终增量构建成功，退出码 0，产物 `cmake-build-release/DDNet.exe`。
- `python qmclient_scripts/gate/check_gate.py --mode quick`：10 项通过，1 项格式检查失败；本次修改的格式问题已修正，工作区另有 `src/engine/client/backend/vulkan/backend_vulkan.cpp:8893` 格式问题未纳入本次修复。
- 修正后对本次三个源码文件执行 `python qmclient_scripts/fix_style.py -n`：退出码 0。
- 只读审查：未发现本次修复引入的行为变更；未运行单元测试或启动客户端。

## 提交前补齐（2026-09-15）

提交前 `--mode default` 暴露两处编译阻塞与一次格式问题，已一并修复：

- `src/test/websocket_client_test.cpp`：`QmWebSocketLive` 的 `#if !defined(CONF_WEBSOCKETS)` 分支缺少 `#endif`。客户端传输已由 Rust 承载，与服务端 libwebsockets 开关无关，改为只用 `QM_WS_URL` 环境变量决定是否联网执行。
- `src/test/qm_modes_test.cpp`：`QmNameplateNameScope` 两个用例仍按旧的「自身范围 + 他人范围 + 好友过滤」七参数接口书写，枚举 `QM_NAMEPLATE_OWN_SCOPE_*` / `QM_NAMEPLATE_OTHERS_SCOPE_*` 在实现中不存在（`modes.h` 已改为六档互斥的 `qm_nameplate_show_scope`，并按注释明确不再按好友过滤）。用例改写为对三参数 `ShouldShowQmNameplateName` 的六档覆盖，不新增行为。
- `src/engine/client/backend/vulkan/backend_vulkan.cpp`：`fix_style.py` 修正换行缩进。

复跑 `python qmclient_scripts/gate/check_gate.py --mode default`：12 项通过、0 警告、1 项失败（`CMake run_cxx_tests`，57 项用例失败，约 30 项为源码快照断言过期、其余为行为/数值不一致，含 shimmer 因子越界等真实缺陷）。该项未在本轮修复，留待单独任务处理。
