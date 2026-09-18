# 酷狗与 QQ 音乐歌词接入

## 完成范围

本次接入面向 64 位 Windows 桌面版 QmClient，跟随正在运行的酷狗或 QQ 音乐读取当前歌曲、播放进度和原始歌词，继续使用现有歌词岛与歌词开关。两个来源沿用现有 Hook 互斥规则：同一时间只启用一个音乐来源，设置页和启动跟随会自动处理来源切换。

实现不收集 Cookie，也不按歌名搜索歌词。采集器使用应用当前播放对象中的歌曲 ID 或精确 MID，请求官方歌词接口，并在客户端按歌曲身份、进程 ID、歌词 generation 丢弃迟到结果。

## 酷狗音乐

酷狗桌面版通过本机 CDP 页面读取播放信息。兼容版本来自酷狗 20.1.22.27795 附带的 CEF 89.20.0；只有九处完整字节指纹全部匹配时才允许修改 `libcef.dll`，未知版本、升级后的 DLL 和部分修改状态都会拒绝写入。

设置页的“设置酷狗歌词接入”会启动随 QmClient 发布的 `qm-music-helper.exe --kugou-setup`。操作前必须完全退出酷狗，程序会显示目标 DLL 和备份路径并再次询问；原始文件备份为同目录的 `libcef.dll.qmclient-original`，已有备份不会覆盖。成功后需要手动重新启动酷狗。 “恢复酷狗原始文件”执行相同的退出检查、版本比较和确认流程，恢复后仍需手动启动酷狗。写入中断时会尝试回滚，原始备份会保留。

酷狗取词优先请求精确歌曲 hash 的 KRC，必要时回退 LRC；网络请求有 8 秒总时限和 4 MB 响应上限。helper 将解密歌词写入 `%LOCALAPPDATA%\QmClient\kugou-hook` 的临时 JSON 文件，并通过独立共享内存发布快照。应用或采集线程停止响应超过 1.5 秒时会清空旧歌曲和歌词，避免继续显示过期内容。

## QQ 音乐

QQ 音乐使用只读进程权限读取 `QQMusic.exe` 的 x86 播放结构，不注入代码、不修改安装文件。当前支持的 ProductVersion 是：20.05、21.81、22.16、22.22、22.31、22.41、22.52、22.60；未知版本、x64 进程或不匹配的模块会显示不支持并等待重试。

歌词请求按当前 songID 或播放流中的精确 MID 发送到官方接口，QRC 会在 helper 中解密，LRC 与翻译也会一并发布。采集器会等待元数据稳定，切歌的中间空态不会把上一首 ID 交给请求；同一首歌稍后补齐歌手信息不会丢弃正在进行的取词任务。网络请求不带应用 Cookie，单次响应限制为 4 MB，读取总时限为 6 秒并最多重试三次。

## 使用方式

1. 在 QmClient 的 HUD/歌词设置中启用“启用 酷狗音乐 歌词”或“启用 QQ 音乐 歌词”。
2. 同时打开“启用歌词”和“在媒体岛中显示歌词”。
3. 首次使用酷狗时先退出酷狗，点击“设置酷狗歌词接入”，按提示完成确认，再手动启动酷狗。QQ 音乐无需安装文件操作，直接启动支持版本即可。
4. 设置页状态行会显示等待应用、版本不支持、连接成功、歌词加载或网络错误等状态。helper 默认从 QmClient 同目录启动，也可以通过对应的 `qm_kugou_hook_helper_path` 或 `qm_qqmusic_hook_helper_path` 配置覆盖；超时配置默认 1500 毫秒，范围 250–10000 毫秒。

酷狗和 QQ 音乐使用独立的 `qm-music-helper.exe` 采集实例、共享内存名称和本地歌词缓存目录；helper 随 QmClient 的 Windows 64 位目标打包。Linux、macOS、Android 构建不会启动这些 Windows 采集器。

## 代码与测试覆盖

- `src/qm-music-hook/`：共享 helper、快照发布、酷狗 CDP/补丁、QQ 进程读取与歌词请求。
- `music_lyrics_integration`：来源选择、helper 重启、歌词文件异步加载、进程/generation/epoch 防陈旧结果。
- 设置注册、HUD 状态行、酷狗设置/恢复按钮、配置项和 12 种语言翻译已接线。
- 新增 2 个来源注册测试、8 个传输与发布测试、9 个酷狗协议/补丁测试、13 个 QQ 协议测试，覆盖切歌、暂停、跳转、退出、歌词延迟、无歌词和恢复保护等状态。
- 参考实现为 `VTB-LINK/Metabox-Nexus-PlayerCap` 提交 `87415dddd095f6d4e0b3449cec1200744c636631`；相关 MIT 归属已写入根目录 `license.txt`。

## 验证与限制

已执行：

- `git diff --check`（本次音乐接入文件）。
- `python qmclient_scripts/languages_qmclient/extract_strings.py`：完成扫描。
- `python qmclient_scripts/languages_qmclient/generate_all.py`：完成语言产物生成。
- `python qmclient_scripts/languages_qmclient/review_duplicate_entries.py --show-groups 0 --show-unused 0`：未发现重复键、空译文或未使用新增键。
- `python qmclient_scripts/gate/check_gate.py --mode quick`：源码卫生检查，不编译、不运行测试。

本机没有安装酷狗音乐或 QQ 音乐，因此没有真实应用联调、CDP 连接、进程读取和网络歌词回归结果。没有执行游戏编译或 C++/Rust 测试；这些验证留给安装对应应用后的 Windows 64 位联调和提交前门禁。语言增量检查仍报告仓库中既有的表情相关缺失键与 CJK 源键，未混入本次音乐接入。
