// 请抬头享受阳光｜日子很好 我很我---------致咩子
#include "qm_realtime.h"

#include <engine/shared/json.h>

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
		OutMessage.m_HasTitles = DataIsObject || json_object_get(pRoot, "presences") != nullptr;
		// 保留原树直到消息应用结束；payload 可以指向 data，但必须释放完整根对象。
		const json_value *pPayload = DataIsObject ? pDataField : pRoot;
		OutMessage.m_pTitlePayload = std::shared_ptr<const json_value>(pPayload, [pRoot](const json_value *) { json_value_free(pRoot); });
		OutMessage.m_pPayload = OutMessage.m_pTitlePayload;
		OutMessage.m_HasRealtimeData = DataIsObject;
		return true;
	}
	else
	{
		OutMessage.m_Event = EQmRealtimeEvent::UNKNOWN;
		for(const auto &Entry : {
			    std::pair<const char *, EQmRealtimeEvent>{"users", EQmRealtimeEvent::USERS},
			    {"developers", EQmRealtimeEvent::DEVELOPERS},
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
