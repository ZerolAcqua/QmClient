// 请抬头享受阳光｜日子很好 我很我---------致咩子
#include "test.h"

#include <base/system.h>

#include <engine/client/friends.h>
#include <engine/client/serverbrowser.h>
#include <engine/client/serverbrowser_http.h>
#include <engine/client/serverbrowser_http_parse.h>
#include <engine/client/serverbrowser_ping_cache.h>
#include <engine/console.h>
#include <engine/engine.h>
#include <engine/favorites.h>
#include <engine/friends.h>
#include <engine/shared/config.h>
#include <engine/shared/json.h>
#include <engine/storage.h>

#include <game/client/components/qmclient/browser_friend_list.h>

#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

class CServerBrowserTestAccess
{
public:
	static void Initialize(CServerBrowser &Browser, IFriends *pFriends, IFavorites *pFavorites, IServerBrowserHttp *pHttp, IServerBrowserPingCache *pPingCache)
	{
		Browser.m_pFriends = pFriends;
		Browser.m_pFavorites = pFavorites;
		Browser.m_pHttp = pHttp;
		Browser.m_pPingCache = pPingCache;
		Browser.m_ServerlistType = IServerBrowser::TYPE_INTERNET;
	}
	static void SetFirstInfo(CServerBrowser &Browser, const CServerInfo &Info) { Browser.SetInfo(Browser.m_vpServerlist[0], Info); }
	static void SetFriends(CServerBrowser &Browser, IFriends *pFriends) { Browser.m_pFriends = pFriends; }
	static void CompleteHttpRefresh(CServerBrowser &Browser) { Browser.m_RefreshingHttp = true; }
	static bool NeedsResort(const CServerBrowser &Browser) { return Browser.m_NeedResort; }
	static void Sort(CServerBrowser &Browser) { Browser.Sort(); }
	static void CleanUp(CServerBrowser &Browser) { Browser.CleanUp(); }
	static void ReplaceFirstAddress(CServerBrowser &Browser, const NETADDR &Address)
	{
		Browser.ReplaceEntry(Browser.m_vpServerlist[0], &Address, 1);
	}
};

namespace
{
	class CBrowserTestFriends : public IFriends
	{
	public:
		mutable int m_Queries = 0;
		mutable uint64_t m_Revision = 0;
		mutable std::function<void()> m_NextQuery;
		void Init(bool) override {}
		int NumFriends() const override { return 0; }
		uint64_t Revision() const override { return m_Revision; }
		const CFriendInfo *GetFriend(int) const override { return nullptr; }
		int GetFriendState(const char *, const char *) const override
		{
			++m_Queries;
			auto Callback = std::move(m_NextQuery);
			m_NextQuery = {};
			if(Callback)
				Callback();
			return FRIEND_NO;
		}
		bool IsFriend(const char *, const char *, bool) const override { return false; }
		const char *GetFriendCategory(const char *, const char *) const override { return ""; }
		const char *GetFriendNote(const char *, const char *) const override { return ""; }
		bool SetFriendNote(const char *, const char *, const char *) override { return false; }
		bool ClearFriendNote(const char *, const char *) override { return false; }
		const char *DefaultCategory() const override { return DEFAULT_CATEGORY; }
		int NumCategories() const override { return 0; }
		const char *GetCategory(int) const override { return ""; }
		int FindCategory(const char *) const override { return -1; }
		bool AddCategory(const char *) override { return false; }
		bool MoveCategory(int, int) override { return false; }
		bool RenameCategory(const char *, const char *) override { return false; }
		bool RemoveCategory(const char *) override { return false; }
		bool SetFriendCategory(const char *, const char *, const char *) override { return false; }
		void AddFriend(const char *, const char *, const char *) override {}
		void RemoveFriend(const char *, const char *) override {}
	};

	class CBrowserTestFavorites : public IFavorites
	{
		void OnConfigSave(IConfigManager *) override {}

	public:
		TRISTATE m_Favorite = TRISTATE::NONE;
		TRISTATE m_AllowPing = TRISTATE::NONE;
		TRISTATE IsFavorite(const NETADDR *, int) const override { return m_Favorite; }
		TRISTATE IsPingAllowed(const NETADDR *, int) const override { return m_AllowPing; }
		void Add(const NETADDR *, int) override {}
		void AllowPing(const NETADDR *, int, bool) override {}
		void Remove(const NETADDR *, int) override {}
		void AllEntries(const CEntry **ppEntries, int *pNumEntries) override
		{
			*ppEntries = nullptr;
			*pNumEntries = 0;
		}
	};

	class CBrowserTestHttp : public IServerBrowserHttp
	{
	public:
		std::vector<CServerInfo> m_vServers;
		bool m_Refreshing = false;
		void Update() override {}
		bool IsRefreshing() const override { return m_Refreshing; }
		bool IsError() const override { return false; }
		void Refresh() override { m_Refreshing = true; }
		bool GetBestUrl(const char **ppBestUrl) const override
		{
			*ppBestUrl = nullptr;
			return true;
		}
		int NumServers() const override { return m_vServers.size(); }
		const CServerInfo &Server(int Index) const override { return m_vServers[Index]; }
	};

