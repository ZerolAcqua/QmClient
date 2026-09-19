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

TEST(QmRealtime, SponsorsAreIndependentFromNews)
{
	SQmRealtimeMessage Message;
	ASSERT_TRUE(Parse("{\"type\":\"sponsors\",\"data\":{\"markdown\":\"# 赞助名单\\n- 喵不一\\n- 少女`\",\"version\":4}}", Message));
	EXPECT_EQ(Message.m_Event, EQmRealtimeEvent::SPONSORS);
	ASSERT_TRUE(Message.m_HasRealtimeData);
	ASSERT_NE(Message.m_pPayload, nullptr);
	EXPECT_STREQ(json_string_get(json_object_get(Message.m_pPayload.get(), "markdown")), "# 赞助名单\n- 喵不一\n- 少女`");
	EXPECT_EQ(json_int_get(json_object_get(Message.m_pPayload.get(), "version")), 4);
	EXPECT_FALSE(Message.m_HasBroadcast);

	// 赞助频道数据不完整时，不能冒充空名单，也不能改写新闻内容。
	ASSERT_TRUE(Parse("{\"type\":\"sponsors\",\"data\":null}", Message));
	EXPECT_EQ(Message.m_Event, EQmRealtimeEvent::SPONSORS);
	EXPECT_FALSE(Message.m_HasRealtimeData);
	EXPECT_FALSE(Message.m_HasBroadcast);

	ASSERT_TRUE(Parse("{\"type\":\"broadcast\",\"data\":{\"markdown\":\"更新内容\",\"version\":8}}", Message));
	EXPECT_EQ(Message.m_Event, EQmRealtimeEvent::BROADCAST);
	EXPECT_TRUE(Message.m_HasBroadcast);
	EXPECT_EQ(Message.m_BroadcastMarkdown, "更新内容");
}

TEST(QmRealtime, UnknownEventsAreIgnoredNotFatal)
{
	SQmRealtimeMessage Message;
	ASSERT_TRUE(Parse("{\"type\":\"future_event\",\"data\":{\"x\":1}}", Message));
	EXPECT_EQ(Message.m_Event, EQmRealtimeEvent::UNKNOWN);
	EXPECT_EQ(Message.m_Type, "future_event");
}

TEST(QmRealtime, ParsesAnonymousQmclientJsonEmoticonEvents)
{
	SQmRealtimeMessage Message;
	ASSERT_TRUE(Parse(R"({"type":"emoticon","v":2,"data":{"client_id":"anonymous-client","player_id":3,"player_name":"tester","server_address":"example:8303","emoticon":4,"launch_mode":true,"super_launch":false,"sequence":12}})", Message));
	EXPECT_EQ(Message.m_Event, EQmRealtimeEvent::EMOTICON);
	ASSERT_TRUE(Message.m_HasEmoticon);
	EXPECT_EQ(Message.m_Emoticon, 4);
	EXPECT_EQ(Message.m_PlayerId, 3);
	EXPECT_TRUE(Message.m_LaunchMode);
	EXPECT_FALSE(Message.m_SuperLaunch);
	EXPECT_EQ(Message.m_EmoticonClientId, "anonymous-client");
	EXPECT_EQ(Message.m_EmoticonPlayerName, "tester");
	EXPECT_EQ(Message.m_EmoticonServerAddress, "example:8303");
	EXPECT_EQ(Message.m_EmoticonSequence, 12u);
}

