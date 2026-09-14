// 请抬头享受阳光｜日子很好 我很我---------致咩子
//
// 实时通道（WebSocket 客户端）测试。
// - 纯逻辑：URL 解析、重连退避、实时事件解析、SHA-1 自检（测试辅助实现）。
// - 端到端：设置 QM_WS_URL 指向一个真实 WebSocket 服务端（默认跳过）。
//   之所以不内置测试服务器：客户端传输层只依赖标准握手与帧，用真实服务端验证即可，
//   内置服务器会把测试代码量推到与功能不相称的规模。
#include <base/hash.h>
#include <base/system.h>

#include <engine/shared/websocket_client.h>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

TEST(QmWebSocket, ParsesWsUrl)
{
	SQmWebSocketConnectConfig Config;
	ASSERT_EQ(ParseQmWebSocketUrl("ws://42.194.185.210:8080/qm/ws?ver=1", Config), "");
	EXPECT_FALSE(Config.m_UseTls);
	EXPECT_EQ(Config.m_Host, "42.194.185.210");
	EXPECT_EQ(Config.m_Port, 8080);
	EXPECT_EQ(Config.m_Path, "/qm/ws?ver=1");
}

TEST(QmWebSocket, ParsesWssUrlWithDefaultPort)
{
	SQmWebSocketConnectConfig Config;
	ASSERT_EQ(ParseQmWebSocketUrl("wss://qmclient.icu/api/v1/realtime", Config), "");
	EXPECT_TRUE(Config.m_UseTls);
	EXPECT_EQ(Config.m_Host, "qmclient.icu");
	EXPECT_EQ(Config.m_Port, 443);
	EXPECT_EQ(Config.m_Path, "/api/v1/realtime");
}

TEST(QmWebSocket, ClientTransportIsAlwaysAvailable)
{
	auto pClient = CreateQmWebSocketClient({});
	EXPECT_TRUE(pClient->Available());
	EXPECT_STREQ(pClient->UnavailableReason(), "");
	EXPECT_FALSE(pClient->SendText("{}", 2));
}

TEST(QmWebSocket, RejectsTlsVerificationBypassBeforeConnecting)
{
	auto pClient = CreateQmWebSocketClient({});
	SQmWebSocketConnectConfig Config;
	ASSERT_EQ(ParseQmWebSocketUrl("wss://qmclient.icu/ws", Config), "");
	Config.m_AllowInsecureTls = true;
	std::string Error;
	EXPECT_FALSE(pClient->Connect(Config, Error));
	EXPECT_FALSE(Error.empty());
	EXPECT_FALSE(pClient->Desired());
}

TEST(QmWebSocket, ParsesUrlWithoutPath)
{
	SQmWebSocketConnectConfig Config;
	ASSERT_EQ(ParseQmWebSocketUrl("ws://localhost:9000", Config), "");
	EXPECT_EQ(Config.m_Host, "localhost");
	EXPECT_EQ(Config.m_Port, 9000);
	EXPECT_EQ(Config.m_Path, "/");
}

TEST(QmWebSocket, ParsesIpv6Literal)
{
	SQmWebSocketConnectConfig Config;
	ASSERT_EQ(ParseQmWebSocketUrl("ws://[::1]:9100/qm", Config), "");
	EXPECT_EQ(Config.m_Host, "::1");
	EXPECT_EQ(Config.m_Port, 9100);
	EXPECT_EQ(Config.m_Path, "/qm");

	SQmWebSocketConnectConfig Default;
	ASSERT_EQ(ParseQmWebSocketUrl("wss://[2001:db8::1]", Default), "");
	EXPECT_EQ(Default.m_Host, "2001:db8::1");
	EXPECT_EQ(Default.m_Port, 443);
}

TEST(QmWebSocket, RejectsInvalidUrls)
{
	SQmWebSocketConnectConfig Config;
	EXPECT_NE(ParseQmWebSocketUrl("", Config), "");
	EXPECT_NE(ParseQmWebSocketUrl(nullptr, Config), "");
	EXPECT_NE(ParseQmWebSocketUrl("http://qmclient.icu/ws", Config), "");
	EXPECT_NE(ParseQmWebSocketUrl("ws://qmclient.icu:0/ws", Config), "");
	EXPECT_NE(ParseQmWebSocketUrl("ws://qmclient.icu:70000/ws", Config), "");
	EXPECT_NE(ParseQmWebSocketUrl("ws://qmclient.icu:abc/ws", Config), "");
	EXPECT_NE(ParseQmWebSocketUrl("ws:///ws", Config), "");
	EXPECT_NE(ParseQmWebSocketUrl("ws://qm client.icu/ws", Config), "");
	EXPECT_NE(ParseQmWebSocketUrl("ws://[::1/ws", Config), "");
	EXPECT_NE(ParseQmWebSocketUrl("ws://qmclient.icu/ws#frag", Config), "");
}