	class CBrowserTestPingCache : public IServerBrowserPingCache
	{
	public:
		void Load() override {}
		int NumEntries() const override { return 0; }
		void CachePing(const NETADDR &, int) override {}
		int GetPing(const NETADDR *, int) const override { return 30; }
	};

	class CServerBrowserFilterTest : public ::testing::Test
	{
	protected:
		std::unique_ptr<CConfig> m_pSavedConfig;
		CBrowserTestFriends m_Friends;
		CBrowserTestFavorites m_Favorites;
		CServerBrowser m_Browser;
		CBrowserTestHttp *m_pHttp = nullptr;

		void SetUp() override
		{
			m_pSavedConfig = std::make_unique<CConfig>(g_Config);
			g_Config.m_BrFilterEmpty = g_Config.m_BrFilterFull = g_Config.m_BrFilterPw = 0;
			g_Config.m_BrFilterCountry = g_Config.m_BrFilterFriends = g_Config.m_BrFilterSpectators = 0;
			g_Config.m_BrFilterUnfinishedMap = g_Config.m_BrFilterLogin = g_Config.m_BrFilterConnectingPlayers = 0;
			g_Config.m_BrFilterServerAddress[0] = g_Config.m_BrFilterGametype[0] = '\0';
			g_Config.m_BrFilterString[0] = g_Config.m_BrExcludeString[0] = '\0';
			g_Config.m_BrSort = IServerBrowser::SORT_NAME;
			g_Config.m_BrSortOrder = 0;
			str_copy(g_Config.m_BrLocation, "auto");
			m_pHttp = new CBrowserTestHttp;
			CServerBrowserTestAccess::Initialize(m_Browser, &m_Friends, &m_Favorites, m_pHttp, new CBrowserTestPingCache);
		}
		void TearDown() override { g_Config = *m_pSavedConfig; }

		void AddHttpServer(const char *pName, const char *pMap = "Map", const char *pPlayer = "Player", const char *pClan = "Clan")
		{
			CServerInfo Info{};
			Info.m_NumAddresses = 1;
			ASSERT_FALSE(net_addr_from_str(&Info.m_aAddresses[0], "127.0.0.1:8303"));
			Info.m_aAddresses[0].port += m_pHttp->m_vServers.size();
			str_copy(Info.m_aName, pName);
			str_copy(Info.m_aMap, pMap);
			str_copy(Info.m_aGameType, "DDRace");
			Info.m_NumClients = Info.m_NumPlayers = Info.m_NumReceivedClients = 1;
			Info.m_MaxClients = Info.m_MaxPlayers = 16;
			str_copy(Info.m_aClients[0].m_aName, pPlayer);
			str_copy(Info.m_aClients[0].m_aClan, pClan);
			Info.m_aClients[0].m_Player = true;
			m_pHttp->m_vServers.push_back(Info);
		}
		void FinishHttp()
		{
			CServerBrowserTestAccess::CompleteHttpRefresh(m_Browser);
			m_Browser.Update();
		}
	};
}

TEST_F(CServerBrowserFilterTest, CompletedHttpListDoesNotSortAgainWithoutANewRequest)
{
	m_Favorites.m_Favorite = TRISTATE::ALL;
	m_Favorites.m_AllowPing = TRISTATE::SOME;
	AddHttpServer("Alpha");
	FinishHttp();
	ASSERT_EQ(m_Friends.m_Queries, 1);
	EXPECT_FALSE(CServerBrowserTestAccess::NeedsResort(m_Browser));
	ASSERT_EQ(m_Browser.NumSortedServers(), 1);
	EXPECT_EQ(m_Browser.SortedGet(0)->m_Favorite, TRISTATE::ALL);
	EXPECT_EQ(m_Browser.SortedGet(0)->m_FavoriteAllowPing, TRISTATE::SOME);
	m_Browser.Update();
	EXPECT_EQ(m_Friends.m_Queries, 1);
	ASSERT_EQ(m_Browser.NumSortedServers(), 1);
	EXPECT_STREQ(m_Browser.SortedGet(0)->m_aName, "Alpha");
}

TEST_F(CServerBrowserFilterTest, CachedHttpListConsumesItsImmediateSortRequest)
{
	m_Favorites.m_Favorite = TRISTATE::SOME;
	m_Favorites.m_AllowPing = TRISTATE::NONE;
	AddHttpServer("Alpha");
	m_Browser.Refresh(IServerBrowser::TYPE_INTERNET, true);
	ASSERT_EQ(m_Friends.m_Queries, 1);
	EXPECT_FALSE(CServerBrowserTestAccess::NeedsResort(m_Browser));
	ASSERT_EQ(m_Browser.NumSortedServers(), 1);
	EXPECT_EQ(m_Browser.SortedGet(0)->m_Favorite, TRISTATE::SOME);
	EXPECT_EQ(m_Browser.SortedGet(0)->m_FavoriteAllowPing, TRISTATE::NONE);
	m_Browser.Update();
	EXPECT_EQ(m_Friends.m_Queries, 1);
}

