# 服务器浏览器「栖梦」工具箱页签移除（2026-09-18）

## 确认范围

用户反馈服务器浏览器右侧工具箱的「栖梦」页签（唯一内容是「栖梦客户端在线分布」），确认按「移除整个页签」处理：工具箱回到过滤器 / 信息 / 好友三个页签，Tab 热键的页签循环也随之缩短。

不在本次范围：在线分布的数据链。`users` 名单仍由 WS 下发并按原规则解析，继续驱动同服 Q1menG 同步标记与语音支持标记；中心服务、WS 协议字段与下发策略均未改动。

## 改动

- `src/game/client/components/menus_browser.cpp`
  - 删除 `UI_TOOLBOX_PAGE_QM` 与 `CMenus::RenderServerbrowserQm`，以及只被该面板使用的两个静态查找函数 `FindSortedServerByAddress` / `FindServerByAddress`（留着会变成未使用静态函数）。
  - `RenderServerbrowserTabBar` 改为三个等宽页签，新 UI 胶囊槽位与旧 UI 分段底色两条分支同步；`NUM_UI_TOOLBOX_PAGES` 由 4 变 3。
  - `RenderServerbrowserToolBox` 的页签分发去掉 QM 分支。
- `src/game/client/components/menus.h`：删除 `RenderServerbrowserQm` 声明与 `SMALL_TAB_BROWSER_QM` 动画槽位。
- `src/engine/shared/config_variables.h`：`ui_toolbox_page` 上限 `3` → `2`。旧配置里保存的 `3` 在加载时经 `SIntConfigVariable::CommandCallback` 钳制为 `2`，无需额外迁移或清理。
- i18n：移除 8 条只被该面板使用的 source key（`Qm`、`QmClient online distribution`、`No active QmClient reports yet`、`Set a voice server to enable QmClient distribution`、`%d servers, %d users`、`%d servers, %d users, %d dummies`、`Syncing... Showing last received data`、`Not in the current browser list`），并重新生成 12 种语言产物。
- 测试：`qm_new_ui_menu_branch_test.cpp` 的工具箱胶囊测试注释改为三个页签；`qmclient_monitoring_test.cpp` 删除已不存在函数的提取与断言。
- 版本：`3.8.3` → `3.8.4`。

## 保留项

- `m_QmClientDistribution`、`ParseQmClientUsersJson`、同服识别与语音标记、WS 下发策略不变；分布数据抓到后仍在使用，只是不再有侧栏展示入口。
- `docs/qmclient/ws_distribution_flicker_fix.md` 是 2026-09-17 的当时记录，未回改；其中「`menus_browser.cpp` 显示同步提示」现在只对应历史状态。

## 验证记录

- `python qmclient_scripts/gate/check_gate.py --mode quick --report-json-path tmp/qm-toolbox-removal/gate.json`：PASS，11 通过、0 警告、0 失败。
- i18n 流程：`extract_strings.py`（增量）→ `generate_all.py` → `validate.py` → `review_duplicate_entries.py`。提取后上述 8 条 key 消失，运行时产物覆盖 5402 个 source key；重复 key 0、空译文 0、疑似未使用 0。`validate.py` 返回 1，失败项是既有表情功能的 9 条缺译与 `menus_qmclient.cpp` 的 3 个中文 source key，均不在本次范围。
- `bump_version.py --version 3.8.4`：首次写回 `version.h` 报 Windows `OSError [Errno 22]`（与 2026-09-17 记录过的同类写入错误一致），重试成功；文件保持 LF、无 BOM，仅一行变更。
- 未编译、未运行 C++ 测试、未做更新后客户端的界面端到端确认：按开发期工作流本次只跑 quick 源码卫生门禁。界面效果需后续生成新客户端后确认。

## 证据

- 门禁报告与日志：`tmp/qm-toolbox-removal/gate.json`、`tmp/qm-toolbox-removal/gate.log`。
- i18n 校验与重复审查输出：`tmp/qm-toolbox-removal/i18n_validate.txt`、`tmp/qm-toolbox-removal/i18n_review.txt`。
- 一次性 i18n 维护源裁剪脚本：`tmp/qm-toolbox-removal/prune_i18n_keys.py`。
