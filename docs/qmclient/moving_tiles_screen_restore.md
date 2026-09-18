# 移动方块绘制后的坐标恢复

## 问题与范围

实体层为 100% 且开启“在实体层显示移动方块”时，`CMovingTiles::OnRender()` 会按地图分组的偏移、视差和缩放修改屏幕映射，但原实现结束时只清除裁剪。角色先于移动方块绘制，名字在其后绘制，因此分组映射与游戏映射不一致时，名字可能偏离角色。

本次只修复绘制状态的恢复，不修改移动方块的识别、动画、分组映射、裁剪规则或游戏行为。

## 实现与回归检查

- `src/game/client/components/tclient/moving_tiles.cpp`：通过开关和空列表检查后保存入口屏幕映射；完成绘制并清除裁剪后，按原顺序恢复四个坐标。前景和背景实例共用此逻辑。
- `src/test/qm_new_ui_menu_branch_test.cpp`：先补充 `QmMovingTiles.RestoresIncomingScreenMappingAfterRendering` 源码回归检查，约束分组处理前保存映射、绘制后恢复相同四个坐标，且收尾不提前返回或再次覆盖映射。它检查源码中的状态恢复约束，不替代实际地图渲染测试。
- `src/game/version.h`：按补丁版本从工作区原有的 `3.9.1` 更新为 `3.9.2`。

## 审查与验证

- 只读审查 findings：未发现本次补丁新增问题。主函数唯一提前返回位于保存映射之前；分组裁剪等跳过路径在局部 lambda 内返回，仍会执行主函数末尾的恢复操作。
- `python qmclient_scripts/gate/check_gate.py --mode quick --report-json-path tmp/moving_tiles_screen_restore/quick_gate.json`：退出码 0，11 项通过，0 警告、0 失败。
- `git diff --check -- src/game/client/components/tclient/moving_tiles.cpp src/test/qm_new_ui_menu_branch_test.cpp src/game/version.h`：退出码 0。
- 按用户要求未编译、未运行测试，未获取测试失败到通过的运行证据；具体地图复现尚未验证。

后续地图验收：在存在分组偏移或视差的地图中，实体层设为 100%，切换移动方块开关并移动、缩放相机；名字应始终跟随角色，移动方块仍按原分组和动画显示。