TEST_F(CServerBrowserFilterTest, RequestRaisedWhileSortingRemainsPendingForTheNextUpdate)
{
	AddHttpServer("Alpha");
	FinishHttp();
	m_Browser.RequestResort();
	m_Friends.m_NextQuery = [this] { m_Browser.RequestResort(); };
	// 友状态缓存（d297a1eb0）按 Revision 失效：递增版本号才能让下一次 Sort 重新查询好友，
	// 从而在排序过程中真的产生新的 resort 请求。
	m_Friends.m_Revision = 1;
	m_Browser.Update();
	EXPECT_EQ(m_Friends.m_Queries, 2);
	EXPECT_TRUE(CServerBrowserTestAccess::NeedsResort(m_Browser));
	m_Friends.m_Revision = 2;
	m_Browser.Update();
	EXPECT_EQ(m_Friends.m_Queries, 3);
	EXPECT_FALSE(CServerBrowserTestAccess::NeedsResort(m_Browser));
}

TEST_F(CServerBrowserFilterTest, SearchTokensPreserveUtf8QuotesPlayerHitsExclusionsAndOrder)
{
	AddHttpServer("Gamma", "Race", "(connecting)", "");
	AddHttpServer("beta", "红色地图", "bob", "Équipe");
	AddHttpServer("Alpha", "Novice", "Alice", "Clan");
	FinishHttp();
	struct SCase
	{
		const char *m_pSearch;
		const char *m_pExclude;
		bool m_HideConnecting;
		std::vector<std::pair<const char *, int>> m_vExpected;
	};
	const SCase aCases[] = {
		{" ALPHA ; 红色 ; équipe ", "", false, {{"Alpha", IServerBrowser::QUICK_SERVERNAME}, {"beta", IServerBrowser::QUICK_MAPNAME | IServerBrowser::QUICK_PLAYER}}},
		{"　\"Alpha\"　; \"beta\"", "", false, {{"Alpha", IServerBrowser::QUICK_SERVERNAME}, {"beta", IServerBrowser::QUICK_SERVERNAME}}},
		{"\"alpha\"", "", false, {}},
		{"alice", "", false, {{"Alpha", IServerBrowser::QUICK_PLAYER}}},
		{"connecting", "", false, {{"Gamma", IServerBrowser::QUICK_PLAYER}}},
		{"connecting", "", true, {}},
		{"alpha;beta;gamma", " novice ; 红色 ", false, {{"Gamma", IServerBrowser::QUICK_SERVERNAME}}},
		{"alpha;beta", "alice", false, {{"Alpha", IServerBrowser::QUICK_SERVERNAME}, {"beta", IServerBrowser::QUICK_SERVERNAME}}},
		{"alpha;beta", " \"Alpha\" ", false, {{"beta", IServerBrowser::QUICK_SERVERNAME}}},
		{"alpha", "\"ALPHA\"", false, {{"Alpha", IServerBrowser::QUICK_SERVERNAME}}},
		{"alpha;beta", " DDRACE ", false, {}},
		{" ;　; ", "", false, {}},
		{"\"\"", "", false, {{"Gamma", IServerBrowser::QUICK_PLAYER}}},
		{"\"", "", false, {{"Gamma", IServerBrowser::QUICK_PLAYER}}},
	};
	for(const SCase &Case : aCases)
	{
		SCOPED_TRACE(Case.m_pSearch);
		SCOPED_TRACE(Case.m_pExclude);
		str_copy(g_Config.m_BrFilterString, Case.m_pSearch);
		str_copy(g_Config.m_BrExcludeString, Case.m_pExclude);
		g_Config.m_BrFilterConnectingPlayers = Case.m_HideConnecting;
		CServerBrowserTestAccess::Sort(m_Browser);
		ASSERT_EQ(m_Browser.NumSortedServers(), (int)Case.m_vExpected.size());
		for(size_t Index = 0; Index < Case.m_vExpected.size(); ++Index)
		{
			EXPECT_STREQ(m_Browser.SortedGet(Index)->m_aName, Case.m_vExpected[Index].first);
			EXPECT_EQ(m_Browser.SortedGet(Index)->m_QuickSearchHit, Case.m_vExpected[Index].second);
		}
	}
}

TEST_F(CServerBrowserFilterTest, SearchPreservesTheInputOrderOfEqualSortKeys)
{
	AddHttpServer("Same", "First");
	AddHttpServer("Same", "Second");
	str_copy(g_Config.m_BrFilterString, " same ");
	FinishHttp();
	ASSERT_EQ(m_Browser.NumSortedServers(), 2);
	EXPECT_STREQ(m_Browser.SortedGet(0)->m_aMap, "First");
	EXPECT_STREQ(m_Browser.SortedGet(1)->m_aMap, "Second");
}