TEST(QmRealtime, RejectsInvalidOrLegacyAnonymousEmoticonPayloads)
{
	SQmRealtimeMessage Message;
	ASSERT_TRUE(Parse(R"({"type":"emoticon","v":2,"data":{"client_id":"anonymous-client","player_id":3,"player_name":"tester","server_address":"example:8303","emoticon":999,"launch_mode":true,"super_launch":false}})", Message));
	EXPECT_FALSE(Message.m_HasEmoticon);

	ASSERT_TRUE(Parse(R"({"type":"event","event":{"type":"emoticon","emoticon":4,"launchMode":true,"superLaunch":false}})", Message));
	EXPECT_EQ(Message.m_Event, EQmRealtimeEvent::UNKNOWN);
	EXPECT_FALSE(Message.m_HasEmoticon);
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

TEST(QmRealtime, InvalidTitleSnapshotsCannotReplaceLivePresences)
{
	// 外层仍是可识别事件，但没有完整名单时不能授权调用方清空已有称号。
	for(const char *pPayload : {
		    R"({})",
		    R"({"server_time":123})",
		    R"({"server_time":123,"presences":null})",
		    R"({"server_time":123,"presences":{}})",
		    R"({"presences":[]})",
		    R"({"server_time":0,"presences":[]})",
		    R"({"server_time":-1,"presences":[]})",
		    R"({"server_time":"123","presences":[]})"})
	{
		for(const bool Nested : {false, true})
		{
			const std::string Payload = pPayload;
			const std::string Input = Nested ? "{\"type\":\"titles\",\"data\":" + Payload + "}" : (Payload == "{}" ? "{\"type\":\"titles\"}" : "{\"type\":\"titles\"," + Payload.substr(1));
			SCOPED_TRACE(Input);
			SQmRealtimeMessage Message;
			ASSERT_TRUE(Parse(R"({"type":"titles","data":{"server_time":123,"presences":[]}})", Message));
			ASSERT_TRUE(Message.m_HasTitles);
			ASSERT_TRUE(Parse(Input.c_str(), Message));
			EXPECT_EQ(Message.m_Event, EQmRealtimeEvent::TITLES);
			EXPECT_FALSE(Message.m_HasTitles);
		}
	}
}

TEST(QmRealtime, ValidTitleSnapshotKeepsLeaseAndStyleForBothWireLayouts)
{
	const std::string Payload = R"({"server_address":"example:8303","server_time":100,"presences":[{"server_address":"example:8303","player_id":2,"player_name":"tester","title":"Sponsor","style":"rainbow","issued_at":95,"expires_at":110}]})";
	for(const std::string &Input : {
		    "{\"type\":\"titles\",\"data\":" + Payload + "}",
		    "{\"type\":\"titles\"," + Payload.substr(1)})
	{
		SQmRealtimeMessage Message;
		ASSERT_TRUE(Parse(Input.c_str(), Message));
		ASSERT_TRUE(Message.m_HasTitles);
		ASSERT_NE(Message.m_pTitlePayload, nullptr);
		int64_t ServerTime = 0;
		const auto vPresences = ParseQmTitlePresences(Message.m_pTitlePayload.get(), "example:8303", &ServerTime);
		ASSERT_EQ(vPresences.size(), 1u);
		EXPECT_EQ(ServerTime, 100);
		EXPECT_EQ(vPresences[0].m_PlayerId, 2);
		EXPECT_EQ(vPresences[0].m_PlayerName, "tester");
		EXPECT_EQ(vPresences[0].m_Title, "Sponsor");
		EXPECT_EQ(vPresences[0].m_Style, "rainbow");
		EXPECT_EQ(vPresences[0].m_RemainingSeconds, 10);
	}
}

TEST(QmRealtime, DistributionKeepsLastSnapshotAfterLeaseExpires)
{
	SQmClientDistributionSnapshot Snapshot;
	EXPECT_TRUE(Snapshot.m_vServers.empty());
	EXPECT_FALSE(Snapshot.IsStale(100));
	SQmClientUsersParseResult Result;
	Result.m_Parsed = true;
	Result.m_vServerDistribution = {{"one:8303", 2, 1}};
	Result.m_OnlineUserCount = 2;
	Result.m_OnlineDummyCount = 1;
	Result.m_vLocalServerMarks.emplace_back().m_Name = "player";
	ASSERT_TRUE(Snapshot.Apply(Result, 120));
	EXPECT_FALSE(Snapshot.IsStale(119));
	EXPECT_TRUE(Snapshot.IsStale(120));
	EXPECT_TRUE(Snapshot.IsStale(3600));
	ASSERT_EQ(Snapshot.m_vServers.size(), 1u);
	EXPECT_EQ(Snapshot.m_vServers[0].m_ServerAddress, "one:8303");
	EXPECT_EQ(Snapshot.m_OnlineUserCount, 2);
	EXPECT_EQ(Snapshot.m_OnlineDummyCount, 1);
	// 列表缓存不消费识别标记，游戏内标记继续使用独立的原租约。
	ASSERT_EQ(Result.m_vLocalServerMarks.size(), 1u);
	EXPECT_EQ(Result.m_vLocalServerMarks[0].m_Name, "player");
}

TEST(QmRealtime, InvalidDistributionDoesNotEraseOrRefreshLastSnapshot)
{
	SQmClientDistributionSnapshot Snapshot;
	SQmClientUsersParseResult Result;
	Result.m_Parsed = true;
	Result.m_vServerDistribution = {{"one:8303", 2, 1}};
	Result.m_OnlineUserCount = 2;
	Result.m_OnlineDummyCount = 1;
	ASSERT_TRUE(Snapshot.Apply(Result, 120));
	SQmClientUsersParseResult Invalid;
	ASSERT_FALSE(Snapshot.Apply(Invalid, 500));
	ASSERT_EQ(Snapshot.m_vServers.size(), 1u);
	EXPECT_EQ(Snapshot.m_OnlineUserCount, 2);
	EXPECT_EQ(Snapshot.m_OnlineDummyCount, 1);
	EXPECT_TRUE(Snapshot.IsStale(120));
}

TEST(QmRealtime, NewDistributionReplacesStaleSnapshotIncludingExplicitEmptyList)
{
	SQmClientDistributionSnapshot Snapshot;
	SQmClientUsersParseResult First;
	First.m_Parsed = true;
	First.m_vServerDistribution = {{"one:8303", 2, 1}};
	First.m_OnlineUserCount = 2;
	First.m_OnlineDummyCount = 1;
	ASSERT_TRUE(Snapshot.Apply(First, 120));
	EXPECT_TRUE(Snapshot.IsStale(150));

	SQmClientUsersParseResult Next;
	Next.m_Parsed = true;
	Next.m_vServerDistribution = {{"two:8303", 1, 0}};
	Next.m_OnlineUserCount = 1;
	ASSERT_TRUE(Snapshot.Apply(Next, 170));
	ASSERT_EQ(Snapshot.m_vServers.size(), 1u);
	EXPECT_EQ(Snapshot.m_vServers[0].m_ServerAddress, "two:8303");
	EXPECT_EQ(Snapshot.m_OnlineUserCount, 1);
	EXPECT_EQ(Snapshot.m_OnlineDummyCount, 0);
	EXPECT_FALSE(Snapshot.IsStale(150));

	SQmRealtimeMessage Message;
	ASSERT_TRUE(Parse(R"({"type":"users","data":{"users":[]}})", Message));
	SQmClientUsersParseResult Empty;
	ASSERT_TRUE(ParseQmClientUsersJson(Message.m_pPayload.get(), "two:8303", Empty));
	ASSERT_TRUE(Snapshot.Apply(Empty, 200));
	EXPECT_TRUE(Snapshot.m_vServers.empty());
	EXPECT_EQ(Snapshot.m_OnlineUserCount, 0);
	EXPECT_EQ(Snapshot.m_OnlineDummyCount, 0);
	EXPECT_FALSE(Snapshot.IsStale(180));
}
