// 请抬头享受阳光｜日子很好 我很我---------致咩子
#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_QM_REALTIME_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_QM_REALTIME_H

#include "qmclient_utils.h"

#include <memory>
#include <string>

typedef struct _json_value json_value;

// QmClient 实时通道（WebSocket）的消息协议。
//
// 线上格式：一条 WebSocket 文本消息 = 一个 JSON 对象，
//   {"type": "<事件名>", "data": <负载>, "v": <协议版本>}
// 未知事件名会被忽略（记为 UNKNOWN），保证服务端加事件不会让老客户端出错。
//
// 服务端 -> 客户端：
//   ping      -> 客户端回 pong
//   pong      -> 用于测量往返时延
//   state     -> data 为在线状态增量，如 {"online_users":12,"online_dummies":3}
//   broadcast -> data 为 {"markdown":"...","version":N}，用于「新功能」弹窗
//   titles    -> data 与头衔 HTTP 接口同一结构（{"server_time":N,"presences":[...]}），
//                用于替代客户端每 5 秒一次的头衔 HTTP 轮询
//
// 客户端 -> 服务端：
//   hello    （连接后订阅）、server（切换服务器）、pong
enum class EQmRealtimeEvent
{
	INVALID = 0,
	UNKNOWN,
	PING,
	PONG,
	STATE,
	BROADCAST,
	TITLES,
	USERS,
	DEVELOPERS,
	PLAYTIME,
	TIME,
	TITLE_PROFILE,
	TITLE_STATUS,
	EMOTICON,
	ERROR,
};

struct SQmRealtimeMessage
{
	EQmRealtimeEvent m_Event = EQmRealtimeEvent::INVALID;
	// 原始 type 字符串，仅用于日志与排查。
	std::string m_Type;
	// state 事件的字段是否存在，避免把「没下发」误当成 0。
	bool m_HasOnlineUsers = false;
	bool m_HasOnlineDummies = false;
	int m_OnlineUsers = 0;
	int m_OnlineDummies = 0;
	// 只要 state 的 data 是 JSON 对象就为 true，即便里面没有已知字段。
	bool m_StatePayloadValid = false;
	// broadcast 事件的内容。
	bool m_HasBroadcast = false;
	std::string m_BroadcastMarkdown;
	int m_BroadcastVersion = 0;
	// titles 负载保留首次解析的 JSON 树，应用称号时直接复用。
	bool m_HasTitles = false;
	// 指向根对象或 data 子对象，所有权同时覆盖完整 JSON 树。
	std::shared_ptr<const json_value> m_pTitlePayload;
	std::shared_ptr<const json_value> m_pPayload;
	// data 字段存在且是 JSON 对象。
	bool m_HasRealtimeData = false;
	bool m_HasEmoticon = false;
	int m_Emoticon = -1;
	int m_PlayerId = -1;
	bool m_LaunchMode = false;
	bool m_SuperLaunch = false;
	uint64_t m_EmoticonSequence = 0;
	std::string m_EmoticonClientId;
	std::string m_EmoticonPlayerName;
	std::string m_EmoticonServerAddress;
};

// 解析一条实时通道文本消息。返回 false 表示消息无法解析为协议事件（调用方应忽略）。
bool ParseQmRealtimeMessage(const char *pData, size_t Size, SQmRealtimeMessage &OutMessage);

// 空的旧配置自动使用专用入口；认证凭据只发给固定的加密中心服。
const char *QmRealtimeEffectiveUrl(const char *pConfiguredUrl);
bool QmRealtimeAllowsCredentials(const char *pUrl);

#endif // GAME_CLIENT_COMPONENTS_QMCLIENT_QM_REALTIME_H