// 意图：「梦」列排序键来自游戏层推送的在线分布（address → 人数），
// 默认人数多的在前，再点一次表头变人数少的在前；没有梦客户端的服务器计 0 排在后面。
TEST_F(CServerBrowserFilterTest, QmClientCountColumnSortsByPushedDistribution)
{
	AddHttpServer("Alpha");
	AddHttpServer("Beta");
	AddHttpServer("Gamma");
	FinishHttp();
	ASSERT_EQ(m_Browser.NumSortedServers(), 3);

	std::unordered_map<std::string, int> Counts;
	Counts["127.0.0.1:8304"] = 5; // Beta
	Counts["127.0.0.1:8305"] = 2; // Gamma，Alpha 不在分布里
	m_Browser.SetQmClientServerCounts(Counts);

	g_Config.m_BrSort = IServerBrowser::SORT_QM_CLIENTS;
	g_Config.m_BrSortOrder = 0;
	m_Browser.Update();
	ASSERT_EQ(m_Browser.NumSortedServers(), 3);
	EXPECT_STREQ(m_Browser.SortedGet(0)->m_aName, "Beta");
	EXPECT_STREQ(m_Browser.SortedGet(1)->m_aName, "Gamma");
	EXPECT_STREQ(m_Browser.SortedGet(2)->m_aName, "Alpha");
	EXPECT_EQ(m_Browser.SortedGet(0)->m_QmClientCount, 5);
	EXPECT_EQ(m_Browser.SortedGet(2)->m_QmClientCount, 0);

	g_Config.m_BrSortOrder = 1;
	m_Browser.Update();
	ASSERT_EQ(m_Browser.NumSortedServers(), 3);
	EXPECT_STREQ(m_Browser.SortedGet(0)->m_aName, "Alpha");
	EXPECT_STREQ(m_Browser.SortedGet(1)->m_aName, "Gamma");
	EXPECT_STREQ(m_Browser.SortedGet(2)->m_aName, "Beta");
}

// 意图：服务器列表重载（CleanUp + 重新填充）会把计数清零，
// 新条目从最近一次推送的分布取值，避免刷新后整列按 0 排。
TEST_F(CServerBrowserFilterTest, QmClientCountsRestoreAfterServerListReload)
{
	AddHttpServer("Alpha");
	AddHttpServer("Beta");
	FinishHttp();

	std::unordered_map<std::string, int> Counts;
	Counts["127.0.0.1:8304"] = 3; // Beta
	m_Browser.SetQmClientServerCounts(Counts);
	g_Config.m_BrSort = IServerBrowser::SORT_QM_CLIENTS;
	g_Config.m_BrSortOrder = 0;
	m_Browser.Update();
	ASSERT_EQ(m_Browser.NumSortedServers(), 2);
	ASSERT_STREQ(m_Browser.SortedGet(0)->m_aName, "Beta");

	CServerBrowserTestAccess::CleanUp(m_Browser);
	FinishHttp();
	ASSERT_EQ(m_Browser.NumSortedServers(), 2);
	EXPECT_STREQ(m_Browser.SortedGet(0)->m_aName, "Beta");
	EXPECT_EQ(m_Browser.SortedGet(0)->m_QmClientCount, 3);
	EXPECT_EQ(m_Browser.SortedGet(1)->m_QmClientCount, 0);
}

