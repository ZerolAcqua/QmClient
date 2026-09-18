// 请抬头享受阳光｜日子很好 我很我---------致咩子
#include "qm_realtime.h"

#include <engine/shared/json.h>
#include <engine/shared/protocol.h>

#include <generated/protocol.h>

#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>

namespace
{
	const char *JsonStringField(const json_value *pObject, const char *pName)
	{
		if(pObject == nullptr || pObject->type != json_object || pName == nullptr)
			return nullptr;
		const json_value *pValue = json_object_get(pObject, pName);
		if(pValue == nullptr || pValue->type != json_string)
			return nullptr;
		return pValue->u.string.ptr;
	}

	bool JsonIntField(const json_value *pObject, const char *pName, int64_t &OutValue)
	{
		if(pObject == nullptr || pObject->type != json_object || pName == nullptr)
			return false;
		const json_value *pValue = json_object_get(pObject, pName);
		if(pValue == nullptr || pValue->type != json_integer)
			return false;
		OutValue = pValue->u.integer;
		return true;
	}

	bool JsonBoolField(const json_value *pObject, const char *pName, bool &OutValue)
	{
		if(pObject == nullptr || pObject->type != json_object || pName == nullptr)
			return false;
		const json_value *pValue = json_object_get(pObject, pName);
		if(pValue == nullptr || pValue->type != json_boolean)
			return false;
		OutValue = json_boolean_get(pValue) != 0;
		return true;
	}

	int ClampCount(int64_t Value)
	{
		if(Value < 0)
			return 0;
		if(Value > 1000000)
			return 1000000;
		return (int)Value;
	}
}

