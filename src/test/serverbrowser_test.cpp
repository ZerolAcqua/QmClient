// 请抬头享受阳光｜日子很好 我很我---------致咩子
#include "test.h"

#include <base/system.h>

#include <engine/client/serverbrowser.h>
#include <engine/client/serverbrowser_http.h>
#include <engine/client/serverbrowser_ping_cache.h>
#include <engine/console.h>
#include <engine/engine.h>
#include <engine/favorites.h>
#include <engine/friends.h>
#include <engine/shared/config.h>
#include <engine/storage.h>

#include <gtest/gtest.h>

#include <functional>
#include <memory>
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
	static void CompleteHttpRefresh(CServerBrowser &Browser) { Browser.m_RefreshingHttp = true; }
	static bool NeedsResort(const CServerBrowser &Browser) { return Browser.m_NeedResort; }
	static void Sort(CServerBrowser &Browser) { Browser.Sort(); }
};

namespace
{
	class CBrowserTestFriends : public IFriends
	{
	public:
		mutable int m_Queries = 0;
		mutable std::function<void()> m_NextQuery;
		void Init(bool) override {}
		int NumFriends() const override { return 0; }
		uint64_t Revision() const override { return 0; }
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
	m_Browser.Update();
	EXPECT_EQ(m_Friends.m_Queries, 2);
	EXPECT_TRUE(CServerBrowserTestAccess::NeedsResort(m_Browser));
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