TEST(ServerBrowser, PingCache)
{
	CTestInfo Info;
	Info.m_DeleteTestStorageFilesOnSuccess = true;

	auto pConsole = CreateConsole(CFGFLAG_CLIENT);
	std::unique_ptr<IStorage> pStorage = Info.CreateTestStorage();
	ASSERT_NE(pStorage, nullptr) << "Error creating test storage";
	auto pPingCache = std::unique_ptr<IServerBrowserPingCache>(CreateServerBrowserPingCache(pConsole.get(), pStorage.get()));

	NETADDR Localhost4, Localhost6, OtherLocalhost4, OtherLocalhost6;
	ASSERT_FALSE(net_addr_from_str(&Localhost4, "127.0.0.1:8303"));
	ASSERT_FALSE(net_addr_from_str(&Localhost6, "[::1]:8304"));
	ASSERT_FALSE(net_addr_from_str(&OtherLocalhost4, "127.0.0.1:8305"));
	ASSERT_FALSE(net_addr_from_str(&OtherLocalhost6, "[::1]:8306"));
	EXPECT_LT(net_addr_comp(&Localhost4, &Localhost6), 0);
	NETADDR aLocalhostBoth[2] = {Localhost4, Localhost6};

	EXPECT_EQ(pPingCache->NumEntries(), 0);
	EXPECT_EQ(pPingCache->GetPing(&Localhost4, 1), -1);
	EXPECT_EQ(pPingCache->GetPing(&Localhost6, 1), -1);
	EXPECT_EQ(pPingCache->GetPing(aLocalhostBoth, 2), -1);
	EXPECT_EQ(pPingCache->GetPing(&OtherLocalhost4, 1), -1);
	EXPECT_EQ(pPingCache->GetPing(&OtherLocalhost6, 1), -1);

	pPingCache->Load();

	EXPECT_EQ(pPingCache->NumEntries(), 0);
	EXPECT_EQ(pPingCache->GetPing(&Localhost4, 1), -1);
	EXPECT_EQ(pPingCache->GetPing(&Localhost6, 1), -1);
	EXPECT_EQ(pPingCache->GetPing(aLocalhostBoth, 2), -1);
	EXPECT_EQ(pPingCache->GetPing(&OtherLocalhost4, 1), -1);
	EXPECT_EQ(pPingCache->GetPing(&OtherLocalhost6, 1), -1);

	// Newer pings overwrite older.
	pPingCache->CachePing(Localhost4, 123);
	pPingCache->CachePing(Localhost4, 234);
	pPingCache->CachePing(Localhost4, 345);
	pPingCache->CachePing(Localhost4, 456);
	pPingCache->CachePing(Localhost4, 567);
	pPingCache->CachePing(Localhost4, 678);
	pPingCache->CachePing(Localhost4, 789);
	pPingCache->CachePing(Localhost4, 890);
	pPingCache->CachePing(Localhost4, 901);
	pPingCache->CachePing(Localhost4, 135);
	pPingCache->CachePing(Localhost4, 246);
	pPingCache->CachePing(Localhost4, 357);
	pPingCache->CachePing(Localhost4, 468);
	pPingCache->CachePing(Localhost4, 579);
	pPingCache->CachePing(Localhost4, 680);
	pPingCache->CachePing(Localhost4, 791);
	pPingCache->CachePing(Localhost4, 802);
	pPingCache->CachePing(Localhost4, 913);

	EXPECT_EQ(pPingCache->NumEntries(), 1);
	EXPECT_EQ(pPingCache->GetPing(&Localhost4, 1), 913);
	EXPECT_EQ(pPingCache->GetPing(&Localhost6, 1), -1);
	EXPECT_EQ(pPingCache->GetPing(aLocalhostBoth, 2), 913);
	EXPECT_EQ(pPingCache->GetPing(&OtherLocalhost4, 1), 913);
	EXPECT_EQ(pPingCache->GetPing(&OtherLocalhost6, 1), -1);

	pPingCache->CachePing(Localhost4, 234);
	pPingCache->CachePing(Localhost6, 345);
	EXPECT_EQ(pPingCache->NumEntries(), 2);
	EXPECT_EQ(pPingCache->GetPing(&Localhost4, 1), 234);
	EXPECT_EQ(pPingCache->GetPing(&Localhost6, 1), 345);
	EXPECT_EQ(pPingCache->GetPing(aLocalhostBoth, 2), 234);
	EXPECT_EQ(pPingCache->GetPing(&OtherLocalhost4, 1), 234);
	EXPECT_EQ(pPingCache->GetPing(&OtherLocalhost6, 1), 345);

	// Port doesn't matter for overwriting.
	pPingCache->CachePing(Localhost4, 1337);
	EXPECT_EQ(pPingCache->NumEntries(), 2);
	EXPECT_EQ(pPingCache->GetPing(&Localhost4, 1), 1337);
	EXPECT_EQ(pPingCache->GetPing(&Localhost6, 1), 345);
	EXPECT_EQ(pPingCache->GetPing(aLocalhostBoth, 2), 345);
	EXPECT_EQ(pPingCache->GetPing(&OtherLocalhost4, 1), 1337);
	EXPECT_EQ(pPingCache->GetPing(&OtherLocalhost6, 1), 345);

	pPingCache.reset(CreateServerBrowserPingCache(pConsole.get(), pStorage.get()));

	// Persistence.
	pPingCache->Load();
	EXPECT_EQ(pPingCache->NumEntries(), 2);
	EXPECT_EQ(pPingCache->GetPing(&Localhost4, 1), 1337);
	EXPECT_EQ(pPingCache->GetPing(&Localhost6, 1), 345);
	EXPECT_EQ(pPingCache->GetPing(aLocalhostBoth, 2), 345);
	EXPECT_EQ(pPingCache->GetPing(&OtherLocalhost4, 1), 1337);
	EXPECT_EQ(pPingCache->GetPing(&OtherLocalhost6, 1), 345);

	// 重复加载必须复位读取语句，周期性缓存刷新不能把 SQLITE_DONE 当作失败。
	pPingCache->Load();
	EXPECT_EQ(pPingCache->NumEntries(), 2);
	EXPECT_EQ(pPingCache->GetPing(&Localhost4, 1), 1337);
	EXPECT_EQ(pPingCache->GetPing(&Localhost6, 1), 345);
}

namespace
{
	std::string HttpListEntry(const char *pAddresses, const char *pName = "Example")
	{
		return std::string("{\"addresses\":") + pAddresses + R"(,"location":"eu","info":{"max_clients":16,"max_players":16,"passworded":false,"game_type":"DDRace","name":")" + pName + R"(","map":{"name":"Map"},"version":"0.6","clients":[]}})";
	}