bool ParseQmRealtimeMessage(const char *pData, size_t Size, SQmRealtimeMessage &OutMessage)
{
	OutMessage = SQmRealtimeMessage();
	if(pData == nullptr || Size == 0)
		return false;

	json_value *pRoot = json_parse(pData, Size);
	if(pRoot == nullptr)
		return false;
	if(pRoot->type != json_object)
	{
		json_value_free(pRoot);
		return false;
	}

	const char *pType = JsonStringField(pRoot, "type");
	if(pType == nullptr || pType[0] == '\0')
	{
		json_value_free(pRoot);
		return false;
	}
	OutMessage.m_Type = pType;

	if(str_comp(pType, "ping") == 0)
		OutMessage.m_Event = EQmRealtimeEvent::PING;
	else if(str_comp(pType, "pong") == 0)
		OutMessage.m_Event = EQmRealtimeEvent::PONG;
	else if(str_comp(pType, "state") == 0)
	{
		OutMessage.m_Event = EQmRealtimeEvent::STATE;
		const json_value *pDataField = json_object_get(pRoot, "data");
		if(pDataField != nullptr && pDataField->type == json_object)
		{
			OutMessage.m_StatePayloadValid = true;
			int64_t Value = 0;
			if(JsonIntField(pDataField, "online_users", Value))
			{
				OutMessage.m_HasOnlineUsers = true;
				OutMessage.m_OnlineUsers = ClampCount(Value);
			}
			if(JsonIntField(pDataField, "online_dummies", Value))
			{
				OutMessage.m_HasOnlineDummies = true;
				OutMessage.m_OnlineDummies = ClampCount(Value);
			}
		}
	}
	else if(str_comp(pType, "broadcast") == 0)
	{
		OutMessage.m_Event = EQmRealtimeEvent::BROADCAST;
		const json_value *pDataField = json_object_get(pRoot, "data");
		const char *pMarkdown = JsonStringField(pDataField, "markdown");
		if(pMarkdown != nullptr)
		{
			OutMessage.m_HasBroadcast = true;
			OutMessage.m_BroadcastMarkdown = pMarkdown;
			int64_t Version = 0;
			if(JsonIntField(pDataField, "version", Version))
				OutMessage.m_BroadcastVersion = ClampCount(Version);
		}
	}
	else if(str_comp(pType, "titles") == 0)
	{
		OutMessage.m_Event = EQmRealtimeEvent::TITLES;
		// 与头衔 HTTP 接口同构：负载可以在 data 里，也可以直接放在消息顶层。
		const json_value *pDataField = json_object_get(pRoot, "data");
		const bool DataIsObject = pDataField != nullptr && pDataField->type == json_object;
		// 保留原树直到消息应用结束；payload 可以指向 data，但必须释放完整根对象。
		const json_value *pPayload = DataIsObject ? pDataField : pRoot;
		// 无效快照不能冒充空名单，避免清掉尚未过期的称号；合法空数组仍是权威结果。
		int64_t ServerTime = 0;
		OutMessage.m_HasTitles = json_object_get(pPayload, "presences")->type == json_array &&
					 JsonIntField(pPayload, "server_time", ServerTime) && ServerTime > 0;
		OutMessage.m_pTitlePayload = std::shared_ptr<const json_value>(pPayload, [pRoot](const json_value *) { json_value_free(pRoot); });
		OutMessage.m_pPayload = OutMessage.m_pTitlePayload;
		OutMessage.m_HasRealtimeData = DataIsObject;
		return true;
	}
	else if(str_comp(pType, "emoticon") == 0)
	{
		OutMessage.m_Event = EQmRealtimeEvent::EMOTICON;
		const json_value *pDataField = json_object_get(pRoot, "data");
		if(pDataField == nullptr || pDataField->type != json_object)
		{
			json_value_free(pRoot);
			return true;
		}

		int64_t Value = 0;
		const char *pClientId = JsonStringField(pDataField, "client_id");
		const char *pPlayerName = JsonStringField(pDataField, "player_name");
		const char *pServerAddress = JsonStringField(pDataField, "server_address");
		bool LaunchMode = false;
		bool SuperLaunch = false;
		if(pClientId == nullptr || pPlayerName == nullptr || pServerAddress == nullptr ||
			!JsonIntField(pDataField, "emoticon", Value) || Value < 0 || Value >= NUM_EMOTICONS)
		{
			json_value_free(pRoot);
			return true;
		}
		OutMessage.m_Emoticon = (int)Value;
		if(!JsonIntField(pDataField, "player_id", Value) || Value < 0 || Value >= MAX_CLIENTS)
		{
			json_value_free(pRoot);
			return true;
		}
		OutMessage.m_PlayerId = (int)Value;
		if(JsonBoolField(pDataField, "launch_mode", LaunchMode))
			OutMessage.m_LaunchMode = LaunchMode;
		if(JsonBoolField(pDataField, "super_launch", SuperLaunch))
			OutMessage.m_SuperLaunch = SuperLaunch;
		if(!OutMessage.m_LaunchMode && !OutMessage.m_SuperLaunch)
		{
			json_value_free(pRoot);
			return true;
		}
		OutMessage.m_EmoticonClientId = pClientId;
		OutMessage.m_EmoticonPlayerName = pPlayerName;
		OutMessage.m_EmoticonServerAddress = pServerAddress;
		if(JsonIntField(pDataField, "sequence", Value) && Value >= 0)
			OutMessage.m_EmoticonSequence = (uint64_t)Value;
		OutMessage.m_HasEmoticon = true;
		OutMessage.m_HasRealtimeData = true;
		OutMessage.m_pPayload = std::shared_ptr<const json_value>(pDataField, [pRoot](const json_value *) { json_value_free(pRoot); });
		return true;
	}
	else
	{
		OutMessage.m_Event = EQmRealtimeEvent::UNKNOWN;
		for(const auto &Entry : {
			    std::pair<const char *, EQmRealtimeEvent>{"users", EQmRealtimeEvent::USERS},
			    {"developers", EQmRealtimeEvent::DEVELOPERS},
			    {"sponsors", EQmRealtimeEvent::SPONSORS},
			    {"playtime", EQmRealtimeEvent::PLAYTIME},
			    {"time", EQmRealtimeEvent::TIME},
			    {"title_profile", EQmRealtimeEvent::TITLE_PROFILE},
			    {"title_status", EQmRealtimeEvent::TITLE_STATUS},
			    {"error", EQmRealtimeEvent::ERROR}})
		{
			if(str_comp(pType, Entry.first) == 0)
			{
				OutMessage.m_Event = Entry.second;
				break;
			}
		}
		const json_value *pPayload = json_object_get(pRoot, "data");
		if(OutMessage.m_Event != EQmRealtimeEvent::UNKNOWN && pPayload && pPayload->type == json_object)
		{
			OutMessage.m_pPayload = std::shared_ptr<const json_value>(pPayload, [pRoot](const json_value *) { json_value_free(pRoot); });
			OutMessage.m_HasRealtimeData = true;
			return true;
		}
	}

	json_value_free(pRoot);
	return true;
}

const char *QmRealtimeEffectiveUrl(const char *pConfiguredUrl)
{
	return pConfiguredUrl && pConfiguredUrl[0] ? pConfiguredUrl : "wss://qmclient.icu/ws";
}

bool QmRealtimeAllowsCredentials(const char *pUrl)
{
	return pUrl && str_comp(pUrl, "wss://qmclient.icu/ws") == 0;
}
