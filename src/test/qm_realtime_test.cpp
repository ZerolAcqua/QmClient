#include <engine/shared/json.h>

#include <game/client/components/qmclient/qm_realtime.h>

#include <gtest/gtest.h>

#include <cstring>
#include <utility>

namespace
{
	bool Parse(const char *pJson, SQmRealtimeMessage &Message)
	{
		return ParseQmRealtimeMessage(pJson, strlen(pJson), Message);
	}
}

TEST(QmRealtime, ParsesPingAndPong)
{
	SQmRealtimeMessage Message;
	ASSERT_TRUE(Parse("{\"type\":\"ping\"}", Message));
	EXPECT_EQ(Message.m_Event, EQmRealtimeEvent::PING);
	EXPECT_EQ(Message.m_Type, "ping");

	ASSERT_TRUE(Parse("{\"type\":\"pong\",\"t\":123}", Message));
	EXPECT_EQ(Message.m_Event, EQmRealtimeEvent::PONG);
}

TEST(QmRealtime, ParsesStateFieldsIndependently)
{
	SQmRealtimeMessage Message;
	ASSERT_TRUE(Parse("{\"type\":\"state\",\"data\":{\"online_users\":12,\"online_dummies\":3}}", Message));
	EXPECT_EQ(Message.m_Event, EQmRealtimeEvent::STATE);
	EXPECT_TRUE(Message.m_StatePayloadValid);
	EXPECT_TRUE(Message.m_HasOnlineUsers);
	EXPECT_EQ(Message.m_OnlineUsers, 12);
	EXPECT_TRUE(Message.m_HasOnlineDummies);
	EXPECT_EQ(Message.m_OnlineDummies, 3);
}

TEST(QmRealtime, PartialStateDoesNotZeroMissingFields)
{
	// 只下发 dummies 时，users 必须标记为「未下发」，避免被当成 0 覆盖。
	SQmRealtimeMessage Message;
	ASSERT_TRUE(Parse("{\"type\":\"state\",\"data\":{\"online_dummies\":7}}", Message));
	EXPECT_EQ(Message.m_Event, EQmRealtimeEvent::STATE);
	EXPECT_TRUE(Message.m_StatePayloadValid);
	EXPECT_FALSE(Message.m_HasOnlineUsers);
	EXPECT_TRUE(Message.m_HasOnlineDummies);
	EXPECT_EQ(Message.m_OnlineDummies, 7);

	// data 不是对象：事件仍然有效，但没有任何字段被下发。
	ASSERT_TRUE(Parse("{\"type\":\"state\",\"data\":\"nope\"}", Message));
	EXPECT_EQ(Message.m_Event, EQmRealtimeEvent::STATE);
	EXPECT_FALSE(Message.m_StatePayloadValid);
	EXPECT_FALSE(Message.m_HasOnlineUsers);
	EXPECT_FALSE(Message.m_HasOnlineDummies);
}

TEST(QmRealtime, ParsesBroadcastMarkdown)
{
	SQmRealtimeMessage Message;
	ASSERT_TRUE(Parse("{\"type\":\"broadcast\",\"v\":1,\"data\":{\"markdown\":\"# 新功能\\n- 实时通道\",\"version\":9}}", Message));
	EXPECT_EQ(Message.m_Event, EQmRealtimeEvent::BROADCAST);
	EXPECT_TRUE(Message.m_HasBroadcast);
	EXPECT_EQ(Message.m_BroadcastMarkdown, "# 新功能\n- 实时通道");
	EXPECT_EQ(Message.m_BroadcastVersion, 9);

	// markdown 缺失时不上报广播内容，调用方应保持原状态。
	ASSERT_TRUE(Parse("{\"type\":\"broadcast\",\"data\":{\"version\":2}}", Message));
	EXPECT_EQ(Message.m_Event, EQmRealtimeEvent::BROADCAST);
	EXPECT_FALSE(Message.m_HasBroadcast);
}

TEST(QmRealtime, UnknownEventsAreIgnoredNotFatal)
{
	SQmRealtimeMessage Message;
	ASSERT_TRUE(Parse("{\"type\":\"future_event\",\"data\":{\"x\":1}}", Message));
	EXPECT_EQ(Message.m_Event, EQmRealtimeEvent::UNKNOWN);
	EXPECT_EQ(Message.m_Type, "future_event");
}