	bool ParseHttpListForTest(const std::string &Text, std::vector<CServerInfo> &vServers)
	{
		json_value *pJson = JsonParse(Text.c_str(), Text.size());
		const bool Failed = ServerBrowserParseHttpList(pJson, &vServers);
		json_value_free(pJson);
		return Failed;
	}
}

TEST(ServerBrowserHttpParse, PreservesAddressPreferenceAndSkipsUnsupportedServers)
{
	std::vector<CServerInfo> vServers;
	const std::string Text = "{\"servers\":[" +
				 HttpListEntry(R"(["tw-0.7+udp://127.0.0.1:8303","tw-0.6+udp://127.0.0.1:8304"])", "Mixed") + "," +
				 HttpListEntry(R"(["invalid://127.0.0.1:8303"])") + "," +
				 HttpListEntry(R"(["tw-0.7+udp://127.0.0.1:8305"])", "Seven") + "]}";
	ASSERT_FALSE(ParseHttpListForTest(Text, vServers));
	ASSERT_EQ(vServers.size(), 2u);
	EXPECT_STREQ(vServers[0].m_aName, "Mixed");
	EXPECT_EQ(vServers[0].m_NumAddresses, 1);
	EXPECT_EQ(vServers[0].m_aAddresses[0].port, 8304);
	EXPECT_STREQ(vServers[1].m_aName, "Seven");
	EXPECT_EQ(vServers[1].m_aAddresses[0].port, 8305);
}

TEST(ServerBrowserHttpParse, FailureAfterValidEntryPreservesPublishedList)
{
	std::vector<CServerInfo> vServers(1);
	str_copy(vServers[0].m_aName, "Old list");
	for(const std::string &Text : {std::string("not json"), std::string("{}"),
		    "{\"servers\":[" + HttpListEntry(R"(["tw-0.6+udp://127.0.0.1:8303"])") + R"(,{"addresses":false,"info":{}}]})"})
	{
		EXPECT_TRUE(ParseHttpListForTest(Text, vServers));
		ASSERT_EQ(vServers.size(), 1u);
		EXPECT_STREQ(vServers[0].m_aName, "Old list");
	}
}

TEST(ServerBrowserHttpParse, EmptySuccessReplacesOldListAndInvalidInfoIsSkipped)
{
	std::vector<CServerInfo> vServers(1);
	EXPECT_FALSE(ParseHttpListForTest(R"({"servers":[]})", vServers));
	EXPECT_TRUE(vServers.empty());
	const std::string Text = R"({"servers":[{"addresses":["tw-0.6+udp://127.0.0.1:8303"],"info":{}},)" +
				 HttpListEntry(R"(["tw-0.6+udp://127.0.0.1:8304"])") + "]}";
	ASSERT_FALSE(ParseHttpListForTest(Text, vServers));
	ASSERT_EQ(vServers.size(), 1u);
	EXPECT_EQ(vServers[0].m_aAddresses[0].port, 8304);
}

TEST_F(CServerBrowserFilterTest, QmClientCountsStayCurrentWithoutCountSorting)
{
	AddHttpServer("Alpha");
	AddHttpServer("Beta");
	FinishHttp();
	g_Config.m_BrSort = IServerBrowser::SORT_NAME;
	m_Browser.Update();
	m_Browser.SetQmClientServerCounts({{"127.0.0.1:8304", 7}});
	EXPECT_FALSE(CServerBrowserTestAccess::NeedsResort(m_Browser));
	ASSERT_EQ(m_Browser.NumSortedServers(), 2);
	EXPECT_STREQ(m_Browser.SortedGet(0)->m_aName, "Alpha");
	EXPECT_EQ(m_Browser.SortedGet(0)->m_QmClientCount, 0);
	EXPECT_EQ(m_Browser.SortedGet(1)->m_QmClientCount, 7);
	// 刷新来自 HTTP 的信息也必须保留游戏层推送的计数，不能信任输入里的计数。
	m_pHttp->m_vServers[1].m_QmClientCount = 99;
	FinishHttp();
	EXPECT_EQ(m_Browser.SortedGet(1)->m_QmClientCount, 7);
	m_Browser.SetQmClientServerCounts({});
	EXPECT_EQ(m_Browser.SortedGet(1)->m_QmClientCount, 0);
	EXPECT_FALSE(CServerBrowserTestAccess::NeedsResort(m_Browser));
}

TEST_F(CServerBrowserFilterTest, QmClientCountsPushedBeforeHttpListAppearOnFirstPublish)
{
	g_Config.m_BrSort = IServerBrowser::SORT_NAME;
	m_Browser.SetQmClientServerCounts({{"127.0.0.1:8303", 4}});
	AddHttpServer("Alpha");
	FinishHttp();
	ASSERT_EQ(m_Browser.NumSortedServers(), 1);
	EXPECT_EQ(m_Browser.SortedGet(0)->m_QmClientCount, 4);
}

TEST_F(CServerBrowserFilterTest, QmClientCountFollowsReplacedServerAddress)
{
	AddHttpServer("Alpha");
	FinishHttp();
	m_Browser.SetQmClientServerCounts({{"127.0.0.1:8303", 4}, {"127.0.0.1:8304", 9}});
	ASSERT_EQ(m_Browser.Get(0)->m_QmClientCount, 4);
	NETADDR Address;
	ASSERT_FALSE(net_addr_from_str(&Address, "127.0.0.1:8304"));
	CServerBrowserTestAccess::ReplaceFirstAddress(m_Browser, Address);
	EXPECT_EQ(m_Browser.Get(0)->m_QmClientCount, 9);
	Address.port = 8305;
	CServerBrowserTestAccess::ReplaceFirstAddress(m_Browser, Address);
	EXPECT_EQ(m_Browser.Get(0)->m_QmClientCount, 0);
}

TEST_F(CServerBrowserFilterTest, FriendPanelReusesStableRowsAndRefreshesPlayerData)
{
	auto pFriends = std::make_unique<CFriends>();
	CFriends &Friends = *pFriends;
	Friends.AddFriend("Alice", "Clan");
	CServerBrowserTestAccess::SetFriends(m_Browser, &Friends);
	AddHttpServer("Alpha", "Map", "Alice");
	FinishHttp();
	CQmBrowserFriendList List;
	ASSERT_TRUE(List.Update(Friends, m_Browser, false));
	const int Category = Friends.FindCategory(IFriends::DEFAULT_CATEGORY);
	ASSERT_EQ(List.Groups()[Category].size(), 1u);
	const auto *pRows = List.Groups()[Category].data();
	EXPECT_FALSE(List.Update(Friends, m_Browser, false));
	EXPECT_EQ(List.Groups()[Category].data(), pRows);
	EXPECT_TRUE(List.Groups()[Friends.FindCategory(IFriends::OFFLINE_CATEGORY)].empty());

	str_copy(m_pHttp->m_vServers[0].m_aClients[0].m_aSkin, "bluekitty");
	m_pHttp->m_vServers[0].m_aClients[0].m_Afk = true;
	FinishHttp();
	ASSERT_TRUE(List.Update(Friends, m_Browser, false));
	ASSERT_EQ(List.Groups()[Category].size(), 1u);
	EXPECT_STREQ(List.Groups()[Category][0].Skin(), "bluekitty");
	EXPECT_TRUE(List.Groups()[Category][0].IsAfk());
	EXPECT_EQ(List.Groups()[Category][0].ServerInfo(), m_Browser.Get(0));

	CServerBrowserTestAccess::CleanUp(m_Browser);
	ASSERT_TRUE(List.Update(Friends, m_Browser, false));
	EXPECT_TRUE(List.Groups()[Category].empty());
	const auto &Offline = List.Groups()[Friends.FindCategory(IFriends::OFFLINE_CATEGORY)];
	ASSERT_EQ(Offline.size(), 1u);
	EXPECT_EQ(Offline[0].ServerInfo(), nullptr);
}

TEST_F(CServerBrowserFilterTest, FriendPanelTracksCategoryEditsWithoutServerRefresh)
{
	auto pFriends = std::make_unique<CFriends>();
	CFriends &Friends = *pFriends;
	Friends.AddFriend("Alice", "Clan");
	CServerBrowserTestAccess::SetFriends(m_Browser, &Friends);
	AddHttpServer("Alpha", "Map", "Alice");
	FinishHttp();
	CQmBrowserFriendList List;
	ASSERT_TRUE(List.Update(Friends, m_Browser, false));
	ASSERT_TRUE(Friends.AddCategory("Practice"));
	ASSERT_TRUE(Friends.SetFriendCategory("Alice", "Clan", "Practice"));
	ASSERT_TRUE(List.Update(Friends, m_Browser, false));
	ASSERT_EQ(List.Groups()[Friends.FindCategory("Practice")].size(), 1u);
	Friends.AddFriend("Alice", "Clan", IFriends::DEFAULT_CATEGORY);
	ASSERT_TRUE(List.Update(Friends, m_Browser, false));
	EXPECT_EQ(List.Groups()[Friends.FindCategory(IFriends::DEFAULT_CATEGORY)].size(), 1u);
	Friends.AddFriend("Alice", "Clan", IFriends::DEFAULT_CATEGORY);
	EXPECT_FALSE(List.Update(Friends, m_Browser, false));
	ASSERT_TRUE(Friends.SetFriendCategory("Alice", "Clan", "Practice"));
	ASSERT_TRUE(Friends.RenameCategory("Practice", "Race"));
	ASSERT_TRUE(List.Update(Friends, m_Browser, false));
	EXPECT_STREQ(List.Groups()[Friends.FindCategory("Race")][0].Category(), "Race");
	ASSERT_TRUE(Friends.MoveCategory(Friends.FindCategory("Race"), 0));
	ASSERT_TRUE(List.Update(Friends, m_Browser, false));
	ASSERT_EQ(List.Groups()[0].size(), 1u);
	EXPECT_STREQ(List.Groups()[0][0].Name(), "Alice");
	ASSERT_TRUE(Friends.RemoveCategory("Race"));
	ASSERT_TRUE(List.Update(Friends, m_Browser, false));
	EXPECT_EQ(List.Groups()[Friends.FindCategory(IFriends::DEFAULT_CATEGORY)].size(), 1u);
	EXPECT_FALSE(List.Update(Friends, m_Browser, false));
}

TEST_F(CServerBrowserFilterTest, FriendPanelKeepsDuplicateOnlineRowsClanMatchesAndOfflineRules)
{
	g_Config.m_ClFriendsIgnoreClan = 0;
	auto pFriends = std::make_unique<CFriends>();
	CFriends &Friends = *pFriends;
	Friends.AddFriend("Alice", "Clan");
	Friends.AddFriend("Alice", "Other");
	Friends.AddFriend("", "Team");
	Friends.AddFriend("Zoe", "Clan");
	CServerBrowserTestAccess::SetFriends(m_Browser, &Friends);
	AddHttpServer("Alpha", "Map", "Alice");
	AddHttpServer("Beta", "Map", "Alice");
	AddHttpServer("Team", "Map", "Bob", "Team");
	FinishHttp();
	CQmBrowserFriendList List;
	ASSERT_TRUE(List.Update(Friends, m_Browser, false));
	EXPECT_EQ(List.Groups()[Friends.FindCategory(IFriends::DEFAULT_CATEGORY)].size(), 2u);
	EXPECT_EQ(List.Groups()[Friends.FindCategory(IFriends::CLAN_MEMBERS_CATEGORY)].size(), 1u);
	const int Offline = Friends.FindCategory(IFriends::OFFLINE_CATEGORY);
	ASSERT_EQ(List.Groups()[Offline].size(), 2u);
	EXPECT_STREQ(List.Groups()[Offline][0].Name(), "Alice");
	EXPECT_STREQ(List.Groups()[Offline][0].Clan(), "Other");
	EXPECT_STREQ(List.Groups()[Offline][1].Name(), "Zoe");
	g_Config.m_ClFriendsIgnoreClan = 1;
	ASSERT_TRUE(List.Update(Friends, m_Browser, true));
	ASSERT_EQ(List.Groups()[Offline].size(), 1u);
	EXPECT_STREQ(List.Groups()[Offline][0].Name(), "Zoe");
}

TEST_F(CServerBrowserFilterTest, FriendListRevisionChangesOnReloadResortAddressReplacementAndCleanup)
{
	AddHttpServer("Alpha");
	const auto EmptyRevision = m_Browser.FriendListRevision();
	FinishHttp();
	EXPECT_NE(m_Browser.FriendListRevision(), EmptyRevision);
	const auto LoadedRevision = m_Browser.FriendListRevision();
	m_Browser.Update();
	EXPECT_EQ(m_Browser.FriendListRevision(), LoadedRevision);
	m_Browser.RequestResort();
	m_Browser.Update();
	EXPECT_NE(m_Browser.FriendListRevision(), LoadedRevision);
	const auto SortedRevision = m_Browser.FriendListRevision();
	CServerInfo Info = *m_Browser.Get(0);
	str_copy(Info.m_aClients[0].m_aName, "New player");
	CServerBrowserTestAccess::SetFirstInfo(m_Browser, Info);
	EXPECT_NE(m_Browser.FriendListRevision(), SortedRevision);
	const auto InfoRevision = m_Browser.FriendListRevision();
	NETADDR NewAddress;
	ASSERT_FALSE(net_addr_from_str(&NewAddress, "127.0.0.1:9303"));
	CServerBrowserTestAccess::ReplaceFirstAddress(m_Browser, NewAddress);
	EXPECT_NE(m_Browser.FriendListRevision(), InfoRevision);
	const auto AddressRevision = m_Browser.FriendListRevision();
	CServerBrowserTestAccess::CleanUp(m_Browser);
	EXPECT_NE(m_Browser.FriendListRevision(), AddressRevision);
}

TEST_F(CServerBrowserFilterTest, FriendPanelTracksOfflineFriendRemovalAndAddition)
{
	auto pFriends = std::make_unique<CFriends>();
	CQmBrowserFriendList List;
	ASSERT_TRUE(List.Update(*pFriends, m_Browser, false));
	pFriends->AddFriend("Alice", "Clan");
	ASSERT_TRUE(List.Update(*pFriends, m_Browser, false));
	const int Offline = pFriends->FindCategory(IFriends::OFFLINE_CATEGORY);
	ASSERT_EQ(List.Groups()[Offline].size(), 1u);
	pFriends->RemoveFriend("Alice", "Clan");
	ASSERT_TRUE(List.Update(*pFriends, m_Browser, false));
	EXPECT_TRUE(List.Groups()[Offline].empty());
	EXPECT_FALSE(List.Update(*pFriends, m_Browser, false));
}
