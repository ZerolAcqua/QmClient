# 弹射表情重写

## 范围与验收

- 保留 Tab 切换、松开轮盘发射、1.5 秒蓄力、超级头顶表情、显示过滤与现有实时消息；不修改服务端。
- 使用当前表情皮肤 alpha 大于零的像素区域，与地图实心格碰撞；透明区域不碰撞，轮廓跟随图片旋转和缩放。
- 表情不影响人物、地图玩法或其他表情。HUD 在飞行表情上方，轮盘仍在 HUD 上方。
- 发射由输入释放提交，使用最新选择；本地效果不等待网络；池满时替换最旧表情。
- 普通与超级弹射保留原速度、重力、反弹系数、寿命、缩放与特效。墙边出生时寻找附近可容纳轮廓的位置。若出生点周围完全没有容纳空间，只播放发射特效；不生成穿墙的实体。淡出膨胀无法容纳时保持最后有效尺寸继续淡出。
- 新增回归测试代码，按项目约定不编译、不运行测试；仅运行 quick 源码门禁。

## 网络边界

继续发送 `emoticon`、`player_id`、`launch_mode`、`super_launch`，接收现有广播格式。远端依据事件到达时的人物位置和方向播放，不承诺双方轨迹一致；能否收到其他人的事件取决于现有服务器是否转发表情消息。

## 指定 ID 发射命令

- F1/控制台输入 `qm_emote <ID>`，立即朝当前瞄准方向发射该 ID 的表情；例如 `qm_emote 2` 发射爱心。可用 `bind x "qm_emote 2"` 绑定按键。没有新增聊天命令。硬改名：原 `shot_emote` 已移除，旧 bind 需改为 `qm_emote`。
- ID 沿用 `datasrc/network.py` 中的 `Emoticons` 顺序，范围为 `0` 至 `NUM_EMOTICONS - 1`（当前为 `0–15`），不做重新映射。
- 两个命令共用 `i[emote-id]` 参数定义和控制台解析器。缺参和未加引号的非法整数沿用现有报错；整数越界由公共 `ResolveEffect()` 静默忽略。引号、额外参数等输入也完整沿用 `emote` 的现有解析语义。
- `src/game/client/components/qmclient/emoticon_commands.h` 集中注册 `emote` 与 `qm_emote`；后者调用 `CEmoticon::Emote(ID, true)`。强制发射仅作用于本次请求，不修改轮盘的发射开关，也不改变现有蓄力状态的消费规则。
- “发射表情按键”仍绑定 `toggle_emote_launcher`，轮盘释放仍调用 `Emote(ID)`。两条路径共用 ID 校验、`SpawnProjectile()`、`CNetMsg_Cl_Emoticon`、分身复制与 Qm 同步发送；同步的 `launch_mode` 使用本次实际效果。
- 在 `src/test/qm_modes_test.cpp` 补充聚焦测试，覆盖控制台注册、全部合法 ID、参数解析、公共入口调用、后续普通表情与越界处理。测试使用实际控制台解析器；不编译或运行测试。
- 命令首次实现时版本为 `3.9.10→3.9.11`，命令名当时为 `shot_emote`。后续硬改名为 `qm_emote`（不保留旧名），版本 `3.10.0→3.10.1`。不新增 UI 卡片或配置项。

### 命令审查与验证

- Findings：只读审查未发现本次改名的阻断问题。行为入口仍为 `Emote(ID, true)`，`emote` 命令与轮盘路径未改；help 文案仍为 `Launch emote`。
- `py -3 qmclient_scripts/gate/check_gate.py --mode quick --report-json-path tmp/qm_emote_rename_gate.json`：10 通过 / 0 警告 / 1 失败。失败项为仓库内未改动的 `src/game/client/components/hud.cpp` 格式问题，与本次改名无关；本次涉及的 `emoticon_commands.h` / `qm_modes_test.cpp` / `version.h` 单独 `fix_style.py -n` 通过。
- 未执行游戏编译或 C++ 测试（按约定）；F1 输入、bind、分身复制与远端同步尚未实机验证。

## 审查与验证

- 只读审查发现并修正：轮盘鼠标坐标缺少初始化；零长度发射方向先归一化；满池丢弃新发射；输入释放依赖渲染帧；贴墙膨胀无法容纳时重复搜索。最终审查未发现其他阻断项。
- `python qmclient_scripts/gate/check_gate.py --mode quick --report-json-path tmp/emoticon_rewrite_gate.json`：10 项通过，0 警告，0 失败，1 项不适用。
- `git diff --check`：通过。
- 新增 9 个回归测试，覆盖透明孔洞、旋转和缩放、薄墙、高速运动、不同帧率、池满、最新选择、贴墙出生、过期与狭窄空间淡出。测试代码未编译、未运行；HUD 叠放及操作手感尚未实机验证。
- 客户端版本：3.9.3 → 3.9.4。