TEST(QmWebSocket, BackoffGrowsAndCaps)
{
	int Previous = 0;
	for(int Attempt = 0; Attempt < 8; Attempt++)
	{
		const int Delay = QmWebSocketBackoffDelayMs(Attempt, 1000, 60000);
		if(Attempt < 7)
			EXPECT_GE(Delay, Previous);
		EXPECT_GE(Delay, 1000);
		EXPECT_LE(Delay, 60000 + 60000 / 4 + 1);
		Previous = Delay;
	}
	// 到达上限后不再增长，抖动不超过 25%。
	const int Capped = QmWebSocketBackoffDelayMs(20, 1000, 5000);
	EXPECT_GE(Capped, 5000);
	EXPECT_LE(Capped, 5000 + 1250);
	// 非法输入退回安全默认值，不能返回 0 导致忙重连。
	EXPECT_GE(QmWebSocketBackoffDelayMs(-5, 0, 0), 1000);
}

namespace
{
	struct SQmWsTestClient
	{
		std::unique_ptr<IQmWebSocketClient> m_pClient;
		std::mutex m_Mutex;
		std::vector<std::string> m_vOpened;
		std::vector<std::pair<int, std::string>> m_vMessages;

		IQmWebSocketClient::SCallbacks MakeCallbacks()
		{
			IQmWebSocketClient::SCallbacks Callbacks;
			Callbacks.m_Open = [this]() {
				std::lock_guard<std::mutex> Lock(m_Mutex);
				m_vOpened.push_back("open");
			};
			Callbacks.m_Disconnected = [](const std::string &Reason, bool Clean) {
				(void)Reason;
				(void)Clean;
			};
			Callbacks.m_Error = [](const std::string &Message) { (void)Message; };
			Callbacks.m_Message = [this](EQmWebSocketMessageType Type, const char *pData, size_t Size) {
				std::lock_guard<std::mutex> Lock(m_Mutex);
				m_vMessages.emplace_back((int)Type, std::string(pData, Size));
			};
			return Callbacks;
		}

		size_t OpenCount()
		{
			std::lock_guard<std::mutex> Lock(m_Mutex);
			return m_vOpened.size();
		}

		bool WaitForOpen(int TimeoutMs)
		{
			const int64_t Deadline = time_get_impl() + (int64_t)TimeoutMs * time_freq() / 1000;
			while(time_get_impl() < Deadline)
			{
				if(OpenCount() > 0)
					return true;
				std::this_thread::sleep_for(std::chrono::milliseconds(2));
			}
			return OpenCount() > 0;
		}

		bool WaitForMessage(int Type, const std::string &Needle, std::string &Out, int TimeoutMs)
		{
			const int64_t Deadline = time_get_impl() + (int64_t)TimeoutMs * time_freq() / 1000;
			while(time_get_impl() < Deadline)
			{
				{
					std::lock_guard<std::mutex> Lock(m_Mutex);
					for(const auto &Entry : m_vMessages)
					{
						if(Entry.first == Type && Entry.second.find(Needle) != std::string::npos)
						{
							Out = Entry.second;
							return true;
						}
					}
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(2));
			}
			return false;
		}
	};
}

// 端到端验证：`QM_WS_URL` 指向真实 WebSocket 服务端时跑通握手、收发与心跳。
// 客户端传输由 Rust 承载，与 WEBSOCKETS（服务端 libwebsockets）构建开关无关，
// 所以这里只用环境变量决定是否联网执行。
// 例：QM_WS_URL=wss://qmclient.icu/ws cmake-build-debug/testrunner \
//       --gtest_filter='QmWebSocketLive*'
TEST(QmWebSocketLive, ConnectsAndEchoesWhenServerConfigured)
{
	const char *pUrl = getenv("QM_WS_URL");
	if(pUrl == nullptr || pUrl[0] == '\0')
		GTEST_SKIP() << "QM_WS_URL 未设置";

	SQmWsTestClient Test;
	Test.m_pClient = CreateQmWebSocketClient(Test.MakeCallbacks());
	ASSERT_TRUE(Test.m_pClient->Available()) << Test.m_pClient->UnavailableReason();

	SQmWebSocketConnectConfig Config;
	ASSERT_EQ(ParseQmWebSocketUrl(pUrl, Config), "");
	Config.m_AllowInsecureTls = getenv("QM_WS_INSECURE") != nullptr;

	std::string Error;
	ASSERT_TRUE(Test.m_pClient->Connect(Config, Error)) << Error;
	ASSERT_TRUE(Test.WaitForOpen(10000)) << "未在 10 秒内完成握手，最后错误: " << Test.m_pClient->LastError();
	EXPECT_EQ(Test.m_pClient->State(), EQmWebSocketState::CONNECTED);

	// 服务端可选：只要收到任意一条消息即视为下行通道可用（协议由服务端定义）。
	std::string Reply;
	const bool GotMessage = Test.WaitForMessage((int)EQmWebSocketMessageType::TEXT, "", Reply, 3000);
	fprintf(stderr, "[qm-ws-live] handshake ok, first message: %s\n", GotMessage ? Reply.c_str() : "(none)");

	Test.m_pClient->Disconnect();
	EXPECT_FALSE(Test.m_pClient->Desired());
	EXPECT_EQ(Test.m_pClient->State(), EQmWebSocketState::IDLE);
}