TEST(QmRealtime, RejectsNonProtocolPayloads)
{
	SQmRealtimeMessage Message;
	EXPECT_FALSE(ParseQmRealtimeMessage(nullptr, 0, Message));
	EXPECT_FALSE(Parse("", Message));
	EXPECT_FALSE(Parse("not json", Message));
	EXPECT_FALSE(Parse("[1,2,3]", Message));
	EXPECT_FALSE(Parse("{\"data\":{}}", Message));
	EXPECT_FALSE(Parse("{\"type\":5}", Message));
	EXPECT_EQ(Message.m_Event, EQmRealtimeEvent::INVALID);
}

TEST(QmRealtime, ParsesDedicatedServiceSnapshotsWithoutHttpTasks)
{
	for(const auto &Entry : {
		    std::pair<const char *, EQmRealtimeEvent>{"users", EQmRealtimeEvent::USERS},
		    {"developers", EQmRealtimeEvent::DEVELOPERS},
		    {"playtime", EQmRealtimeEvent::PLAYTIME},
		    {"time", EQmRealtimeEvent::TIME},
		    {"title_profile", EQmRealtimeEvent::TITLE_PROFILE}})
	{
		SQmRealtimeMessage Message;
		std::string Input = std::string("{\"type\":\"") + Entry.first + "\",\"data\":{\"server_address\":\"example:8303\",\"server_time\":123}}";
		ASSERT_TRUE(Parse(Input.c_str(), Message));
		EXPECT_EQ(Message.m_Event, Entry.second);
		ASSERT_NE(Message.m_pPayload, nullptr);
		Input.clear();
		EXPECT_EQ(json_object_get(Message.m_pPayload.get(), "server_time")->u.integer, 123);
	}
}

TEST(QmRealtime, DedicatedEndpointDefaultsAndCredentialBoundary)
{
	EXPECT_STREQ(QmRealtimeEffectiveUrl(""), "wss://qmclient.icu/ws");
	EXPECT_TRUE(QmRealtimeAllowsCredentials("wss://qmclient.icu/ws"));
	EXPECT_FALSE(QmRealtimeAllowsCredentials("ws://qmclient.icu/ws"));
	EXPECT_FALSE(QmRealtimeAllowsCredentials("wss://qmclient.icu.attacker.example/ws"));
	EXPECT_FALSE(QmRealtimeAllowsCredentials("wss://example.com/ws"));
}

TEST(QmRealtime, TitlesKeepParsedPayloadForBothWireLayouts)
{
	for(const char *pJson : {
		    "{\"type\":\"titles\",\"data\":{\"server_time\":123,\"presences\":[]}}",
		    "{\"type\":\"titles\",\"server_time\":123,\"presences\":[]}"})
	{
		SQmRealtimeMessage Message;
		ASSERT_TRUE(Parse(pJson, Message));
		EXPECT_EQ(Message.m_Event, EQmRealtimeEvent::TITLES);
		EXPECT_TRUE(Message.m_HasTitles);
		ASSERT_NE(Message.m_pTitlePayload, nullptr);
		const json_value *pServerTime = json_object_get(Message.m_pTitlePayload.get(), "server_time");
		ASSERT_EQ(pServerTime->type, json_integer);
		EXPECT_EQ(pServerTime->u.integer, 123);
		EXPECT_EQ(json_object_get(Message.m_pTitlePayload.get(), "presences")->type, json_array);
	}
}

TEST(QmRealtime, TitlesPayloadOwnershipSurvivesInputAndMessageReuse)
{
	SQmRealtimeMessage Message;
	{
		std::string Input = "{\"type\":\"titles\",\"data\":{\"presences\":[],\"server_time\":456}}";
		ASSERT_TRUE(Parse(Input.c_str(), Message));
		Input.assign(Input.size(), 'x');
	}
	SQmRealtimeMessage Copy = Message;
	std::weak_ptr<const json_value> Payload = Message.m_pTitlePayload;
	ASSERT_TRUE(Parse("{\"type\":\"ping\"}", Message));
	EXPECT_EQ(Message.m_pTitlePayload, nullptr);
	ASSERT_FALSE(Payload.expired());
	EXPECT_EQ(json_object_get(Copy.m_pTitlePayload.get(), "server_time")->u.integer, 456);
	Copy = SQmRealtimeMessage();
	EXPECT_TRUE(Payload.expired());
	ASSERT_TRUE(Parse("{\"type\":\"titles\",\"data\":{}}", Message));
	EXPECT_FALSE(Parse("bad json", Message));
	EXPECT_EQ(Message.m_pTitlePayload, nullptr);
}
